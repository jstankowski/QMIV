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

class xRasterizerSTD
{
public:
  static void RasterizeTile(xPicP* DstPic, const xProjectedCloudP& Projected, const tSplIdxV& SplatIndicesTile, int32V2 TileIdxXY, bool CapEffectiveAlpha);

  static void RasterizePic (tRstBuf* DstPic, const xProjectedCloudI& Projected,                                                      bool CapEffectiveAlpha);
  static void RasterizePic (tRstBuf* DstPic, const xProjectedCloudP& Projected,                                                      bool CapEffectiveAlpha);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR
