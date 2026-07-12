/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xGaussianCloud.h"
#include "xProjectedCloud.h"
#include "xCameraParams.h"
#include "xMultiThreaded.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

class xRenderer : public xMultiThreaded
{
public:
  void create        (int32V2 PicSize, xThreadPool* ThreadPool = nullptr);
  void destroy       ();
  void setCapEffAlpha(bool CapEffAlpha) { m_CapEffAlpha = CapEffAlpha; }
  void renderFrame   (xPicP* DstPic, const tGaussC* Cloud, const tCamPar& CamParams);

  void  setGatherTimeStats(bool GatherTimeStats)       { m_GatherTimeStats = GatherTimeStats; }
  bool  getGatherTimeStats(                    ) const { return m_GatherTimeStats;            }

protected:
  int32V2        m_PicSize     = { NOT_VALID, NOT_VALID };
  int32          m_NumTilesX   = NOT_VALID;
  int32          m_NumTilesY   = NOT_VALID;
  int32          m_NumTiles    = NOT_VALID;
  tTileSplatIdxM m_Tiles; // [NumTilesX * NumTilesY]
  bool           m_CapEffAlpha = true;

  const tGaussC*   m_Cloud       = nullptr;
  tCamPar          m_CamParams ;
  xProjectedCloudI m_ProjectedI;
  xProjectedCloudP m_ProjectedP;
  tValidV          m_ValidMask ;
  tRstBuf          m_RenderBuf ;

protected:
  bool   m_PrintDebug      = false;
  bool   m_GatherTimeStats = false;
  int64  m_TotalSynthIters = 0;
  uint64 m_TicksTotalFrame = 0;
  uint64 m_Ticks___Project = 0;
  uint64 m_Ticks______Sort = 0;
  uint64 m_Ticks____Assign = 0;
  uint64 m_Ticks_Rasterize = 0;  
  uint64 m_Ticks___Convert = 0;  

public:
  std::string formatAndResetStats(const std::string Prefix, flt64 TicksPerMilliSec);

protected:
  void xRenderFrameST(xPicP* DstPic);
  void xRenderFrameMT(xPicP* DstPic);

  void xProjectI();
  void xProjectP();

  static void xSortP(xProjectedCloudP* Projected, const uint8* ValidMask, uint32 TotalNumSplats);
  static void xSortI(xProjectedCloudI* Projected, const uint8* ValidMask, uint32 TotalNumSplats);

  void xAssignTile(const xProjectedCloudP& Projected);
  static inline int32 xTileIdx(int32 TX, int32 TY, int32 NumTilesX) { return TY * NumTilesX + TX; }

  static void xConvertToPicP(xPicP* DstPic, const tRstBuf* SrcBuf);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR
