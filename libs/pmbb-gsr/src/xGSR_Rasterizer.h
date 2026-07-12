/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xProjectedCloud.h"

// portable implementation - always included
#include "xGSR_RasterizerSTD.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

class xRasterizer
{
public:
  static inline void RasterizeTile(xPicP* DstPic, const xProjectedCloudP& Projected, const tSplIdxV& SplatIndicesTile, int32V2 TileIdxXY, bool CapEffectiveAlpha)
  {
    xRasterizerSTD::RasterizeTile(DstPic, Projected, SplatIndicesTile, TileIdxXY, CapEffectiveAlpha);
  }
  static inline void RasterizePic(tRstBuf* DstPic, const xProjectedCloudI& Projected, bool CapEffectiveAlpha)
  {
    xRasterizerSTD::RasterizePic(DstPic, Projected, CapEffectiveAlpha);
  }
  static inline void RasterizePic(tRstBuf* DstPic, const xProjectedCloudP& Projected, bool CapEffectiveAlpha)
  {
    xRasterizerSTD::RasterizePic(DstPic, Projected, CapEffectiveAlpha);
  }
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR

