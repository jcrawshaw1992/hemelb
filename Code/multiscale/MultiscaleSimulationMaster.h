// 
// Copyright (C) University College London, 2007-2012, all rights reserved.
// 
// This file is part of HemeLB and is CONFIDENTIAL. You may not work 
// with, install, use, duplicate, modify, redistribute or share this
// file, or any part thereof, other than as allowed by any agreement
// specifically made by you with University College London.
// 

#ifndef HEMELB_MULTISCALE_MULTISCALESIMULATIONMASTER_H
#define HEMELB_MULTISCALE_MULTISCALESIMULATIONMASTER_H
#include <vector>
#include "multiscale/Intercommunicator.h"
#include "lb/boundaries/iolets/InOutLetVelocityAware.h"
#include "SimulationMaster.h"

/*Temporary addition */
#include <mpi.h>

namespace hemelb
{
  namespace multiscale
  {
    /***
     * Instead of adding multiscale functionality to the standard simulation master, we keep this here,
     * so the main code can be read without thinking about multiscale.
     */
    template<class Intercommunicator> class MultiscaleSimulationMaster : public SimulationMaster
    {
      public:
        MultiscaleSimulationMaster(hemelb::configuration::CommandLine &options, Intercommunicator & aintercomms) :
            SimulationMaster(options), intercomms(aintercomms), multiscaleIoletType("inoutlet")
        {
          // We only have one shared object type so far, an iolet.

          //lb::boundaries::iolets::InOutLetVelocityAware::DefineType(multiscaleIoletType);
          lb::boundaries::iolets::InOutLetMultiscale::DefineType(multiscaleIoletType);

          int GlobalIoletCount[] = {inletValues->GetLocalIoletCount(), outletValues->GetLocalIoletCount()};

          MPI_Bcast(GlobalIoletCount, 2, MPI_INT, 0, MPI_COMM_WORLD);

          std::vector<std::vector<site_t> > invertedInletBoundaryList(GlobalIoletCount[0]);
          std::vector<std::vector<site_t> > invertedOutletBoundaryList(GlobalIoletCount[1]);



          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("inlets start %i/%i",inletValues->GetLocalIoletCount(), GlobalIoletCount[0]);
          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("outlets start %i/%i",outletValues->GetLocalIoletCount(), GlobalIoletCount[1]);

          //TODO: Propagate IoletCounts from process 0 to others to get this completely straight!
          //Throw a warning when process 0 count mismatches with the aggregate of the others.

          std::vector<site_t> dummy;

          /* Populate with dummy entries for testing... */
          for (unsigned int i = 0; i < GlobalIoletCount[0]; i++)
          {
            invertedInletBoundaryList[i] = dummy;
          }

          for (unsigned int i = 0; i < GlobalIoletCount[1]; i++)
          {
            invertedOutletBoundaryList[i] = dummy;
          }

          /* Do not include the non-iolet adjacent sites (resp. MidFluid and Wall-adjacent). */
          long long int offset = latticeData->GetMidDomainCollisionCount(0)
              + latticeData->GetMidDomainCollisionCount(1);
          /* Do include iolet adjacent sites (inlet) */
          long long int ioletsSiteCount = latticeData->GetMidDomainCollisionCount(2);
          invertedInletBoundaryList = PopulateInvertedBoundaryList(latticeData,
                                                                   invertedInletBoundaryList,
                                                                   offset,
                                                                   ioletsSiteCount);

          offset += latticeData->GetMidDomainCollisionCount(2);
          /* Do include iolet adjacent sites (outlet) */
          ioletsSiteCount = latticeData->GetMidDomainCollisionCount(3);
          invertedOutletBoundaryList = PopulateInvertedBoundaryList(latticeData,
                                                                    invertedOutletBoundaryList,
                                                                    offset,
                                                                    ioletsSiteCount);

          offset += latticeData->GetMidDomainCollisionCount(3);
          /* Do include iolet adjacent sites (inlet-wall) */
          ioletsSiteCount = latticeData->GetMidDomainCollisionCount(4);
          invertedInletBoundaryList = PopulateInvertedBoundaryList(latticeData,
                                                                   invertedInletBoundaryList,
                                                                   offset,
                                                                   ioletsSiteCount);

          offset += latticeData->GetMidDomainCollisionCount(4);
          /* Do include iolet adjacent sites (outlet-wall) */
          ioletsSiteCount = latticeData->GetMidDomainCollisionCount(5);
          invertedOutletBoundaryList = PopulateInvertedBoundaryList(latticeData,
                                                                    invertedOutletBoundaryList,
                                                                    offset,
                                                                    ioletsSiteCount);

          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Populated inlets: %i %i", invertedInletBoundaryList.size(), invertedInletBoundaryList[0].size());
          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Populated outlets: %i %i", invertedOutletBoundaryList.size(), invertedOutletBoundaryList[0].size());

          //PrintVectorList(invertedInletBoundaryList);
          //PrintVectorList(invertedOutletBoundaryList);

          //TODO: Debug
          invertedInletBoundaryList  = ExchangeAndCompleteInverseBoundaryList(invertedInletBoundaryList);
          invertedOutletBoundaryList = ExchangeAndCompleteInverseBoundaryList(invertedOutletBoundaryList);

          //PrintVectorList(invertedInletBoundaryList);
          //PrintVectorList(invertedOutletBoundaryList);

          //std::cout << "1) inlets: " << invertedInletBoundaryList.size() << " " << invertedInletBoundaryList[0].size()
          //    << std::endl;
          //std::cout << "1) outlets: " << invertedOutletBoundaryList.size() << " "
          //    << invertedOutletBoundaryList[0].size() << std::endl;

          // we only want to register those iolets which are needed on this process.
          // Fortunately, the BoundaryValues instance has worked this out for us.
          for (unsigned int i = 0; i < inletValues->GetLocalIoletCount(); i++)
          {
            hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("1) %i %i", i, GlobalIoletCount[0]);
            // could be a if dynamic_cast<> rather than using a castable? virtual method pattern, if we prefer.
            if (inletValues->GetLocalIolet(i)->IsRegistrationRequired())
            {
              hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("2) inlets: %i %i %i", invertedInletBoundaryList.size(), invertedInletBoundaryList[0].size(), i);
              static_cast<lb::boundaries::iolets::InOutLetMultiscale*>(inletValues->GetLocalIolet(i))->Register(intercomms,
                                                                                                                multiscaleIoletType);
              hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("3) inlets: %i %i", invertedInletBoundaryList.size(), invertedInletBoundaryList[i].size());
              //static_cast<lb::boundaries::iolets::InOutLetVelocityAware*>(inletValues->GetLocalIolet(i))->InitialiseNeighbouringSites(neighbouringDataManager,
              //                                                                                                                        latticeData,
              //                                                                                                                        static_cast<hemelb::lb::MacroscopicPropertyCache*>(&latticeBoltzmannModel->GetPropertyCache()),
              //                                                                                                                        invertedInletBoundaryList[i]);
            }
          }

          for (unsigned int i = 0; i < outletValues->GetLocalIoletCount(); i++)
          {
            hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("1) %i %i", i, GlobalIoletCount[1]);
            if (outletValues->GetLocalIolet(i)->IsRegistrationRequired())
            {
              hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("2) outlets: %i %i %i", invertedOutletBoundaryList.size(), invertedOutletBoundaryList[0].size(), i);
              static_cast<lb::boundaries::iolets::InOutLetMultiscale*>(outletValues->GetLocalIolet(i))->Register(intercomms,
                                                                                                                 multiscaleIoletType);
              hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("3) outlets: %i %i", invertedOutletBoundaryList.size(), invertedOutletBoundaryList[i].size());
              //static_cast<lb::boundaries::iolets::InOutLetVelocityAware*>(outletValues->GetLocalIolet(i))->InitialiseNeighbouringSites(neighbouringDataManager,
              //                                                                                                                         latticeData,
              //                                                                                                                         static_cast<hemelb::lb::MacroscopicPropertyCache*>(&latticeBoltzmannModel->GetPropertyCache()),
              //                                                                                                                         invertedOutletBoundaryList[i]);
            }
          }

          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("MSMaster ShareICs started...");
          intercomms.ShareInitialConditions();
          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("MSMaster Init finished!");
        }

        void PrintVectorList(std::vector<std::vector<site_t> > v)
        {
          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Printing Vector List:");
          for (unsigned int i = 0; i < v.size(); i++)
          {
            for (unsigned int j = 0; j < v[i].size(); j++)
            {
              hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Boundary: %i % i %i", i, j, v[i][j]);
            }
          }
        }

        void DoTimeStep()
        {
          bool advance=intercomms.DoMultiscale(GetState()->GetTime());
          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("At time step %i, should advance %i", GetState()->GetTimeStep(), static_cast<int>(advance));

          if (advance)
          {
            hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::Singleton>("Measured Density is %f. Pressure is %f.",
                                                                                inletValues->GetLocalIolet(0)->GetDensity(GetState()->GetTimeStep()),
                                                                                inletValues->GetLocalIolet(0)->GetPressureMax());
            SimulationMaster::DoTimeStep(); //This one hangs!
            hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::Singleton>("HemeLB advanced to time %f.",
                                                                                GetState()->GetTime());
          }
          else
          {
            hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::Singleton>("HemeLB waiting pending multiscale siblings.");
            return;
          };
        }
        Intercommunicator &intercomms;
        typename Intercommunicator::IntercommunicandTypeT multiscaleIoletType;

      private:
        std::vector<std::vector<site_t> > PopulateInvertedBoundaryList(hemelb::geometry::LatticeData* latticeData,
                                                                       std::vector<std::vector<site_t> > invertedBoundaryList,
                                                                       int offset,
                                                                       int ioletsSiteCount)
        {
          //Populate an invertedBoundaryList
          //out: iBL
          //in: LatticeData, [Site Object]->SiteData->GetBoundaryID,
          //MPI_Gatherv if data is only this process.

          for (int i = 0; i < ioletsSiteCount; i++)
          {
            /* 1. Obtain Boundary ID number. */
            hemelb::geometry::SiteData s = latticeData->GetSite(offset + i).GetSiteData();
            int boundaryID = s.GetBoundaryId();

            /* 2. Grow the list to an appropriate size if needed. */
            while ( ((int) invertedBoundaryList.size()) <= boundaryID)
            {
              hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("WARNING: Growing the invertedBoundaryList, because we created in wrongly in MultiscaleSimulation Master.");
              std::vector<site_t> a(0);
              invertedBoundaryList.push_back(a);
              if (invertedBoundaryList.size() > 100000)
              {
                hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("ERROR: invertedBoundaryList is growing to ridiculous proportions due to a faulty boundaryID.");
                exit(-1);
              }
            }

            /* 3. Insert this site in the inverted Boundary List. */
            invertedBoundaryList[boundaryID].push_back(latticeData->GetGlobalNoncontiguousSiteIdFromGlobalCoords(latticeData->GetSite(offset + i).GetGlobalSiteCoords()));
          }
          return invertedBoundaryList;
        }

        std::vector<std::vector<site_t> > ExchangeAndCompleteInverseBoundaryList(std::vector<std::vector<site_t> > inList)
        {

          std::vector<std::vector<site_t> > outList;

          int *recvSizes = new int[hemelb::topology::NetworkTopology::Instance()->GetProcessorCount()];
          int *recvDispls = new int[hemelb::topology::NetworkTopology::Instance()->GetProcessorCount()];

          /* TODO: ASSUMPTION:
           * inList.size() is equal everywhere. This is not necessarily the case.
           * Use an AllReduce MAX and resize inList accordingly to make the remnant
           * of the code work here for unequal inList sizes. */

          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Starting IBL Exchange... size = %i", inList.size());

          for (unsigned int i = 0; i < inList.size(); i++)
          {
            int sendSize = ((int) inList[i].size());
            site_t *sendList = new site_t[inList[i].size()];
            for (unsigned int j = 0; j < inList[i].size(); j++)
            {
              sendList[j] = inList[i][j];
            }

            MPI_Allgather(&sendSize,
                          1,
                          MPI_INT,
                          recvSizes,
                          1,
                          MPI_INT,
                          hemelb::topology::NetworkTopology::Instance()->GetComms().GetCommunicator());

            int64_t totalSize = 0;

            int np = 0;
            int rank = 0;
            MPI_Comm_size(hemelb::topology::NetworkTopology::Instance()->GetComms().GetCommunicator(), &np);
            MPI_Comm_rank(hemelb::topology::NetworkTopology::Instance()->GetComms().GetCommunicator(), &rank);
            int64_t offset = 0;

            //std::cout << "rank = " << rank << " np = " << np << "):" << std::endl;

            for (int j = 0; j < np; j++)
            {
              totalSize += recvSizes[j];
              recvDispls[j] = offset;
              offset += recvSizes[j];
              std::cout << "rank = " << rank << ":" << recvSizes[j] << "/" << recvDispls[j] << std::endl;
            }

            site_t *recvList = new site_t[totalSize]; //inList[i].size()

            //std::cout << "rank = " << rank << ", AllGatherv" << std::endl;

            MPI_Allgatherv(sendList,
                           inList[i].size(),
                           MPI_LONG_LONG,
                           recvList,
                           recvSizes,
                           recvDispls,
                           MPI_LONG_LONG,
                           hemelb::topology::NetworkTopology::Instance()->GetComms().GetCommunicator());

            std::cout << "done." << std::endl;

            std::vector<site_t> subList(totalSize);
            for (int j = 0; j < totalSize; j++)
            {
              subList[j] = recvList[j]; //I could have used push_back here.
            }
            outList.push_back(subList);
          }
          hemelb::log::Logger::Log<hemelb::log::Info, hemelb::log::OnePerCore>("Finishing IBL Exchange...");
          return outList;
        }
    }
    ;
  }
}

#endif // HEMELB_MULTISCALE_MULTISCALE_SIMULATION_MASTER_H
