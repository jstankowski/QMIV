/*
    SPDX-FileCopyrightText: 2025 Patrycja Kaźmierczak <patrycja.kazmierczak@student.put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefIVQM.h"
#include "xPic.h"

#if X_SIMD_CAN_USE_NEON

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

class xCorrespPixelShiftNEON
{
  //asymetric Q interleaved
public:
  static uint64V4 CalcDistAsymmetricRow(const xPicI* Tst, const xPicI* Ref, const int32 y, const int32V4& GlobalColorShift, const int32 SearchRange, const int32V4& CmpWeights);
protected:
  static int32x4_t xCalcDistWithinBlock(const int32x4_t& TstPel, const xPicI* Ref, const int32 CenterX, const int32 CenterY, const int32 SearchRange, const int32x4_t& CmpWeights);

  //shift-compensated picture generation - TODO
public:
  static void GenShftCompRow(xPicI* DstRef, const xPicI* Ref, const xPicI* Tst, const int32 y, const int32V4& GlobalColorShift, const int32 SearchRange, const int32V4& CmpWeights);
protected:
  static inline int32x4_t xFindBestPixelWithinBlock(const int32x4_t& TstPelI32V, const xPicI* Ref, const int32 CenterX, const int32 CenterY, const int32 SearchRange, const int32x4_t& CmpWeightsI32V);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB

#endif //X_SIMD_CAN_USE_NEON
