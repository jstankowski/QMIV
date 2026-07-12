/*
    SPDX-FileCopyrightText: 2019-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <functional>
#include <random>

#include "xMatrix.h"

using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================
 
template <int32 Size> void TestInversionEye()
{
  xMatrix<Size, Size, flt64> A; 
  xMatrix<Size, Size, flt64> I;
  xMatrix<Size, Size, flt64> R;

  for(int32 i = 1; i < 20; i += 3)
  {
    A.setIdentity();
    R.setIdentity();
    R.modMultiplyByScalar((flt64)1.0 / (flt64)i);
    A.modMultiplyByScalar(i);

    I.zero();
    bool InvertibleCH = xMatrix<Size, Size, flt64>::InvertCholesky(I, A);
    CHECK(InvertibleCH);
    if(i == 1) { CHECK(I.isCloseToIdentity   ( )); }
    else       { CHECK(I.isApproximatelyEqual(R)); }

    I.zero();
    bool InvertibleLU = xMatrix<Size, Size, flt64>::InvertLU(I, A);
    CHECK(InvertibleLU);
    if(i == 1) { CHECK(I.isCloseToIdentity   ( )); }
    else       { CHECK(I.isApproximatelyEqual(R)); }

    I.zero();
    bool InvertibleQR = xMatrix<Size, Size, flt64>::InvertQR(I, A);
    CHECK(InvertibleQR);
    if(i == 1) { CHECK(I.isCloseToIdentity   ( )); }
    else       { CHECK(I.isApproximatelyEqual(R)); }
  }
}

void TestSymmetricPositiveDefinite5x5()
{
  xMatrix<5, 5, flt64> A( { {
    {  6.,  1.,  0.,  0.,  1. },
    {  1.,  6.,  1.,  0.,  0. },
    {  0.,  1.,  6.,  1.,  0. },
    {  0.,  0.,  1.,  6.,  1. },
    {  1.,  0.,  0.,  1.,  6. },
  } });

  xMatrix<5, 5, flt64> R({ {
    {  0.176724138, -0.030172414,  0.004310345,  0.004310345, -0.030172414 },
    { -0.030172414,  0.176724138, -0.030172414,  0.004310345,  0.004310345 },
    {  0.004310345, -0.030172414,  0.176724138, -0.030172414,  0.004310345 },
    {  0.004310345,  0.004310345, -0.030172414,  0.176724138, -0.030172414 },
    { -0.030172414,  0.004310345,  0.004310345, -0.030172414,  0.176724138 },
   } });

  xMatrix<5, 5, flt64> I; 

  flt64 Tolerance = 0.000000001;

  I.zero();
  bool InvertibleCH = xMatrix<5, 5, flt64>::InvertCholesky(I, A);
  CHECK(InvertibleCH);
  CHECK(I.isApproximatelyEqual(R, Tolerance));
  
  I.zero();
  bool InvertibleLU = xMatrix<5, 5, flt64>::InvertLU(I, A);
  CHECK(InvertibleLU);
  CHECK(I.isApproximatelyEqual(R, Tolerance));
  
  I.zero();
  bool InvertibleQR = xMatrix<5, 5, flt64>::InvertQR(I, A);
  CHECK(InvertibleQR);
  CHECK(I.isApproximatelyEqual(R, Tolerance));
}

void TestPseudoRandom5x5()
{
  xMatrix<5, 5, flt64> A({ {
    { 3.900454115,  0.844186643,  0.676514336,  0.727858057,  0.951457957, },
    { 0.012703197,  1.083098557,  0.048812794,  0.099928561,  0.508066306, },
    { 0.200247539,  0.744154169,  2.131366569,  0.700844752,  0.293228106, },
    { 0.774479454,  0.005108839,  0.112857654,  1.251067847,  0.247668229, },
    { 0.023236299,  0.727321154,  0.340034942,  0.197503156,  2.197275145, },
  } });

  xMatrix<5, 5, flt64> R({ {
  {  0.281337147, -0.122335369, -0.069431651, -0.103551007, -0.072599184, },
  {  0.009207524,  1.071872400,  0.015142528, -0.060455753, -0.247037823, },
  {  0.028059203, -0.374821727,  0.476184212, -0.259492335,  0.040220066, },
  { -0.177844538,  0.166631905,  0.015678182,  0.890799308, -0.064019486, },
  {  0.005620456, -0.310480321, -0.079378274, -0.018806167,  0.537179241, },
  } });

  xMatrix<5, 5, flt64> I;

  flt64 Tolerance = 0.000000001;

  I.zero();
  bool InvertibleLU = xMatrix<5, 5, flt64>::InvertLU(I, A);
  CHECK(InvertibleLU);
  CHECK(I.isApproximatelyEqual(R, Tolerance));

  I.zero();
  bool InvertibleQR = xMatrix<5, 5, flt64>::InvertQR(I, A);
  CHECK(InvertibleQR);
  CHECK(I.isApproximatelyEqual(R, Tolerance));
}

//===============================================================================================================================================================================================================

TEST_CASE("InversionEye")
{
  TestInversionEye< 3>();
  TestInversionEye< 4>();
  TestInversionEye< 5>();
  TestInversionEye< 6>();
  TestInversionEye< 7>();
  TestInversionEye< 8>();
  TestInversionEye< 9>();
  TestInversionEye<10>();
  TestInversionEye<11>();
  TestInversionEye<12>();
  TestInversionEye<13>();
  TestInversionEye<14>();
  TestInversionEye<15>();
  TestInversionEye<16>();
  TestInversionEye<17>();
  TestInversionEye<18>();
}

TEST_CASE("SymmetricPositiveDefinite5x5")
{
  TestSymmetricPositiveDefinite5x5();
}

TEST_CASE("PseudoRandom5x5")
{
  TestPseudoRandom5x5();
}

//===============================================================================================================================================================================================================

