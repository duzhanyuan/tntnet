/*
 * Copyright (C) 2005 Tommi Maekitalo
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <tnt/cookie.h>
#include <tnt/httperror.h>
#include <tnt/http.h>
#include <tnt/httpmessage.h>
#include <tnt/urlescostream.h>
#include <cctype>
#include <cxxtools/log.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace tnt
{
  log_define("tntnet.cookie")

  const Cookie Cookies::emptyCookie;

  const std::string Cookie::maxAge  = "Max-Age";
  const std::string Cookie::comment = "Comment";
  const std::string Cookie::domain  = "Domain";
  const std::string Cookie::path    = "Path";
  const std::string Cookie::secure  = "Secure";
  const std::string Cookie::version = "Version";
  const std::string Cookie::expires = "Expires";

  unsigned Cookie::getMaxAge() const
  {
    std::string a = getAttr(maxAge);
    if (!a.empty())
    {
      std::istringstream s(a);
      unsigned ret;
      s >> ret;
      if (s)
        return ret;
    }
    return 0;
  }

  void Cookie::setMaxAge(unsigned value)
  {
    std::ostringstream s;
    s << value;
    setAttr(maxAge, s.str());
  }

  void Cookies::clearCookie(const std::string& name)
  {
    cookies_type::iterator it = data.find(name);
    if (it != data.end())
    {
      it->second.setAttr(Cookie::maxAge, "0");
      it->second.setAttr(Cookie::expires, HttpMessage::htdate(static_cast<time_t>(0)));
    }
    else
    {
      Cookie c;
      c.setAttr(Cookie::maxAge, "0");
      c.setAttr(Cookie::expires, HttpMessage::htdate(static_cast<time_t>(0)));
      setCookie(name, c);
    }
  }

  void Cookies::clearCookie(const std::string& name, const Cookie& c)
  {
    Cookie cc(c);
    cc.setAttr(Cookie::maxAge, "0");
    setCookie(name, cc);
  }

  class CookieParser
  {
      // Cookie: $Version="1"; Customer="WILE_E_COYOTE"; $Path="/acme"
      Cookie::attrs_type common_attrs;
      Cookie::attrs_type* current_attrs;
      Cookie current_cookie;
      bool attr;
      std::string current_cookie_name;

      std::string name;
      std::string value;

      Cookies& mycookies;

      void store_cookie();
      void process_nv();

    public:
      CookieParser(Cookies& c)
        : current_attrs(&common_attrs),
          mycookies(c)
        { }

      void parse(const std::string& header);
  };

  void Cookies::set(const std::string& header)
  {
    CookieParser parser(*this);
    parser.parse(header);
  }

  void CookieParser::store_cookie()
  {
    if (!mycookies.hasCookie(current_cookie_name))
      mycookies.setCookie(current_cookie_name, current_cookie);
    current_cookie.value.clear();
  }

  void CookieParser::process_nv()
  {
    if (attr)
    {
      if (name == Cookie::secure)
      {
        log_debug("attribute: secure");
        current_cookie.secureFlag = true;
      }
      else
      {
        log_debug("attribute: " << name << '=' << value);
        current_attrs->insert(
          Cookie::attrs_type::value_type(name, value));
      }
    }
    else
    {
      if (!current_cookie_name.empty())
        store_cookie();

      log_debug("Cookie: " << name << '=' << value);

      current_cookie_name = name;
      current_cookie.value = value;
      current_cookie.secureFlag = false;
      name.clear();
      current_attrs = &current_cookie.attrs;
      current_cookie.attrs = common_attrs;
    }
  }

  namespace
  {
    void throwInvalidCookie(const std::string& cookie)
    {
      throw HttpError(HTTP_BAD_REQUEST, "invalid cookie: " + cookie);
    }
  }

  void CookieParser::parse(const std::string& header)
  {
    // Cookie: $Version="1"; Customer="WILE_E_COYOTE"; $Path="/acme"

    enum state_type
    {
      state_0,
      state_name,
      state_eq,
      state_value0,
      state_value,
      state_valuee,
      state_qvalue,
      state_qvaluee
    };

    state_type state = state_0;

    for (std::string::const_iterator it = header.begin();
         it != header.end(); ++it)
    {
      char ch = *it;
      switch(state)
      {
        case state_0:
          if (ch == '$')
          {
            attr = true;
            name.clear();
            state = state_name;
          }
          else if (!std::isspace(ch))
          {
            attr = false;
            name = ch;
            state = state_name;
          }
          break;

        case state_name:
          if (std::isspace(ch))
            state = (name == Cookie::secure ? state_valuee : state_eq);
          else if (ch == '=')
          {
            if (name == Cookie::secure)
              state = state_valuee;
            else
            {
              value.clear();
              value.reserve(32);
              state = state_value0;
            }
          }
          else if (ch == ';' && name == Cookie::secure)
            state = state_valuee;
          else
            name += ch;
          break;

        case state_eq:
          if (ch == '=')
          {
            value.clear();
            value.reserve(32);
            state = state_value0;
          }
          else if (!std::isspace(ch))
          {
            log_warn("invalid cookie: " << header << " - '=' expected");
            throwInvalidCookie(header);
          }
          break;

        case state_value0:
          if (ch == '"')
            state = state_qvalue;
          else if (!std::isspace(ch))
          {
            value += ch;
            state = state_value;
          }
          break;

        case state_value:
          if (ch == ';')
          {
            process_nv();
            state = state_0;
          }
          else
            value += ch;
          break;

        case state_valuee:
          if (ch == ';')
          {
            process_nv();
            state = state_0;
          }
          else if (std::isspace(ch))
            state = state_valuee;
          else
          {
            log_warn("invalid cookie: " << header << " - semicolon expected after value");
            throwInvalidCookie(header);
          }
          break;

        case state_qvalue:
          if (ch == '"')
            state = state_qvaluee;
          else
            value += ch;
          break;

        case state_qvaluee:
          if (ch == ';')
          {
            process_nv();
            state = state_0;
          }
          else if (!std::isspace(ch))
          {
            log_warn("invalid cookie: " << header << " - semicolon expected");
            throwInvalidCookie(header);
          }
          break;
      }
    }

    if (state == state_qvaluee || state == state_value || state == state_value0)
      process_nv();
    else if (state != state_0)
    {
      log_warn("invalid cookie: " << header << " - invalid state " << state);
      throwInvalidCookie(header);
    }

    if (!current_cookie.value.empty())
      store_cookie();
  }

  std::ostream& operator<< (std::ostream& out, const Cookies& c)
  {
    // Set-Cookie: Customer="WILE_E_COYOTE"; Version="1"; Path="/acme"

    bool first = true;
    for (Cookies::cookies_type::const_iterator it = c.data.begin();
         it != c.data.end(); ++it)
    {
      if (first)
        first = false;
      else
        out << ' ';

      const Cookie& cookie = it->second;

      // print name (Customer="WILE_E_COYOTE")
      out << it->first << '=';
      UrlEscOstream u(out);
      u << cookie.getValue();

      // print secure-attribute
      if (cookie.secureFlag)
        out << "; " << Cookie::secure;

      // print attributes
      for (Cookie::attrs_type::const_iterator a = cookie.attrs.begin();
           a != cookie.attrs.end(); ++a)
        out << "; " << a->first << '=' << a->second;
      if (cookie.attrs.find(Cookie::version) == cookie.attrs.end())
        out << "; Version=1";
    }

    return out;
  }
}
