/* tnt/cgi.h
   Copyright (C) 2003-2005 Tommi Maekitalo

This file is part of tntnet.

Tntnet is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Tntnet is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with tntnet; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA  02111-1307  USA
*/

#ifndef TNT_CGI_H
#define TNT_CGI_H

#include <tnt/httprequest.h>
#include <tnt/scopemanager.h>
#include <tnt/tntconfig.h>
#include <tnt/comploader.h>

namespace tnt
{
  /// enable cgi-interface for components
  class Cgi
  {
      std::string componentName;
      Tntconfig config;

      HttpRequest request;
      ScopeManager scopeManager;
      Comploader comploader;

      void getHeader(const char* env, const std::string& headername);
      void getMethod();
      void getQueryString();
      void getPathInfo();
      void getRemoteAddr();
      void readBody();
      void execute();

    public:
      Cgi(int argc, char* argv[]);
      bool isFastCgi() const;
      bool isCgi() const  { return !isFastCgi(); }
      int run();
      int runCgi();
      int runFCgi();
  };
}

#endif // TNT_CGI_H

