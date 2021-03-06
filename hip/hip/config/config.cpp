// Copyright (c) 2017, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-734707. All Rights
// reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.
#include "../hip.hpp"
#include <mpi-ext.h>
#include <unistd.h>

namespace mfem
{


// ***************************************************************************
void computeCapabilityOfTheDevice(const int mpi_rank,
                                  const hipDevice_t hipDevice,
                                  const int device)
{
   char name[128];
   int major, minor;
   hipDeviceGetName(name, 128, hipDevice);
   hipDeviceComputeCapability(&major, &minor, device);
   printf("\033[32m[laghos] Rank_%d => Device_%d (%s:sm_%d.%d)\033[m\n",
          mpi_rank, device, name, major, minor);
}

// ***************************************************************************
static bool isTux(void)
{
   char hostname[1024];
   hostname[1023] = '\0';
   gethostname(hostname, 1023);
   if (strncmp("tux", hostname, 3)==0) { return true; }
   return false;
}

// ***************************************************************************
__attribute__((unused))
static void printDevProp(hipDeviceProp_t devProp)
{
   printf("Major revision number:         %d\n",  devProp.major);
   printf("Minor revision number:         %d\n",  devProp.minor);
   printf("Name:                          %s\n",  devProp.name);
   printf("Total global memory:           %lu\n",  devProp.totalGlobalMem);
   printf("Total shared memory per block: %lu\n",  devProp.sharedMemPerBlock);
   printf("Total registers per block:     %d\n",  devProp.regsPerBlock);
   printf("Warp size:                     %d\n",  devProp.warpSize);
   printf("Maximum threads per block:     %d\n",  devProp.maxThreadsPerBlock);
   for (int i = 0; i < 3; ++i)
   {
      printf("Maximum dimension %d of block:  %d\n", i, devProp.maxThreadsDim[i]);
   }
   for (int i = 0; i < 3; ++i)
   {
      printf("Maximum dimension %d of grid:   %d\n", i, devProp.maxGridSize[i]);
   }
   printf("Clock rate:                    %d\n",  devProp.clockRate);
   printf("Total constant memory:         %lu\n",  devProp.totalConstMem);
   printf("Number of multiprocessors:     %d\n",  devProp.multiProcessorCount);
}

// ***************************************************************************
// *   Setup
// ***************************************************************************
void rconfig::Setup(const int _mpi_rank,
                    const int _mpi_size,
                    const bool _hip,
                    const bool _aware,
                    const bool _share,
                    const bool _hcpo,
                    const bool _sync,
                    const int rs_levels)
{
   mpi_rank=_mpi_rank;
   mpi_size=_mpi_size;

   // Look if we are on a Tux machine
   const bool tux = isTux();
   if (tux && Root())
   {
      printf("\033[32m[laghos] \033[1mTux\033[m\n");
   }

   // On Tux machines, look for MPS
   // mps = tux?isNvidiaCudaMpsDaemonRunning():false;
   mps = false;
   if (tux && Mps() && Root())
   {
      printf("\033[32m[laghos] \033[32;1mMPS daemon\033[m\033[m\n");
   }
   if (tux && !Mps() && Root())
   {
      printf("\033[32m[laghos] \033[31;1mNo MPS daemon\033[m\n");
   }

   // Get the number of devices with compute capability greater or equal to 2.0
   // Can be changed wuth HIP_VISIBLE_DEVICES
   hipGetDeviceCount(&gpu_count);
   hip=_hip;
   aware=_aware;
   share=_share;
   hcpo=_hcpo;
   sync=_sync;

   // LAGHOS_DEBUG warning output
#if defined(LAGHOS_DEBUG)
   if (Root())
   {
      printf("\033[32m[laghos] \033[31;1mLAGHOS_DEBUG\033[m\n");
   }
#endif

   // Check for Enforced Kernel Synchronization
   if (Sync() && Root())
   {
      printf("\033[32m[laghos] \033[31;1mEnforced Kernel Synchronization!\033[m\n");
   }

   // Check if MPI is HIP aware
   if (Root())
      printf("\033[32m[laghos] MPI %s HIP aware\033[m\n",
             aware?"\033[1mIS":"is \033[31;1mNOT\033[32m");

   if (Root())
   {
      printf("\033[32m[laghos] HIP device count: %i\033[m\n", gpu_count);
   }

   // Initializes the driver API
   // Must be called before any other function from the driver API
   // Currently, the Flags parameter must be 0.
   const unsigned int Flags = 0; // parameter must be 0
   hipInit(Flags);

   // Returns properties for the selected device
   const int device = Mps()?0:(mpi_rank%gpu_count);
   // Check if we have enough devices for all ranks
   assert(device<gpu_count);

   // Get a handle to our compute device
   hipDeviceGet(&hipDevice,device);
   computeCapabilityOfTheDevice(mpi_rank,hipDevice,device);

   // Get the properties of the device
   struct hipDeviceProp_t properties;
   hipGetDeviceProperties(&properties, device);
#if defined(LAGHOS_DEBUG)
   if (Root())
   {
      printDevProp(properties);
   }
#endif // LAGHOS_DEBUG
   maxXGridSize=properties.maxGridSize[0];
   maxXThreadsDim=properties.maxThreadsDim[0];

   // Create our context
   hStream=new hipStream_t;
   hipStreamCreate(hStream);
}

// ***************************************************************************
bool rconfig::IAmAlone()
{
   return mpi_size==1;
}

// ***************************************************************************
bool rconfig::GeomNeedsUpdate(const int sequence)
{
   assert(sequence==0);
   return (sequence!=0);
}

// ***************************************************************************
bool rconfig::DoHostConformingProlongationOperator()
{
   return (Hip())?hcpo:true;
}

} // namespace mfem
