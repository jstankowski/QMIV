/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2025 Patrycja Kaźmierczak <patrycja.kazmierczak@student.put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <functional>
#include <utility>
#include <array>
#include "xTestUtils.h"
#include "xTimeUtils.h"
#include "xMemory.h"
#include "xStructSim.h"
#include "xStructSimConsts.h"
#include "xPlane.h"
#include "xKBNS.h"

using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================

static const std::vector<int32> c_Dimms = { 4, 8, 11, 16, 32 };
static const std::vector<int32> c_BitDs = { 8, 14 };
static const std::vector<int32> c_Margs = { 0, 4, 32 };
static constexpr int32          c_NumRandomTests = 256;
static constexpr flt64          c_ToleranceSSIM_Gaussian = 0.00001;
static constexpr flt64          c_ToleranceSSIM_Averaged = 0.00000001;

//===============================================================================================================================================================================================================
using fCalcSS = std::function<flt64(const uint16* Tst, const uint16* Ref, int32 StrideT, int32 StrideR, int32 WndSize, flt64 C1, flt64 C2, bool CalcL)>;

void testGaussionCoeffs()
{
  {
    xKBNS1 Acc;
    for(int32 y = 0; y < xStructSimConsts::c_FilterSize; y++)
    {
      for(int32 x = 0; x < xStructSimConsts::c_FilterSize; x++)
      {
        Acc.acc(xStructSimConsts::c_FilterRglrGaussFlt32[y][x]);
      }
    }
    flt64 Sum = Acc.result();
    CHECK(xIsApproximatelyEqual(Sum, 1.0, 0.0000001));
  }

  {
    xKBNS1 Acc;
    for(int32 y = 0; y < xStructSimConsts::c_FilterSize; y++)
    {
      for(int32 x = 0; x < xStructSimConsts::c_FilterSize; x++)
      {
        Acc.acc(xStructSimConsts::c_FilterRglrGaussFlt64[y][x]);
      }
    }
    flt64 Sum = Acc.result();
    CHECK(xIsApproximatelyEqual(Sum, 1.0));
  }
}

