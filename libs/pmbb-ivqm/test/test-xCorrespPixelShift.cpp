/*
    SPDX-FileCopyrightText: 2019-2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
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
#include "xTestUtilsIVQM.h"
#include "xPlane.h"
#include "xPic.h"
#include "xCorrespPixelShift.h"


using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================

static const std::vector<int32> c_Dimms = { 64, 96, 128 };
static const std::vector<int32> c_BitDs = { 8, 10, 14 };
constexpr int32 c_Margin   = 8;
constexpr int32 c_NumIters = 2;
static const std::vector<int32V4> c_GCDs = { {0,0,0,0}, {1,1,1,0}, {-1,-1,-1,0}, {-1,1,1,0} };
constexpr int32 c_SearchRange = 2;

//===============================================================================================================================================================================================================

void testCalcDistAsymmetricRow(fCalcDistAsymmetricRowP CalcDistAsymmetricRowP, fCalcDistAsymmetricRowI CalcDistAsymmetricRowI)
{
  uint32 State = xTestUtils::c_XorShiftSeed;

  for(const int32 y : c_Dimms)
  {
    for(const int32 x : c_Dimms)
    {
      int32V2 Size = { x, y };

      for(const int32 b : c_BitDs)
      {
        xPicP* OrgP = new xPicP(Size, b, c_Margin);
        xPicP* ModP = new xPicP(Size, b, c_Margin);
        xPicI* OrgI = new xPicI(Size, b, c_Margin);
        xPicI* ModI = new xPicI(Size, b, c_Margin);

        xPerlinNoise::fillPerlinNoiseI(OrgP->getAddr(eCmp::C0), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), 8, State); State = xTestUtils::xXorShift32(State);
        xPerlinNoise::fillPerlinNoiseI(OrgP->getAddr(eCmp::C1), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), 8, State); State = xTestUtils::xXorShift32(State);
        xPerlinNoise::fillPerlinNoiseI(OrgP->getAddr(eCmp::C2), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), 8, State); State = xTestUtils::xXorShift32(State);

        OrgP->extend();

        for(int32 Iter = 0; Iter < c_NumIters; Iter++)
        {
          xTestUtilsIVQM::addBlockNoise(ModP->getAddr(eCmp::C0), OrgP->getAddr(eCmp::C0), ModP->getStride(), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), State);
          if(Iter == 0)
          {
            ModP->copy(OrgP, eCmp::C1);
            ModP->copy(OrgP, eCmp::C2);
          }
          else
          {
            xTestUtilsIVQM::addBlockNoise(ModP->getAddr(eCmp::C1), OrgP->getAddr(eCmp::C1), ModP->getStride(), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), State);
            xTestUtilsIVQM::addBlockNoise(ModP->getAddr(eCmp::C2), OrgP->getAddr(eCmp::C2), ModP->getStride(), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), State);
          }
          ModP->extend();
          OrgI->rearrangeFromPlanar(OrgP);
          ModI->rearrangeFromPlanar(ModP);

          for(int32V4 GCD : c_GCDs)
          {
            uint64V4 RefSSDsA = xTestCalcDistAsymmetricPicP(ModP, OrgP, GCD, 2, { 4,1,1,0 }, static_cast<pCalcDistAsymmetricRowP>(xCorrespPixelShiftSTD::CalcDistAsymmetricRow));
            uint64V4 RefSSDsB = xTestCalcDistAsymmetricPicP(OrgP, ModP, GCD, 2, { 4,1,1,0 }, static_cast<pCalcDistAsymmetricRowP>(xCorrespPixelShiftSTD::CalcDistAsymmetricRow));
            if(CalcDistAsymmetricRowP)
            {
              uint64V4 TstSSDsA = xTestCalcDistAsymmetricPicP(ModP, OrgP, GCD, 2, { 4,1,1,0 }, CalcDistAsymmetricRowP);
              uint64V4 TstSSDsB = xTestCalcDistAsymmetricPicP(OrgP, ModP, GCD, 2, { 4,1,1,0 }, CalcDistAsymmetricRowP);
              CHECK(RefSSDsA == TstSSDsA);
              CHECK(RefSSDsB == TstSSDsB);
            }

            if(CalcDistAsymmetricRowI)
            {
              uint64V4 TstSSDsA = xTestCalcDistAsymmetricPicI(ModI, OrgI, GCD, 2, { 4,1,1,0 }, CalcDistAsymmetricRowI);
              uint64V4 TstSSDsB = xTestCalcDistAsymmetricPicI(OrgI, ModI, GCD, 2, { 4,1,1,0 }, CalcDistAsymmetricRowI);
              CHECK(RefSSDsA == TstSSDsA);
              CHECK(RefSSDsB == TstSSDsB);
            }
          }
        }

        delete OrgP; OrgP = nullptr;
        delete ModP; ModP = nullptr;
        delete OrgI; OrgI = nullptr;
        delete ModI; ModI = nullptr;
      }
    }
  }
}

//===============================================================================================================================================================================================================

void testGenShftCompPic(fGenShftCompRowP GenShftCompRowP, fGenShftCompRowI GenShftCompRowI)
{
  uint32 State = xTestUtils::c_XorShiftSeed;

  for(const int32 y : c_Dimms)
  {
    for(const int32 x : c_Dimms)
    {
      int32V2 Size = { x, y };

      for(const int32 b : c_BitDs)
      {
        xPicP* OrgP = new xPicP(Size, b, c_Margin);
        xPicP* ModP = new xPicP(Size, b, c_Margin);
        xPicI* OrgI = new xPicI(Size, b, c_Margin);
        xPicI* ModI = new xPicI(Size, b, c_Margin);

        xPicP* OrgP_SCP = new xPicP(Size, b, c_Margin);
        xPicP* ModP_SCP = new xPicP(Size, b, c_Margin);
        xPicI* OrgI_SCP = new xPicI(Size, b, c_Margin);
        xPicI* ModI_SCP = new xPicI(Size, b, c_Margin);

        xPerlinNoise::fillPerlinNoiseI(OrgP->getAddr(eCmp::C0), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), 8, State); State = xTestUtils::xXorShift32(State);
        xPerlinNoise::fillPerlinNoiseI(OrgP->getAddr(eCmp::C1), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), 8, State); State = xTestUtils::xXorShift32(State);
        xPerlinNoise::fillPerlinNoiseI(OrgP->getAddr(eCmp::C2), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), 8, State); State = xTestUtils::xXorShift32(State);

        OrgP->extend();

        for(int32 Iter = 0; Iter < c_NumIters; Iter++)
        {
          xTestUtilsIVQM::addBlockNoise(ModP->getAddr(eCmp::C0), OrgP->getAddr(eCmp::C0), ModP->getStride(), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), State);
          if(Iter == 0)
          {
            ModP->copy(OrgP, eCmp::C1);
            ModP->copy(OrgP, eCmp::C2);
          }
          else
          {
            xTestUtilsIVQM::addBlockNoise(ModP->getAddr(eCmp::C1), OrgP->getAddr(eCmp::C1), ModP->getStride(), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), State);
            xTestUtilsIVQM::addBlockNoise(ModP->getAddr(eCmp::C2), OrgP->getAddr(eCmp::C2), ModP->getStride(), OrgP->getStride(), OrgP->getWidth(), OrgP->getHeight(), OrgP->getBitDepth(), State);
          }
          ModP->extend();
          OrgI->rearrangeFromPlanar(OrgP);
          ModI->rearrangeFromPlanar(ModP);

          for(int32V4 GCD : c_GCDs)
          {
            uint64V4 RefSSDsA = xTestCalcDistAsymmetricPicP(ModP, OrgP, GCD, 2, { 4,1,1,0 }, static_cast<pCalcDistAsymmetricRowP>(xCorrespPixelShiftSTD::CalcDistAsymmetricRow));
            uint64V4 RefSSDsB = xTestCalcDistAsymmetricPicP(OrgP, ModP, GCD, 2, { 4,1,1,0 }, static_cast<pCalcDistAsymmetricRowP>(xCorrespPixelShiftSTD::CalcDistAsymmetricRow));

            if(GenShftCompRowP)
            {
              xTestGenShftCompPicP(OrgP_SCP, OrgP, ModP, GCD, 2, { 4,1,1,0 }, GenShftCompRowP);
              xTestGenShftCompPicP(ModP_SCP, ModP, OrgP, GCD, 2, { 4,1,1,0 }, GenShftCompRowP);

              uint64V4 TstSSDsA = xTestUtilsIVQM::calcPicSSD(ModP, OrgP_SCP);
              uint64V4 TstSSDsB = xTestUtilsIVQM::calcPicSSD(OrgP, ModP_SCP);

              CHECK(RefSSDsA == TstSSDsA);
              CHECK(RefSSDsB == TstSSDsB);
            }

            if(GenShftCompRowI)
            {
              xTestGenShftCompPicI(OrgI_SCP, OrgI, ModI, GCD, 2, { 4,1,1,0 }, GenShftCompRowI);
              xTestGenShftCompPicI(ModI_SCP, ModI, OrgI, GCD, 2, { 4,1,1,0 }, GenShftCompRowI);

              uint64V4 TstSSDsA = xTestUtilsIVQM::calcPicSSD(ModI, OrgI_SCP);
              uint64V4 TstSSDsB = xTestUtilsIVQM::calcPicSSD(OrgI, ModI_SCP);

              CHECK(RefSSDsA == TstSSDsA);
              CHECK(RefSSDsB == TstSSDsB);
            }
          }
        }

        delete OrgP; OrgP = nullptr;
        delete ModP; ModP = nullptr;
        delete OrgI; OrgI = nullptr;
        delete ModI; ModI = nullptr;

        delete OrgP_SCP; OrgP_SCP = nullptr;
        delete ModP_SCP; ModP_SCP = nullptr;
        delete OrgI_SCP; OrgI_SCP = nullptr;
        delete ModI_SCP; ModI_SCP = nullptr;
      }
    }
  }

}

//===============================================================================================================================================================================================================

TEST_CASE("xCorrespPixelShiftSTD")
{
  testCalcDistAsymmetricRow(static_cast<pCalcDistAsymmetricRowP>(xCorrespPixelShiftSTD::CalcDistAsymmetricRow), static_cast<pCalcDistAsymmetricRowI>(xCorrespPixelShiftSTD::CalcDistAsymmetricRow));
  testGenShftCompPic       (static_cast<pGenShftCompRowP       >(xCorrespPixelShiftSTD::GenShftCompRow       ), static_cast<pGenShftCompRowI       >(xCorrespPixelShiftSTD::GenShftCompRow       ));
}

#if X_SIMD_CAN_USE_SSE
TEST_CASE("xCorrespPixelShiftSSE")
{
  testCalcDistAsymmetricRow(nullptr, static_cast<pCalcDistAsymmetricRowI>(xCorrespPixelShiftSSE::CalcDistAsymmetricRow));
  testGenShftCompPic       (nullptr, static_cast<pGenShftCompRowI       >(xCorrespPixelShiftSSE::GenShftCompRow       ));
}
#endif //X_SIMD_CAN_USE_SSE

#if X_SIMD_CAN_USE_NEON
TEST_CASE("xCorrespPixelShiftNEON")
{
  testCalcDistAsymmetricRow(nullptr, static_cast<pCalcDistAsymmetricRowI>(xCorrespPixelShiftNEON::CalcDistAsymmetricRow));
  testGenShftCompPic       (nullptr, static_cast<pGenShftCompRowI       >(xCorrespPixelShiftNEON::GenShftCompRow       ));
}
#endif //X_SIMD_CAN_USE_NEON

