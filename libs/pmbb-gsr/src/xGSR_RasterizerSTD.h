/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xGSR_RasterizerCmn.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

class xRasterizerSTD : public xRasterizerCmn
{
public:
  static void RasterizeTile(xPicP*   DstPic, tPicM* DstMsk, const xProjectedCloud& Projected, const tSplIdxV& SplatIndicesTile, int32V2 TileIdxXY);

  static void RasterizePic (tRstBuf* DstPic,                const xProjectedCloud& Projected                                                     );
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR
