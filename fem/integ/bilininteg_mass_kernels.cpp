// Copyright (c) 2010-2023, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#include "bilininteg_mass_kernels.hpp"

namespace mfem
{

namespace internal
{

void PAMassAssembleDiagonal(const int dim, const int D1D,
                            const int Q1D, const int NE,
                            const Array<double> &B,
                            const Vector &D,
                            Vector &Y)
{
   if (dim == 2)
   {
      switch ((D1D << 4 ) | Q1D)
      {
         case 0x22: return SmemPAMassAssembleDiagonal2D<2,2,16>(NE,B,D,Y);
         case 0x33: return SmemPAMassAssembleDiagonal2D<3,3,16>(NE,B,D,Y);
         case 0x44: return SmemPAMassAssembleDiagonal2D<4,4,8>(NE,B,D,Y);
         case 0x55: return SmemPAMassAssembleDiagonal2D<5,5,8>(NE,B,D,Y);
         case 0x66: return SmemPAMassAssembleDiagonal2D<6,6,4>(NE,B,D,Y);
         case 0x77: return SmemPAMassAssembleDiagonal2D<7,7,4>(NE,B,D,Y);
         case 0x88: return SmemPAMassAssembleDiagonal2D<8,8,2>(NE,B,D,Y);
         case 0x99: return SmemPAMassAssembleDiagonal2D<9,9,2>(NE,B,D,Y);
         default:   return PAMassAssembleDiagonal2D(NE,B,D,Y,D1D,Q1D);
      }
   }
   else if (dim == 3)
   {
      switch ((D1D << 4 ) | Q1D)
      {
         case 0x23: return SmemPAMassAssembleDiagonal3D<2,3>(NE,B,D,Y);
         case 0x24: return SmemPAMassAssembleDiagonal3D<2,4>(NE,B,D,Y);
         case 0x26: return SmemPAMassAssembleDiagonal3D<2,6>(NE,B,D,Y);
         case 0x34: return SmemPAMassAssembleDiagonal3D<3,4>(NE,B,D,Y);
         case 0x35: return SmemPAMassAssembleDiagonal3D<3,5>(NE,B,D,Y);
         case 0x45: return SmemPAMassAssembleDiagonal3D<4,5>(NE,B,D,Y);
         case 0x48: return SmemPAMassAssembleDiagonal3D<4,8>(NE,B,D,Y);
         case 0x56: return SmemPAMassAssembleDiagonal3D<5,6>(NE,B,D,Y);
         case 0x67: return SmemPAMassAssembleDiagonal3D<6,7>(NE,B,D,Y);
         case 0x78: return SmemPAMassAssembleDiagonal3D<7,8>(NE,B,D,Y);
         case 0x89: return SmemPAMassAssembleDiagonal3D<8,9>(NE,B,D,Y);
         default:   return PAMassAssembleDiagonal3D(NE,B,D,Y,D1D,Q1D);
      }
   }
   MFEM_ABORT("Unknown kernel.");
}

#ifdef MFEM_USE_OCCA
void OccaPAMassApply2D(const int D1D,
                       const int Q1D,
                       const int NE,
                       const Array<double> &B,
                       const Array<double> &Bt,
                       const Vector &D,
                       const Vector &X,
                       Vector &Y)
{
   occa::properties props;
   props["defines/D1D"] = D1D;
   props["defines/Q1D"] = Q1D;
   const occa::memory o_B = OccaMemoryRead(B.GetMemory(), B.Size());
   const occa::memory o_Bt = OccaMemoryRead(Bt.GetMemory(), Bt.Size());
   const occa::memory o_D = OccaMemoryRead(D.GetMemory(), D.Size());
   const occa::memory o_X = OccaMemoryRead(X.GetMemory(), X.Size());
   occa::memory o_Y = OccaMemoryReadWrite(Y.GetMemory(), Y.Size());
   const occa_id_t id = std::make_pair(D1D,Q1D);
   if (!Device::Allows(Backend::OCCA_CUDA))
   {
      static occa_kernel_t OccaMassApply2D_cpu;
      if (OccaMassApply2D_cpu.find(id) == OccaMassApply2D_cpu.end())
      {
         const occa::kernel MassApply2D_CPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "MassApply2D_CPU", props);
         OccaMassApply2D_cpu.emplace(id, MassApply2D_CPU);
      }
      OccaMassApply2D_cpu.at(id)(NE, o_B, o_Bt, o_D, o_X, o_Y);
   }
   else
   {
      static occa_kernel_t OccaMassApply2D_gpu;
      if (OccaMassApply2D_gpu.find(id) == OccaMassApply2D_gpu.end())
      {
         const occa::kernel MassApply2D_GPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "MassApply2D_GPU", props);
         OccaMassApply2D_gpu.emplace(id, MassApply2D_GPU);
      }
      OccaMassApply2D_gpu.at(id)(NE, o_B, o_Bt, o_D, o_X, o_Y);
   }
}

void OccaPAMassApply3D(const int D1D,
                       const int Q1D,
                       const int NE,
                       const Array<double> &B,
                       const Array<double> &Bt,
                       const Vector &D,
                       const Vector &X,
                       Vector &Y)
{
   occa::properties props;
   props["defines/D1D"] = D1D;
   props["defines/Q1D"] = Q1D;
   const occa::memory o_B = OccaMemoryRead(B.GetMemory(), B.Size());
   const occa::memory o_Bt = OccaMemoryRead(Bt.GetMemory(), Bt.Size());
   const occa::memory o_D = OccaMemoryRead(D.GetMemory(), D.Size());
   const occa::memory o_X = OccaMemoryRead(X.GetMemory(), X.Size());
   occa::memory o_Y = OccaMemoryReadWrite(Y.GetMemory(), Y.Size());
   const occa_id_t id = std::make_pair(D1D,Q1D);
   if (!Device::Allows(Backend::OCCA_CUDA))
   {
      static occa_kernel_t OccaMassApply3D_cpu;
      if (OccaMassApply3D_cpu.find(id) == OccaMassApply3D_cpu.end())
      {
         const occa::kernel MassApply3D_CPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "MassApply3D_CPU", props);
         OccaMassApply3D_cpu.emplace(id, MassApply3D_CPU);
      }
      OccaMassApply3D_cpu.at(id)(NE, o_B, o_Bt, o_D, o_X, o_Y);
   }
   else
   {
      static occa_kernel_t OccaMassApply3D_gpu;
      if (OccaMassApply3D_gpu.find(id) == OccaMassApply3D_gpu.end())
      {
         const occa::kernel MassApply3D_GPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "MassApply3D_GPU", props);
         OccaMassApply3D_gpu.emplace(id, MassApply3D_GPU);
      }
      OccaMassApply3D_gpu.at(id)(NE, o_B, o_Bt, o_D, o_X, o_Y);
   }
}
#endif // MFEM_USE_OCCA

void PAMassApply(const int dim,
                 const int D1D,
                 const int Q1D,
                 const int NE,
                 const Array<double> &B,
                 const Array<double> &Bt,
                 const Vector &D,
                 const Vector &X,
                 Vector &Y)
{
#ifdef MFEM_USE_OCCA
   if (DeviceCanUseOcca())
   {
      if (dim == 2)
      {
         return OccaPAMassApply2D(D1D,Q1D,NE,B,Bt,D,X,Y);
      }
      if (dim == 3)
      {
         return OccaPAMassApply3D(D1D,Q1D,NE,B,Bt,D,X,Y);
      }
      MFEM_ABORT("OCCA PA Mass Apply unknown kernel!");
   }
#endif // MFEM_USE_OCCA
   const int id = (D1D << 4) | Q1D;

   if (dim == 2)
   {
      switch (id)
      {
         case 0x22: return SmemPAMassApply2D<2,2,16>(NE,B,Bt,D,X,Y);
         case 0x24: return SmemPAMassApply2D<2,4,16>(NE,B,Bt,D,X,Y);
         case 0x33: return SmemPAMassApply2D<3,3,16>(NE,B,Bt,D,X,Y);
         case 0x34: return SmemPAMassApply2D<3,4,16>(NE,B,Bt,D,X,Y);
         case 0x35: return SmemPAMassApply2D<3,5,16>(NE,B,Bt,D,X,Y);
         case 0x36: return SmemPAMassApply2D<3,6,16>(NE,B,Bt,D,X,Y);
         case 0x44: return SmemPAMassApply2D<4,4,8>(NE,B,Bt,D,X,Y);
         case 0x46: return SmemPAMassApply2D<4,6,8>(NE,B,Bt,D,X,Y);
         case 0x48: return SmemPAMassApply2D<4,8,4>(NE,B,Bt,D,X,Y);
         case 0x55: return SmemPAMassApply2D<5,5,8>(NE,B,Bt,D,X,Y);
         case 0x57: return SmemPAMassApply2D<5,7,8>(NE,B,Bt,D,X,Y);
         case 0x58: return SmemPAMassApply2D<5,8,2>(NE,B,Bt,D,X,Y);
         case 0x66: return SmemPAMassApply2D<6,6,4>(NE,B,Bt,D,X,Y);
         case 0x77: return SmemPAMassApply2D<7,7,4>(NE,B,Bt,D,X,Y);
         case 0x88: return SmemPAMassApply2D<8,8,2>(NE,B,Bt,D,X,Y);
         case 0x99: return SmemPAMassApply2D<9,9,2>(NE,B,Bt,D,X,Y);
         default:   return PAMassApply2D(NE,B,Bt,D,X,Y,D1D,Q1D);
      }
   }
   else if (dim == 3)
   {
      switch (id)
      {
         case 0x22: return SmemPAMassApply3D<2,2>(NE,B,Bt,D,X,Y);
         case 0x23: return SmemPAMassApply3D<2,3>(NE,B,Bt,D,X,Y);
         case 0x24: return SmemPAMassApply3D<2,4>(NE,B,Bt,D,X,Y);
         case 0x26: return SmemPAMassApply3D<2,6>(NE,B,Bt,D,X,Y);
         case 0x34: return SmemPAMassApply3D<3,4>(NE,B,Bt,D,X,Y);
         case 0x35: return SmemPAMassApply3D<3,5>(NE,B,Bt,D,X,Y);
         case 0x36: return SmemPAMassApply3D<3,6>(NE,B,Bt,D,X,Y);
         case 0x37: return SmemPAMassApply3D<3,7>(NE,B,Bt,D,X,Y);
         case 0x45: return SmemPAMassApply3D<4,5>(NE,B,Bt,D,X,Y);
         case 0x46: return SmemPAMassApply3D<4,6>(NE,B,Bt,D,X,Y);
         case 0x48: return SmemPAMassApply3D<4,8>(NE,B,Bt,D,X,Y);
         case 0x56: return SmemPAMassApply3D<5,6>(NE,B,Bt,D,X,Y);
         case 0x58: return SmemPAMassApply3D<5,8>(NE,B,Bt,D,X,Y);
         case 0x67: return SmemPAMassApply3D<6,7>(NE,B,Bt,D,X,Y);
         case 0x78: return SmemPAMassApply3D<7,8>(NE,B,Bt,D,X,Y);
         case 0x89: return SmemPAMassApply3D<8,9>(NE,B,Bt,D,X,Y);
         case 0x9A: return SmemPAMassApply3D<9,10>(NE,B,Bt,D,X,Y);
         default:   return PAMassApply3D(NE,B,Bt,D,X,Y,D1D,Q1D);
      }
   }
   mfem::out << "Unknown kernel 0x" << std::hex << id << std::endl;
   MFEM_ABORT("Unknown kernel.");
}

} // namespace internal

} // namespace mfem
