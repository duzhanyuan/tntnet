////////////////////////////////////////////////////////////////////////
// http.cpp
//

#include <tnt/http.h>
#include <tnt/log.h>
#include <tnt/contenttype.h>
#include <tnt/multipart.h>
#include <tnt/contentdisposition.h>
#include <list>
#include <algorithm>
#include <arpa/inet.h>
#include <cxxtools/thread.h>

namespace tnt
{

////////////////////////////////////////////////////////////////////////
// httpMessage
//

void httpMessage::clear()
{
  method.clear();
  url.clear();
  query_string.clear();
  header.clear();
  body.clear();
  major_version = 0;
  minor_version = 0;
  content_size = 0;
}

void httpMessage::parse(std::istream& in)
{
  parseStartline(in);
  if (in)
  {
    header.parse(in);
    if (in)
      parseBody(in);
  }
}

std::string httpMessage::getHeader(const std::string& key, const std::string& def) const
{
  header_type::const_iterator i = header.find(key);
  return i == header.end() ? def : i->second;
}

void httpMessage::setHeader(const std::string& key, const std::string& value)
{
  log_debug("httpMessage::setHeader(\"" << key << "\", \"" << value << "\")");
  header.insert(header_type::value_type(key, value));
}

std::string httpMessage::dumpHeader() const
{
  std::ostringstream h;
  dumpHeader(h);
  return h.str();
}

void httpMessage::dumpHeader(std::ostream& out) const
{
  for (header_type::const_iterator it = header.begin();
       it != header.end(); ++it)
    out << it->first << ": " << it->second << '\n';
}

std::string httpMessage::htdate(time_t t)
{
  struct tm tm;
  localtime_r(&t, &tm);
  return htdate(&tm);
}

std::string httpMessage::htdate(struct tm* tm)
{
  const char* wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* monthn[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char buffer[80];

  mktime(tm);
  sprintf(buffer, "%s, %02d %s %d %02d:%02d:%02d GMT",
    wday[tm->tm_wday], tm->tm_mday, monthn[tm->tm_mon], tm->tm_year + 1900,
    tm->tm_hour, tm->tm_min, tm->tm_sec);
  return buffer;
}

void httpMessage::parseStartline(std::istream& in)
{
  enum state_type
  {
    state_cmd0,
    state_cmd,
    state_url0,
    state_url,
    state_qparam,
    state_version,
    state_version_major,
    state_version_minor,
    state_end0,
    state_end
  };

  state_type state = state_cmd0;
  clear();

  std::streambuf* buf = in.rdbuf();

  while (state != state_end
      && buf->sgetc() != std::ios::traits_type::eof())
  {
    char ch = buf->sbumpc();
    switch(state)
    {
      case state_cmd0:
        if (!std::isspace(ch))
        {
          method = ch;
          state = state_cmd;
        }
        break;

      case state_cmd:
        if (std::isspace(ch))
          state = state_url0;
        else
          method += ch;
        break;

      case state_url0:
        if (!std::isspace(ch))
        {
          url = ch;
          state = state_url;
        }
        break;

      case state_url:
        if (ch == '?')
          state = state_qparam;
        else if (std::isspace(ch))
          state = state_end0;
        else
          url += ch;
        break;

      case state_qparam:
        if (std::isspace(ch))
          state = state_version;
        else
          query_string += ch;
        break;

      case state_version:
        if (ch == '/')
          state = state_version_major;
        else if (ch == '\r')
          in.setstate(std::ios::failbit);
        break;

      case state_version_major:
        if (ch == '.')
          state = state_version_minor;
        else if (std::isdigit(ch))
          major_version = major_version * 10 + (ch - '0');
        else
          in.setstate(std::ios::failbit);
        break;

      case state_version_minor:
        if (std::isspace(ch))
          state = state_end0;
        else if (std::isdigit(ch))
          minor_version = minor_version * 10 + (ch - '0');
        else
          in.setstate(std::ios::failbit);
        break;

      case state_end0:
        if (ch == '\n')
          state = state_end;
        break;

      case state_end:
        break;
    }
  }

  if (state != state_end)
    in.setstate(std::ios_base::failbit);

  if (!in)
    log_debug("error reading http-Message s" << state);
}

void httpMessage::parseBody(std::istream& in)
{
  std::string content_length_header = getHeader("Content-Length:");
  if (!content_length_header.empty())
  {
    std::istringstream valuestream(content_length_header);
    valuestream >> content_size;
    if (!valuestream)
      throw httpError("400 missing Content-Length");

    body.clear();
    char buffer[512];
    unsigned size = content_size;
    while (size > 0
      && (in.read(buffer, std::min(sizeof(buffer), size)), in.gcount() > 0))
    {
      body.append(buffer, in.gcount());
      size -= in.gcount();
    }
  }
}

namespace
{
  class pstr
  {
      const char* start;
      const char* end;

    public:
      typedef size_t size_type;

      pstr(const char* s, const char* e)
        : start(s), end(e)
        { }

      size_type size() const  { return end - start; }

      bool operator== (const pstr& s) const
      {
        return size() == s.size()
          && std::equal(start, end, s.start);
      }

      bool operator== (const char* str) const
      {
        size_type i;
        for (i = 0; i < size() && str[i] != '\0'; ++i)
          if (start[i] != str[i])
            return false;
        return i == size() && str[i] == '\0';
      }
  };
}

bool httpMessage::checkUrl(const std::string& url)
{
  typedef std::list<pstr> tokens_type;

  // teile in Komponenten auf
  enum state_type {
    state_start,
    state_token,
    state_end
  };

  state_type state = state_start;

  tokens_type tokens;
  const char* p = url.data();
  const char* e = p + url.size();
  const char* s = p;
  for (; state != state_end && p != e; ++p)
  {
    switch (state)
    {
      case state_start:
        if (*p != '/')
        {
          s = p;
          state = state_token;
        }
        break;

      case state_token:
        if (*p == '/')
        {
          tokens.push_back(pstr(s, p));
          state = state_start;
        }
        else if (p == e)
        {
          tokens.push_back(pstr(s, p));
          state = state_end;
        }
        break;

      case state_end:
        break;
    }
  }

  // entferne jedes .. inklusive der vorhergehenden Komponente
  tokens_type::iterator i = tokens.begin();
  while (i != tokens.end())
  {
    if (*i == "..")
    {
      if (i == tokens.begin())
        return false;
      --i;
      i = tokens.erase(i);  // l�sche 2 Komponenten
      i = tokens.erase(i);
    }
    else
    {
      ++i;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////
// httpRequest
//
unsigned httpRequest::serial_ = 0;

void httpRequest::parse(std::istream& in)
{
  httpMessage::parse(in);
  if (in)
  {
    qparam.parse_url(getQueryString());
    if (getMethod() == "POST")
    {
      std::istringstream in(getHeader("Content-Type:"));
      in >> ct;

      if (in)
      {
        log_debug("Content-Type: " << in.str());
        if (ct.isMultipart())
        {
          log_debug("multipart-boundary=" << ct.getBoundary());
          mp.set(ct.getBoundary(), getBody());
          for (multipart::const_iterator it = mp.begin();
               it != mp.end(); ++it)
          {
            // hochgeladene Dateien nicht in qparam �bernehmen
            if (it->getFilename().empty())
            {
              qparam.add(it->getName(),
                std::string(it->getBodyBegin(), it->getBodyEnd()));
            }
          }
        }
        else
        {
          qparam.parse_url(getBody());
        }
      }
      else
        qparam.parse_url(getBody());
    }

    {
      static Mutex monitor;
      MutexLock lock(monitor);
      serial = ++serial_;
    }
  }
}

std::string httpRequest::getPeerIp() const
{
  static Mutex monitor;
  MutexLock lock(monitor);

  char* p = inet_ntoa(peerAddr.sin_addr);
  return std::string(p);
}

std::string httpRequest::getServerIp() const
{
  static Mutex monitor;
  MutexLock lock(monitor);

  char* p = inet_ntoa(serverAddr.sin_addr);
  return std::string(p);
}

////////////////////////////////////////////////////////////////////////
// httpReply
//

void httpReply::sendReply(unsigned ret)
{
  if (!isDirectMode())
  {
    log_debug("HTTP/1.1 " << ret << " OK");
    socket << "HTTP/1.1 " << ret << " OK\r\n";

    if (header.find("Content-Type") == header.end())
    {
      log_debug("Content-Type: " << contentType);
      socket << "Content-Type: " << contentType << "\r\n";
    }

    if (header.find("Content-Length") == header.end())
    {
      log_debug("Content-Length: " << outstream.str().size());
      socket << "Content-Length: " << outstream.str().size() << "\r\n";
    }

    if (header.find("Server") == header.end())
    {
      log_debug("Server: tntnet");
      socket << "Server: tntnet\r\n";
    }

    for (header_type::const_iterator it = header.begin();
         it != header.end(); ++it)
    {
      log_debug(it->first << ": " << it->second);
      socket << it->first << ": " << it->second << "\r\n";
    }

    socket << "\r\n"
           << outstream.str()
           << std::flush;
  }
  else
    socket.flush();
}

void httpReply::throwError(unsigned errorCode, const std::string& errorMessage) const
{
  throw httpError(errorCode, errorMessage);
}

void httpReply::throwError(const std::string& errorMessage) const
{
  throw httpError(errorMessage);
}

void httpReply::throwNotFound(const std::string& errorMessage) const
{
  throw notFoundException(errorMessage);
}

void httpReply::setDirectMode()
{
  if (!isDirectMode())
  {
    log_debug("HTTP/1.1 200 OK\n"
              "Content-Type: " << contentType);

    socket << "HTTP/1.1 200 OK\r\n"
              "Content-Type: " << contentType << "\r\n";

    for (header_type::const_iterator it = header.begin();
         it != header.end(); ++it)
    {
      log_debug(it->first << ": " << it->second);
      socket << it->first << ": " << it->second << "\r\n";
    }

    socket << "\r\n"
           << outstream.str();

    current_outstream = &socket;
  }
}

void httpReply::setDirectModeNoFlush()
{
  current_outstream = &socket;
}

////////////////////////////////////////////////////////////////////////
// httpError
//

static std::string httpErrorFormat(unsigned errcode, const std::string& msg)
{
  std::string ret;
  ret.reserve(4 + msg.size());
  char d[3];
  d[2] = '0' + errcode % 10;
  errcode /= 10;
  d[1] = '0' + errcode % 10;
  errcode /= 10;
  d[0] = '0' + errcode % 10;
  ret.assign(d, d+3);
  ret += ' ';
  ret += msg;
  return ret;
}

httpError::httpError(unsigned errcode, const std::string& msg)
  : msg(httpErrorFormat(errcode, msg))
{
}

}