void testCalcRglr(fCalcSS CalcRglrTst, fCalcSS CalcRglrRef, bool CalcL, flt64 Tolerance)
{
  uint32 State = xTestUtils::c_XorShiftSeed;

  int32V2 Size = { xStructSimConsts::c_FilterSize, xStructSimConsts::c_FilterSize };

  for(const int32 m : c_Margs)
  {
    for(const int32 b : c_BitDs)
    {
      const std::string Description = fmt::format("SizeXxY={}x{} Margin={} BitDepth={}", Size.getX(), Size.getY(), m, b);

      //buffers create
      xPlane<uint16>* Ref = new xPlane<uint16>(Size, b, m);
      xPlane<uint16>* Tst = new xPlane<uint16>(Size, b, m);

      int32 OffT = (5 * Tst->getStride() + 5);
      int32 OffR = (5 * Ref->getStride() + 5);

      //deterministic tests
      CAPTURE(Description + fmt::format(" Deterministic"));
      uint16 Max = (uint16)xBitDepth2MaxValue(b);
      uint16 Mid = (uint16)xBitDepth2MidValue(b);
      std::vector<uint16V2> VPs{ { 0,Max }, { Max,0 }, { Mid,0 }, { Mid,0 }, { Max,Mid }, { Max,Mid } };

      flt64 C1 = xPow2(xStructSimConsts::c_K1<flt64> * (flt64)Max);
      flt64 C2 = xPow2(xStructSimConsts::c_K2<flt64> * (flt64)Max);

      for(const uint16V2 TestVals : VPs)
      {
        Ref->zero(true);
        Tst->zero(true);
        Ref->fill(TestVals[0], false);
        Tst->fill(TestVals[1], false);

        flt64 A = CalcRglrRef(Tst->getAddr()+ OffT, Ref->getAddr()+OffR, Tst->getStride(), Ref->getStride(), xStructSimConsts::c_FilterSize, C1, C2, CalcL);
        flt64 B = CalcRglrTst(Tst->getAddr()+ OffT, Ref->getAddr()+OffR, Tst->getStride(), Ref->getStride(), xStructSimConsts::c_FilterSize, C1, C2, CalcL);
        CHECK(xIsApproximatelyEqual(A, B, Tolerance));
      }

      //gradient test
      CAPTURE(Description + fmt::format(" Gradient"));
      Ref->zero(true);
      Tst->zero(true);
      xTestUtils::fillGradientXY(Ref->getAddr(), Ref->getStride(), Ref->getWidth(), Ref->getHeight(), Ref->getBitDepth(), 0);
      for(int32 GradOffset = 0; GradOffset <= 10; GradOffset++)
      {          
        xTestUtils::fillGradientXY(Tst->getAddr(), Tst->getStride(), Tst->getWidth(), Tst->getHeight(), Tst->getBitDepth(), GradOffset);
        flt64 A = CalcRglrRef(Tst->getAddr()+ OffT, Ref->getAddr()+OffR, Tst->getStride(), Ref->getStride(), xStructSimConsts::c_FilterSize, C1, C2, CalcL);
        flt64 B = CalcRglrTst(Tst->getAddr()+ OffT, Ref->getAddr()+OffR, Tst->getStride(), Ref->getStride(), xStructSimConsts::c_FilterSize, C1, C2, CalcL);
        CHECK(xIsApproximatelyEqual(A, B, Tolerance));
      }

      //random tests
      CAPTURE(Description + fmt::format(" Random"));
      Ref->zero(true);
      Tst->zero(true);
      for(int32 n = 0; n < c_NumRandomTests; n++)
      {
        State = xTestUtils::fillMidNoise(Ref->getAddr(), Ref->getStride(), Ref->getWidth(), Ref->getHeight(), b, 0, State);
        State = xTestUtils::fillMidNoise(Tst->getAddr(), Tst->getStride(), Tst->getWidth(), Tst->getHeight(), b, 0, State);

        flt64 A = CalcRglrRef(Tst->getAddr()+ OffT, Ref->getAddr()+OffR, Tst->getStride(), Ref->getStride(), xStructSimConsts::c_FilterSize, C1, C2, CalcL);
        flt64 B = CalcRglrTst(Tst->getAddr()+ OffT, Ref->getAddr()+OffR, Tst->getStride(), Ref->getStride(), xStructSimConsts::c_FilterSize, C1, C2, CalcL);
        CHECK(xIsApproximatelyEqual(A, B, Tolerance));
      }
    }
  }
}
void testCalcBlckAvg(fCalcSS CalcBlckAvg, int32 WndSize, bool CalcL)
{
  uint32 State = xTestUtils::c_XorShiftSeed;

  int32V2 Size = { WndSize, WndSize };

  for(const int32 m : c_Margs)
  {
    for(const int32 b : c_BitDs)
    {
      const std::string Description = fmt::format("SizeXxY={}x{} Margin={} BitDepth={}", Size.getX(), Size.getY(), m, b);

      //buffers create
      xPlane<uint16>* Ref = new xPlane<uint16>(Size, b, m);
      xPlane<uint16>* Tst = new xPlane<uint16>(Size, b, m);

      //deterministic tests
      CAPTURE(Description + fmt::format(" Deterministic"));
      uint16 Max = (uint16)xBitDepth2MaxValue(b);
      uint16 Mid = (uint16)xBitDepth2MidValue(b);
      std::vector<uint16V2> VPs{ {0,Max}, {Max,0}, {Mid,0}, {Mid,0}, {Max,Mid}, {Max,Mid} };

      flt64 C1 = xPow2(xStructSimConsts::c_K1<flt64> *(flt64)Max);
      flt64 C2 = xPow2(xStructSimConsts::c_K2<flt64> *(flt64)Max);

      for(const uint16V2 TestVals : VPs)
      {
        Ref->zero(true);
        Tst->zero(true);
        Ref->fill(TestVals[0], false);
        Tst->fill(TestVals[1], false);

        flt64 A = xStructSimSTD::CalcBlckAvg(Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        flt64 B =                CalcBlckAvg(Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        CHECK(A == B);
      }

      //gradient test
      CAPTURE(Description + fmt::format(" Gradient"));
      Ref->zero(true);
      Tst->zero(true);
      xTestUtils::fillGradientXY(Ref->getAddr(), Ref->getStride(), Ref->getWidth(), Ref->getHeight(), Ref->getBitDepth(), 0);
      for(int32 GradOffset = 0; GradOffset <= 10; GradOffset++)
      {          
        xTestUtils::fillGradientXY(Tst->getAddr(), Tst->getStride(), Tst->getWidth(), Tst->getHeight(), Tst->getBitDepth(), GradOffset);
        flt64 A = xStructSimSTD::CalcBlckAvg(Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        flt64 B =                CalcBlckAvg(Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        CHECK(A == B);
      }

      //random tests
      CAPTURE(Description + fmt::format(" Random"));
      Ref->zero(true);
      Tst->zero(true);
      for(int32 n = 0; n < c_NumRandomTests; n++)
      {
        State = xTestUtils::fillMidNoise(Ref->getAddr(), Ref->getStride(), Ref->getWidth(), Ref->getHeight(), b, 0, State);
        State = xTestUtils::fillMidNoise(Tst->getAddr(), Tst->getStride(), Tst->getWidth(), Tst->getHeight(), b, 0, State);

        flt64 A = xStructSimSTD::CalcBlckAvg(Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        flt64 B =                CalcBlckAvg(Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        CHECK(A == B);
      }
    }
  }  
}
void testCalcMultiBlckAvg(std::function<void(flt64* restrict SSIMs, const uint16* Tst, const uint16* Ref, int32 StrideT, int32 StrideR, flt64 C1, flt64 C2, bool CalcL)> CalcMultiBlckAvg, int32 WndSize, int32 Stride, int32 BlocksPerBatch, bool CalcL)
{
  uint32 State = xTestUtils::c_XorShiftSeed;

  int32V2 Size = { WndSize + (Stride * (BlocksPerBatch-1)), WndSize };

  std::vector<flt64> A(BlocksPerBatch);
  std::vector<flt64> B(BlocksPerBatch);
  
  for(const int32 m : c_Margs)
  {
    for(const int32 b : c_BitDs)
    {
      const std::string Description = fmt::format("SizeXxY={}x{} Margin={} BitDepth={}", Size.getX(), Size.getY(), m, b);

      //buffers create
      xPlane<uint16>* Ref = new xPlane<uint16>(Size, b, m);
      xPlane<uint16>* Tst = new xPlane<uint16>(Size, b, m);

      //deterministic tests
      CAPTURE(Description + fmt::format(" Deterministic"));
      uint16 Max = (uint16)xBitDepth2MaxValue(b);
      uint16 Mid = (uint16)xBitDepth2MidValue(b);
      std::vector<uint16V2> VPs{ {0,Max}, {Max,0}, {Mid,0}, {Mid,0}, {Max,Mid}, {Max,Mid} };

      flt64 C1 = xPow2(xStructSimConsts::c_K1<flt64> *(flt64)Max);
      flt64 C2 = xPow2(xStructSimConsts::c_K2<flt64> *(flt64)Max);

      for(const uint16V2 TestVals : VPs)
      {
        Ref->zero(true);
        Tst->zero(true);
        Ref->fill(TestVals[0], false);
        Tst->fill(TestVals[1], false);
        std::fill(A.begin(), A.end(), std::numeric_limits<flt64>::quiet_NaN());
        std::fill(B.begin(), B.end(), std::numeric_limits<flt64>::quiet_NaN());

        for(int32 i = 0; i < BlocksPerBatch; i++)
        {
          A[i] = xStructSimSTD::CalcBlckAvg(Tst->getAddr() + Stride * i, Ref->getAddr() + Stride * i, Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        }
        CalcMultiBlckAvg(B.data(), Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), C1, C2, CalcL);
        for(int32 i = 0; i < BlocksPerBatch; i++)
        {
          CHECK(A[i] == B[i]);
        }
      }

      //gradient test
      CAPTURE(Description + fmt::format(" Gradient"));
      Ref->zero(true);
      Tst->zero(true);

      xTestUtils::fillGradientXY(Ref->getAddr(), Ref->getStride(), Ref->getWidth(), Ref->getHeight(), Ref->getBitDepth(), 0);
      for(int32 GradOffset = 1; GradOffset <= 10; GradOffset++)
      {          
        xTestUtils::fillGradientXY(Tst->getAddr(), Tst->getStride(), Tst->getWidth(), Tst->getHeight(), Tst->getBitDepth(), GradOffset);
        std::fill(A.begin(), A.end(), std::numeric_limits<flt64>::quiet_NaN());
        std::fill(B.begin(), B.end(), std::numeric_limits<flt64>::quiet_NaN());
        for(int32 i = 0; i < BlocksPerBatch; i++)
        {
          A[i] = xStructSimSTD::CalcBlckAvg(Tst->getAddr() + Stride * i, Ref->getAddr() + Stride * i, Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        }
        CalcMultiBlckAvg(B.data(), Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), C1, C2, CalcL);
        for(int32 i = 0; i < BlocksPerBatch; i++)
        {
          CHECK(A[i] == B[i]);
        }
      }

      //random tests
      CAPTURE(Description + fmt::format(" Random"));
      Ref->zero(true);
      Tst->zero(true);

      for(int32 n = 0; n < c_NumRandomTests; n++)
      {
        State = xTestUtils::fillMidNoise(Ref->getAddr(), Ref->getStride(), Ref->getWidth(), Ref->getHeight(), b, 0, State);
        State = xTestUtils::fillMidNoise(Tst->getAddr(), Tst->getStride(), Tst->getWidth(), Tst->getHeight(), b, 0, State);


        std::fill(A.begin(), A.end(), std::numeric_limits<flt64>::quiet_NaN());
        std::fill(B.begin(), B.end(), std::numeric_limits<flt64>::quiet_NaN());
        for(int32 i = 0; i < BlocksPerBatch; i++)
        {
          A[i] = xStructSimSTD::CalcBlckAvg(Tst->getAddr() + Stride * i, Ref->getAddr() + Stride * i, Tst->getStride(), Ref->getStride(), WndSize, C1, C2, CalcL);
        }
        CalcMultiBlckAvg(B.data(), Tst->getAddr(), Ref->getAddr(), Tst->getStride(), Ref->getStride(), C1, C2, CalcL);
        for(int32 i = 0; i < BlocksPerBatch; i++)
        {
          CHECK(A[i] == B[i]);
        }
      }
    }
  }
}

//===============================================================================================================================================================================================================

TEST_CASE("xStructSimCoeffs")
{
  testGaussionCoeffs();
}
TEST_CASE("xStructSimSTD")
{
  testCalcRglr(xStructSimSTD::CalcRglrFlt, xStructSimSTD::CalcRglrInt, true , c_ToleranceSSIM_Gaussian); //will fail due to numerical instability of FP-SSIM
  testCalcRglr(xStructSimSTD::CalcRglrFlt, xStructSimSTD::CalcRglrInt, false, c_ToleranceSSIM_Gaussian); //will fail due to numerical instability of FP-SSIM

  testCalcRglr(xStructSimSTD::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, true , c_ToleranceSSIM_Averaged);
  testCalcRglr(xStructSimSTD::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, false, c_ToleranceSSIM_Averaged);

  for(const int32 d : c_Dimms)
  {
    testCalcBlckAvg(xStructSimSTD::CalcBlckAvg, d, true );
    testCalcBlckAvg(xStructSimSTD::CalcBlckAvg, d, false);
  }
}

#if X_SIMD_CAN_USE_SSE
TEST_CASE("xStructSimSSE")
{
  testCalcRglr(xStructSimSSE::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, true , c_ToleranceSSIM_Averaged);
  testCalcRglr(xStructSimSSE::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, false, c_ToleranceSSIM_Averaged);

  for(const int32 d : c_Dimms)
  {
    testCalcBlckAvg(xStructSimSSE::CalcBlckAvg, d, true );
    testCalcBlckAvg(xStructSimSSE::CalcBlckAvg, d, false);
  }
}
#endif //X_SIMD_CAN_USE_SSE

#if X_SIMD_CAN_USE_AVX
TEST_CASE("xStructSimAVX")
{
  testCalcRglr(xStructSimAVX::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, true , c_ToleranceSSIM_Averaged);
  testCalcRglr(xStructSimAVX::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, false, c_ToleranceSSIM_Averaged);

  for(const int32 d : c_Dimms)
  { 
    testCalcBlckAvg(xStructSimAVX::CalcBlckAvg, d, true );
    testCalcBlckAvg(xStructSimAVX::CalcBlckAvg, d, false);
  }
}
#endif //X_SIMD_CAN_USE_AVX

#if X_SIMD_CAN_USE_AVX512
TEST_CASE("xStructSimAVX512")
{
  testCalcRglr(xStructSimAVX512::CalcRglrFlt, xStructSimSTD::CalcRglrFlt, true , c_ToleranceSSIM_Gaussian);
  testCalcRglr(xStructSimAVX512::CalcRglrFlt, xStructSimSTD::CalcRglrFlt, false, c_ToleranceSSIM_Gaussian);
  testCalcRglr(xStructSimAVX512::CalcRglrInt, xStructSimSTD::CalcRglrInt, true , c_ToleranceSSIM_Gaussian);
  testCalcRglr(xStructSimAVX512::CalcRglrInt, xStructSimSTD::CalcRglrInt, false, c_ToleranceSSIM_Gaussian);
  testCalcRglr(xStructSimAVX512::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, true , c_ToleranceSSIM_Averaged);
  testCalcRglr(xStructSimAVX512::CalcRglrAvg, xStructSimSTD::CalcRglrAvg, false, c_ToleranceSSIM_Averaged);

  for(const int32 d : c_Dimms) 
  {
    testCalcBlckAvg(xStructSimAVX512::CalcBlckAvg, d, true );
    testCalcBlckAvg(xStructSimAVX512::CalcBlckAvg, d, false);
  }

  testCalcMultiBlckAvg(xStructSimAVX512::CalcMultiBlckAvg8S4, 8, 4, 7, true );
  testCalcMultiBlckAvg(xStructSimAVX512::CalcMultiBlckAvg8S4, 8, 4, 7, false);

  testCalcMultiBlckAvg(xStructSimAVX512::CalcMultiBlckAvg8S8, 8, 8, 4, true );
  testCalcMultiBlckAvg(xStructSimAVX512::CalcMultiBlckAvg8S8, 8, 8, 4, false);
}
#endif //X_SIMD_CAN_USE_AVX512

#if X_SIMD_CAN_USE_NEON
TEST_CASE("xStructSimNEON")
{
  for(const int32 d : c_Dimms)
  { 
    testCalcBlckAvg(xStructSimNEON::CalcBlckAvg, d, true );
    testCalcBlckAvg(xStructSimNEON::CalcBlckAvg, d, false);
  }
}
#endif //X_SIMD_CAN_USE_NEON

//===============================================================================================================================================================================================================