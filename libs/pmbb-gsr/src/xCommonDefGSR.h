/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>    
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefCORE.h"
#include "xGaussianCloud.h"
#include "xCameraParams.h"
#include "xBfrPic.h"
#include "xString.h"
#include <string_view>

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================
// Constants
//===============================================================================================================================================================================================================
static constexpr int32 c_Log2TileSize     = 5;             // log2(TileSize) 
static constexpr int32 c_TileSize         = 1<<c_Log2TileSize; 
static constexpr int32 c_TileArea         = c_TileSize*c_TileSize;
static constexpr int32 c_DefProjBatchSize = 512;
//projection/rasterization constants
static constexpr flt32 c_CullSigmas        = 3.0f;          // ellipse radius in sigma units for culling/AABB
static constexpr flt32 c_AlphaThreshold    = 1.0f / 255.0f; // minimum effective alpha to splat
static constexpr flt32 c_PowerThreshold    = -5.545f;       // ln(c_AlphaThreshold); skip exp() when weight * alpha_max < threshold
static constexpr flt32 c_AccAlphaSat       = 0.9999f;       // accumulated alpha saturation (early exit)
static constexpr flt32 c_CovRegularize     = 0.3f;          // regularization added to diagonal of Sigma_2D
static constexpr flt32 c_EffectiveAlphaCap = 0.999f;        // MGM cap effective alpha cap - 0.999
//projection/rasterization parameters
static constexpr bool  c_ProjClamping   = true;
static constexpr bool  c_PelCenter0o5   = true;
static constexpr bool  c_TileTerminate  = false; //early tile termination based on c_AccAlphaSat - to check if it is beneficial

//===============================================================================================================================================================================================================
// Types
//===============================================================================================================================================================================================================
using tGaussC  = PMBB_NAMESPACE::GSC::xGaussianCloud;
using tCamPar  = xCameraParams<flt64>;
using tRstBuf  = xBfrPicP<flt32, 4>; //rasterizer buffer
using tValidV  = std::vector<uint8 >;
using tSplIdxV = std::vector<int32>;

using tTileSplatIdxV = std::vector<int32>;
using tTileSplatIdxM = std::vector<tTileSplatIdxV>;


//===============================================================================================================================================================================================================
// Utils
//===============================================================================================================================================================================================================

static inline int32 calcNumTiles(int32V2 PictureSize)
{
  const int32 NumTilesX = (PictureSize.getX() + c_TileSize - 1) >> c_Log2TileSize;
  const int32 NumTilesY = (PictureSize.getY() + c_TileSize - 1) >> c_Log2TileSize;
  return NumTilesX * NumTilesY;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR
