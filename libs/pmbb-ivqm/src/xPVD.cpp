/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#include "xPVD.h"
#include "xDistortion.h"
#include "xPixelOps.h"
#include <cassert>
#include <numeric>

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================
// xPVD
//===============================================================================================================================================================================================================
flt64 xPVD::calcPicPVD(const xPicP* Tst, const xPicP* Ref)
{
  assert(Ref != nullptr && Tst != nullptr);
  assert(Ref->isCompatible(Tst));

  flt64V4 VARs = xCalcPicVAR(Tst, Ref);

  const int32V4 CmpWeightsAverage = c_DefaultCmpWeights;
  const int32   SumCmpWeight      = CmpWeightsAverage.getSum();
  const flt64   CmpWeightInvDenom = 1.0 / (flt64)SumCmpWeight;
  const flt64   MidVal            = (flt64)xBitDepth2MidValue(Ref->getBitDepth());

  flt64 CmbVAR = (VARs * (flt64V4)CmpWeightsAverage).getSum() * CmpWeightInvDenom;
  flt64 PVD    = (flt64)MidVal / (CmbVAR + (flt64)MidVal);
  return PVD;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

flt64V4 xPVD::xCalcPicVAR(const xPicP* Tst, const xPicP* Ref)
{
  flt64V4 VARs = xMakeVec4(0.0);
  for(int32 CmpIdx = 0; CmpIdx < m_NumComponents; CmpIdx++)
  {
    m_ThPI->storeTask([&VARs, &Tst, &Ref, CmpIdx](int32 /**/) { VARs[CmpIdx] = xCalcCmpVAR(Tst, Ref, (eCmp)CmpIdx); });
  }
  m_ThPI->executeStoredTasks();
  return VARs;
}
flt64 xPVD::xCalcCmpVAR(const xPicP* Tst, const xPicP* Ref, eCmp CmpId)
{
  const int32   Width     = Ref->getWidth   ();
  const int32   Height    = Ref->getHeight  ();
  const int64   Area      = Ref->getArea    ();
  const uint16* TstPtr    = Tst->getAddr    (CmpId);
  const uint16* RefPtr    = Ref->getAddr    (CmpId);
  const int32   TstStride = Tst->getStride  ();
  const int32   RefStride = Ref->getStride  ();
  const int32   BitDepth  = Ref->getBitDepth();

  int64  SD  = 0;
  uint64 SSD = 0;
  std::tie(SD, SSD) = xDistortion::CalcSSS(TstPtr, RefPtr, TstStride, RefStride, Width, Height, BitDepth);

  flt64 AvgSSD = (flt64)SSD / (flt64)Area;
  flt64 AvgSD  = (flt64) SD / (flt64)Area;

  flt64 VAR = AvgSSD - xPow2(AvgSD);
  return VAR;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB