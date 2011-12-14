#include "CommandLine.h"
#include <cstring>
#include <cstdlib>
namespace hemelb
{
  namespace configuration
  {
    CommandLine::CommandLine(int aargc, const char * const * const aargv) :
        inputFile("input.xml"), outputDir(""), snapshotsPerCycle(10), imagesPerCycle(10), steeringSessionId(1), argc(aargc), argv(aargv), ok(false)
    {

      // Initialise the network discovery. If this fails, abort.
      // Needs to go first, because need to know if am the IO process for printing usage.

      bool topologySuccess = true;
      // MPI C doesn't provide const-correct interface, so cast away the const on argv.
      hemelb::topology::NetworkTopology::Instance()->Init(argc,
                                                          const_cast<char**>(argv),
                                                          &topologySuccess);

      if (!topologySuccess)
      {
        hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Couldn't get machine information for this network topology. Aborting.\n");
        PrintUsage();
        return;
      }

      // There should be an odd number of arguments since the parameters occur in pairs.
      if ( (argc % 2) == 0)
      {
        PrintUsage();
        return;
      }

      // All arguments are parsed in pairs, one is a "-<paramName>" type, and one
      // is the <parametervalue>.
      for (int ii = 1; ii < argc; ii += 2)
      {
        const char* const paramName = argv[ii];
        const char* const paramValue = argv[ii + 1];
        if (std::strcmp(paramName, "-in") == 0)
        {
          inputFile = std::string(paramValue);
        }
        else if (std::strcmp(paramName, "-out") == 0)
        {
          outputDir = std::string(paramValue);
        }
        else if (std::strcmp(paramName, "-s") == 0)
        {
          char * dummy;
          snapshotsPerCycle = (unsigned int) (strtoul(paramValue, &dummy, 10));
        }
        else if (std::strcmp(paramName, "-i") == 0)
        {
          char *dummy;
          imagesPerCycle = (unsigned int) (strtoul(paramValue, &dummy, 10));
        }
        else if (std::strcmp(paramName, "-ss") == 0)
        {
          char *dummy;
          steeringSessionId = (unsigned int) (strtoul(paramValue, &dummy, 10));
        }
        else
        {
          PrintUsage();
          return;
        }
      }

      ok = true;
    }

    void CommandLine::PrintUsage()
    {
      printf("-!-!-!-!-!-!-!-!-!-!-!-!");
      printf("Correct usage: hemelb [-<Parameter Name> <Parameter Value>]* \n");
      printf("Parameter name and significance:\n");
      printf("-in \t Path to the configuration xml file (default is config.xml)\n");
      printf("-out \t Path to the output folder (default is based on input file, e.g. config_xml_results)\n");
      printf("-s \t Number of snapshots to take per cycle (default 10)\n");
      printf("-i \t Number of images to create per cycle (default is 10)\n");
      printf("-ss \t Steering session identifier (default is 1)\n");
      printf("-!-!-!-!-!-!-!-!-!-!-!-!");
    }
  }
}
