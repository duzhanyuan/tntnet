////////////////////////////////////////////////////////////////////////
// dispatcher.cpp
//

#include "tnt/dispatcher.h"
#include <tnt/http.h>
#include <functional>
#include <iterator>

namespace tnt
{

void dispatcher::addUrlMapEntry(const std::string& url, const compident_type& ci)
{
  WrLock lock(rwlock);

  urlmap.push_back(urlmap_type::value_type(boost::regex(url), ci));
}

compident dispatcher::mapComp(const std::string& compUrl) const
{
  urlmap_type::const_iterator pos = urlmap.begin();
  return mapCompNext(compUrl, pos);
}

namespace {
  class regmatch_formatter : public std::unary_function<const std::string&, std::string>
  {
    public:
      boost::smatch what;
      std::string operator() (const std::string& s) const
      { return boost::regex_format(what, s); }
  };
}

dispatcher::compident_type dispatcher::mapCompNext(const std::string& compUrl,
  dispatcher::urlmap_type::const_iterator& pos) const
{
  using std::transform;
  using std::back_inserter;

  regmatch_formatter formatter;
  nextcomp_pair_type ret;

  for (; pos != urlmap.end(); ++pos)
  {
    if (boost::regex_match(compUrl, formatter.what, pos->first))
    {
      const compident_type& src = pos->second;

      compident_type ci;
      ci.libname = formatter(src.libname);
      ci.compname = formatter(src.compname);
      ci.pathinfo = formatter(src.pathinfo);
      transform(src.args.begin(), src.args.end(), back_inserter(ci.args), formatter);

      return ci;
    }
  }

  throw notFoundException(compUrl);
}

dispatcher::compident_type dispatcher::pos_type::getNext()
{
  if (first)
    first = false;
  else
    ++pos;

  return dis.mapCompNext(url, pos);
}

}
