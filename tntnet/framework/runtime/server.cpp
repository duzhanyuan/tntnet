////////////////////////////////////////////////////////////////////////
// server.cpp
//

#include "tnt/server.h"
#include "tnt/dispatcher.h"
#include "tnt/job.h"
#include <tnt/http.h>
#include <tnt/log.h>

namespace
{
  class component_unload_lock
  {
      tnt::component& comp;

    public:
      component_unload_lock(tnt::component& c)
        : comp(c)
      { comp.lock(); }
      ~component_unload_lock()
      { comp.unlock(); }
  };
}

namespace tnt
{
  Mutex server::mutex;
  unsigned server::nextThreadNumber = 0;

  server::server(jobqueue& q, const dispatcher& d,
    comploader::load_library_listener* libconfigurator)
    : queue(q),
      ourdispatcher(d),
      threadNumber(++nextThreadNumber)
  {
    log_debug("initialize thread " << threadNumber);
    if (libconfigurator)
      mycomploader.addLoadLibraryListener(libconfigurator);
  }

  void server::Run()
  {
    log_debug("start thread " << threadNumber);
    while(1)
    {
      jobqueue::job_ptr j = queue.get();
      std::iostream& socket = j->getStream();

      try
      {
        try
        {
          httpRequest request;
          socket >> request;

          if (!socket)
            log_error("stream error");
          else
          {
            request.setPeerAddr(j->getPeeraddr_in());
            request.setServerAddr(j->getServeraddr_in());
            request.setSsl(j->isSsl());
            httpReply reply(socket);
            try
            {
              Dispatch(request, reply);
            }
            catch (const dl::dlopen_error& e)
            {
              log_warn("dl::dlopen_error catched");
              throw notFoundException(e.getLibname());
            }
            catch (const dl::symbol_not_found& e)
            {
              log_warn("dl::symbol_not_found catched");
              throw notFoundException(e.getSymbol());
            }
            catch (const std::exception& e)
            {
              throw httpError(HTTP_INTERNAL_SERVER_ERROR, e.what());
            }
          }
        }
        catch (const httpError& e)
        {
          log_warn("http-Error: " << e.what());
          socket << "HTTP/1.0 " << e.what()
                 << "\r\n\r\n"
                 << "<html><body><h1>Error</h1><p>"
                 << e.what() << "</p></body></html>" << std::endl;
        }
      }
      catch (const std::exception& e)
      {
        log_error(e.what());
      }
    }
  }

  void server::Dispatch(httpRequest& request, httpReply& reply)
  {
    const std::string& url = request.getUrl();

    log_info("dispatch " << url);

    if (!httpRequest::checkUrl(url))
      throw httpError(HTTP_BAD_REQUEST, "illegal url");

    dispatcher::pos_type pos(ourdispatcher, request.getUrl());
    while (1)
    {
      dispatcher::compident_type ci = pos.getNext();
      try
      {
        component& comp = mycomploader.fetchComp(ci, ourdispatcher);
        component_unload_lock unload_lock(comp);
        request.setPathInfo(ci.pathinfo);
        request.setArgs(ci.args);
        log_debug("call component " << ci << " path \"" << ci.pathinfo << '"');
        unsigned http_return = comp(request, reply, request.getQueryParams());
        if (http_return != DECLINED)
        {
          if (reply.isDirectMode())
            log_debug("request processed");
          else
            log_debug("request processed - ContentSize: " << reply.getContentSize());

          reply.sendReply(http_return);
          return;
        }
      }
      catch (const notFoundException&)
      {
      }
    }

    throw notFoundException(request.getUrl());
  }

}
