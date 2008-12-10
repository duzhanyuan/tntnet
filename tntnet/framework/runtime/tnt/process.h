/*
 * Copyright (C) 2007 Tommi Maekitalo
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


#ifndef TNT_PROCESS_H
#define TNT_PROCESS_H

#include <cxxtools/pipe.h>
#include <string>

namespace tnt
{
  class Process
  {
      bool daemon;
      std::string dir;
      std::string rootdir;
      std::string user;
      std::string group;
      std::string pidfile;

      bool exitRestart;

      int mkDaemon(cxxtools::Pipe& pipe);
      void runMonitor(cxxtools::Pipe& mainPipe);
      void initWorker();

    protected:
      void setDaemon(bool sw = true)        { daemon = sw; }
      void setDir(const std::string& v)     { dir = v; }
      void setRootdir(const std::string& v) { rootdir = v; }
      void setUser(const std::string& v)    { user = v; }
      void setGroup(const std::string& v)   { group = v; }
      void setPidFile(const std::string& v) { pidfile = v; }

    public:
      explicit Process(bool daemon_ = false);
      ~Process();

      void run();
      void shutdown();
      void restart();

    protected:
      virtual void onInit() = 0;
      virtual void doWork() = 0;
      virtual void doShutdown() = 0;
  };
}

#endif // TNT_PROCESS_H
