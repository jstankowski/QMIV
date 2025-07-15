/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "xCommonDefCORE.h"
#include "xHelpersFLT.h"
#include <cfenv>

using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================

uint32 xRefFastLog2(uint32 Value)
{
  for(uint32 i = 31; i >= 0; i--)
  {
    if(Value & (1 << i)) { return i; }
  }
  return std::numeric_limits<uint32>::max(); // Undefined
}

void testFastLog2()
{
  for(uint32 i = 1; i < 65536; i++)
  {
    uint32 Ref = xRefFastLog2(i);
    uint32 Tst = xFastLog2   (i);
    CHECK(Ref == Tst);
  }

  for(uint32 p = 0; p < 32; p++)
  {
    uint32 i = 1 << p;
    uint32 Ref = xRefFastLog2(i);
    uint32 Tst = xFastLog2   (i);
    CHECK(Ref == Tst);
  }
}

//===============================================================================================================================================================================================================

void testRounding(bool Verbose)
{
#if X_PMBB_ARCH_AMD64
  uint32 RoundingModeSSE = _MM_GET_ROUNDING_MODE();
  if(Verbose)
  {
    switch(RoundingModeSSE)
    {
      case _MM_ROUND_NEAREST    : fmt::print("RoundingMode = _MM_ROUND_NEAREST    \n"); break;
      case _MM_ROUND_DOWN       : fmt::print("RoundingMode = _MM_ROUND_DOWN       \n"); break;
      case _MM_ROUND_UP         : fmt::print("RoundingMode = _MM_ROUND_UP         \n"); break;
      case _MM_ROUND_TOWARD_ZERO: fmt::print("RoundingMode = _MM_ROUND_TOWARD_ZERO\n"); break;
    }
  }
  CHECK(RoundingModeSSE == _MM_ROUND_NEAREST);
#endif

  int32 RoundingModeSTD = std::fegetround();
  if(Verbose)
  {
    switch(RoundingModeSTD)
    {
      case FE_TONEAREST : fmt::print("RoundingMode = FE_TONEAREST \n"); break;
      case FE_UPWARD    : fmt::print("RoundingMode = FE_UPWARD    \n"); break;
      case FE_DOWNWARD  : fmt::print("RoundingMode = FE_DOWNWARD  \n"); break;
      case FE_TOWARDZERO: fmt::print("RoundingMode = FE_TOWARDZERO\n"); break;
    }
  }
  CHECK(RoundingModeSTD == FE_TONEAREST);

  for(int32 i = -65536; i < 65536; i++)
  {
    flt32 F32 = (flt32)i / (flt32)1000;
    flt64 F64 = (flt64)i / (flt64)1000;

    int64 RefNI32 = (int64)nearbyint(F32);
    int64 RefNI64 = (int64)nearbyint(F64);
    int64 RefRI32 = (int64)round(F32);
    int64 RefRI64 = (int64)round(F64);

    int64 TstNI32 = (int64)xNearbyF32ToI32(F32);
    int64 TstNI64 = (int64)xNearbyF64ToI32(F64);
    int64 TstRI32 = (int64)xRoundF32ToI32 (F32);
    int64 TstRI64 = (int64)xRoundF64ToI32 (F64);

    CHECK(TstNI32 == RefNI32);
    CHECK(TstNI64 == RefNI64);
    CHECK(TstRI32 == RefRI32);
    CHECK(TstRI64 == RefRI64);

    if(i >= 0)
    {
      int64 RefNU32 = xClipU((int64)nearbyint(F32), (int64)std::numeric_limits<uint32>::max());
      int64 RefNU64 = xClipU((int64)nearbyint(F64), (int64)std::numeric_limits<uint32>::max());
      int64 RefRU32 = xClipU((int64)round    (F32), (int64)std::numeric_limits<uint32>::max());
      int64 RefRU64 = xClipU((int64)round    (F64), (int64)std::numeric_limits<uint32>::max());

      int64 TstNU32 = (int64)xNearbyF32ToU32(F32);
      int64 TstNU64 = (int64)xNearbyF64ToU32(F64);
      int64 TstRU32 = (int64)xRoundF32ToU32 (F32);
      int64 TstRU64 = (int64)xRoundF64ToU32 (F64);

      CHECK(TstNU32 == RefNU32);
      CHECK(TstNU64 == RefNU64);
      CHECK(TstRU32 == RefRU32);
      CHECK(TstRU64 == RefRU64);
    }
  }
}

//===============================================================================================================================================================================================================

TEST_CASE("testFastLog2")
{
  testFastLog2();
}

TEST_CASE("testRounding")
{
  testRounding(false);
}

//===============================================================================================================================================================================================================
