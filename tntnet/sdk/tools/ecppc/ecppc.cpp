////////////////////////////////////////////////////////////////////////
// makerequest.cpp
//

#include "ecppGenerator.h"
#include <fstream>
#include <sstream>
#include <cxxtools/arg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define VERSION "1.1"

int main(int argc, char* argv[])
{
  std::ios::sync_with_stdio(false);

  try
  {
    std::string requestname = arg<const char*>(argc, argv, 'n', "").getValue();
    std::string ofile = arg<const char*>(argc, argv, 'o', "").getValue();
    std::string odir = arg<const char*>(argc, argv, 'O', "").getValue();
    arg<const char*> mimetype(argc, argv, 'm', 0);
    arg<bool> binary(argc, argv, 'b');
    arg<bool> singleton(argc, argv, 's');
    arg<const char*> componentclass(argc, argv, 'C', 0);
    arg<const char*> baseclass(argc, argv, 'B', 0);
    arg<bool> htmlcompress(argc, argv, "--compress-html");
    arg<bool> csscompress(argc, argv, "--compress-css");
    arg<bool> jscompress(argc, argv, "--compress-js");
    arg<bool> compress(argc, argv, 'z');
    arg<bool> externData(argc, argv, 'x');
    arg<bool> verbose(argc, argv, 'v');
    arg<bool> debug(argc, argv, 'd');
    arg<bool> splitBar(argc, argv, 'S');

    if (argc != 2 || argv[1][0] == '-')
    {
      std::cerr
        << "Aufruf: " << argv[0] << " {Optionen} Quelldatei\n\n"
           "  -o Dateiname     Ausgabedatei\n"
           "  -n name          Klassename\n"
           "  -m typ           Mimetyp\n"
           "  -s               generiere singleton\n"
           "  -s-              generiere kein singleton\n"
           "  -b               binär\n"
           "  -B klasse        zusätzliche Basisklasse\n"
           "  -C klasse        alternative Basisklasse (muß von ecppComponent abgeleitet sein)\n"
           "  -compress-html   entferne überflüssige Zeichen aus HTML-code\n"
           "  -compress-css    entferne überflüssige Zeichen aus CSS-code\n"
           "  -compress-js     entferne überflüssige Zeichen aus JavaScript-code\n"
           "  -z               komprimiere konstanten Text\n"
           "  -x               speichere konstanten Text extern\n"
           "  -v               verbose\n"
           "  -d               debug\n"
           "  -S               teile Chunks bei '«' und '»'\n"
        << std::endl;
      return -1;
    }
    
    // requestname aus Inputdatei
    if (requestname.empty())
    {
      std::string input = argv[1];
      std::string::size_type pos_dot = input.find_last_of(".");
      std::string::size_type pos_slash = input.find_last_of("\\/");
      if (pos_dot != std::string::npos)
      {
        if (pos_slash == std::string::npos)
          requestname = input.substr(0, pos_dot);
        else if (pos_slash < pos_dot)
          requestname = input.substr(pos_slash + 1, pos_dot - pos_slash - 1);
        else
        {
          std::cerr << "ungewöhnliche Inputdatei, bitte '-n' spezifizieren (siehe '--help')!" << std::endl;
          return -1;
        }
      }
      else
      {
        requestname = input;
      }

      if (requestname.empty())
      {
        std::cerr << "ungewöhnliche Inputdatei, bitte '-n' spezifizieren (siehe '--help')!" << std::endl;
        return -1;
      }

      if (ofile.empty())
      {
        if (pos_slash == std::string::npos)
          ofile = requestname;
        else
          ofile = input.substr(0, pos_slash + 1) + requestname;
      }
    }
    
    // Output auf ".h" and ".cpp" pruefen
    if (ofile.size() == ofile.rfind(".h") + 2)
      ofile = ofile.substr(0, ofile.size() - 2);
    else if (ofile.size() == ofile.rfind(".cpp") + 4)
      ofile = ofile.substr(0, ofile.size() - 4);

    //
    // initialisiere Generator
    //

    ecppGenerator generator;
    generator.setDebug(debug);

    if (mimetype.isSet())
      generator.setMimetype(mimetype.getValue());

    if (htmlcompress)
      generator.setHtmlCompress();
    else if (csscompress)
      generator.setCssCompress();
    else if (jscompress)
      generator.setJsCompress();

    generator.setCompress(compress);
    generator.setExternData(externData);
    generator.setSplitBar(splitBar);

    //
    // parse Quelldatei
    //

    std::ifstream in(argv[1]);
    if (binary)
    {
      std::ostringstream html;
      html << in.rdbuf();
      generator.processHtml(html.str());
      generator.setRawMode();

      struct stat st;
      if (stat(argv[1], &st) == 0)
        generator.setLastModifiedTime(st.st_ctime);
    }
    else
      generator.parse(in);

    // wenn Kommandozeilenparameter -s explizit angegeben ist, übergebe diesen an den
    // Generator
    if (singleton.isSet())
      generator.setSingleton(singleton);
    if (componentclass.isSet())
      generator.setComponentclass(componentclass.getValue());
    if (baseclass.isSet())
      generator.setBaseclass(baseclass.getValue());

    std::string obase = odir;
    if (!obase.empty())
      obase += '/';
    obase += ofile;
    
    //
    // generiere Code
    //

    if (verbose)
      std::cout << "generiere " << obase << ".h" << std::endl;
    std::ofstream hout((obase + ".h").c_str());
    hout << generator.getHeader(ofile + ".h", requestname);
    hout.close();

    if (verbose)
      std::cout << "generiere " << obase << ".cpp" << std::endl;
    std::ofstream sout((obase + ".cpp").c_str());
    sout << generator.getCpp(ofile + ".cpp", requestname);
    sout.close();
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return -1;
  }
}
