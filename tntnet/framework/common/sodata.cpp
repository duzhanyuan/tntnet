////////////////////////////////////////////////////////////////////////
// sodata.cpp
//

#include <tnt/sodata.h>
#include <tnt/log.h>
#include <cxxtools/thread.h>
#include <zlib.h>
#include <tnt/component.h>
#include <cxxtools/dlloader.h>
#include <stdlib.h>

namespace tnt
{
  static Mutex mutex;

  void sodata::setLangSuffix()
  {
    const char* LANG = getenv("LANG");
    std::string sosuffix = std::string(1, '.');
    if (LANG)
      sosuffix += LANG;
    setSoSuffix(sosuffix);
  }

  void sodata::addRef(const compident& ci)
  {
    MutexLock lock(mutex);
    if (refs++ == 0)
    {
      // read data from shared library
      log_debug("load library " << ci.libname << sosuffix);
      dl::library so((ci.libname + sosuffix + ".so").c_str());

      dl::symbol datalen_sym = so.sym((ci.compname + "_datalen").c_str());
      unsigned datalen = *reinterpret_cast<unsigned*>(datalen_sym.getSym());
      try
      {
        // try to read compressed data
        dl::symbol zdata_sym = so.sym((ci.compname + "_zdata").c_str());
        dl::symbol zdatalen_sym = so.sym((ci.compname + "_zdatalen").c_str());

        const char** zdata = reinterpret_cast<const char**>(zdata_sym.getSym());
        unsigned zdatalen = *reinterpret_cast<unsigned*>(zdatalen_sym.getSym());

        log_debug(zdatalen << " bytes compressed data, " << datalen
          << " bytes uncompressed");

        // ... and uncompress it
        data = new char[datalen];
        uLong destlen = datalen;
        int z_ret = uncompress((Bytef*)data, &destlen, (const Bytef*)*zdata, zdatalen);
        if (z_ret != Z_OK)
        {
          throw std::runtime_error(std::string("error uncompressing data: ") +
            (z_ret == Z_MEM_ERROR ? "Z_MEM_ERROR" :
             z_ret == Z_BUF_ERROR ? "Z_BUF_ERROR" :
             z_ret == Z_DATA_ERROR ? "Z_DATA_ERROR" : "unknown error"));
        }

        log_debug("uncompress ready");
      }
      catch(const dl::symbol_not_found&)
      {
        // compressed data not found - try uncompressed
        dl::symbol data_sym = so.sym((ci.compname + "_data").c_str());
        const char** srcdata = reinterpret_cast<const char**>(data_sym.getSym());

        log_debug(datalen << " bytes data");

        // ... and copy it
        data = new char[datalen];
        std::memcpy(data, *srcdata, datalen);
      }
    }
  }

  void sodata::release()
  {
    MutexLock lock(mutex);
    if (--refs <= 0)
    {
      log_debug("release");
      delete[] data;
      data = 0;
    }
  }

}
