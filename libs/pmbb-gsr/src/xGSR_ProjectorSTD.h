/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xProjectedCloud.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

class xProjectorSTD
{
public:
  template<class tProjectedCloud>
  static void ProjectRng(tProjectedCloud* Projected, uint8* ValidMask, const tGaussC* Cloud, const tCamPar& CamParams, int32V2 PicSize, uint32 BegIdx, uint32 EndIdx);

  // Evaluates spherical harmonics at the given view direction. Falls back to DC-only when SphHarmDegree == 0.
  static flt32V3 xEvalSphericalHarmonics(const tGaussC* Cloud, uint32 SplatIdx, flt32V3 ViewDir);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB::GSR
