/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xGSR_Renderer.h"
#include "xGSR_Projector.h"
#include "xGSR_Rasterizer.h"
#include "xTimeUtils.h"
#include <numeric>

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

void xRenderer::create(int32V2 PictureSize, xThreadPool* ThreadPool)
{
  m_PicSize   = PictureSize;
  m_NumTilesX = xNumUnitsCoveringLength(PictureSize.getX(), c_Log2TileSize);
  m_NumTilesY = xNumUnitsCoveringLength(PictureSize.getY(), c_Log2TileSize);
  m_NumTiles  = m_NumTilesX * m_NumTilesY;

  int32 MaxNumTasks = xMax(4096, m_NumTiles);
  createThrdPoolIntf(ThreadPool, MaxNumTasks);

  m_RenderBuf.create(PictureSize, 0);

  m_Tiles.resize(m_NumTiles);
}
void xRenderer::destroy()
{
  m_RenderBuf.destroy();
  m_Tiles.clear();
  destroyThrdPoolIntf();
}
std::string xRenderer::formatAndResetStats(const std::string Prefix, flt64 TicksPerMilliSec)
{
  if(!m_GatherTimeStats) { return "Time stats gathering is disabled!"; }
  if(!m_TotalSynthIters) { return "No time stats gathered!";           }

  flt64 InvDenom = TicksPerMilliSec == 0.0 ? (flt64)1.0 / ((flt64)m_TotalSynthIters) : (flt64)1.0 / ((flt64)m_TotalSynthIters * TicksPerMilliSec);

  std::string TimeStats; TimeStats.reserve(xMemory::getBestEffortSizePageBase());
  TimeStats += Prefix + fmt::format("Processing time {}\n", TicksPerMilliSec == 0.0 ? "[ticks]" : "[ms]");

  TimeStats += Prefix + "  " + fmt::format("SynthIters  = {}\n"    , m_TotalSynthIters);
  TimeStats += Prefix + "  " + fmt::format("TotalFrame  = {:.2f}\n", m_TicksTotalFrame * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Project     = {:.2f}\n", m_Ticks___Project * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Sort        = {:.2f}\n", m_Ticks______Sort * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Assign      = {:.2f}\n", m_Ticks____Assign * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Rasterize   = {:.2f}\n", m_Ticks_Rasterize * InvDenom);
  TimeStats += Prefix + "  " + fmt::format("Convert     = {:.2f}\n", m_Ticks___Convert * InvDenom);

  m_TotalSynthIters = 0;
  m_TicksTotalFrame = 0;
  m_Ticks___Project = 0;
  m_Ticks______Sort = 0;
  m_Ticks____Assign = 0;
  m_Ticks_Rasterize = 0;
  m_Ticks___Convert = 0;

  return TimeStats;
}
void xRenderer::renderFrame(xPicP* DstPic, const tGaussC* Cloud, const tCamPar& CamParams)
{
  assert(DstPic != nullptr && Cloud != nullptr);

  const uint64 T = m_GatherTimeStats ? xTSC() : 0;

  m_Cloud     = Cloud    ;
  m_CamParams = CamParams;

  if(m_ThPI->isActive()) { xRenderFrameMT(DstPic); }
  else                   { xRenderFrameST(DstPic); }

  if(m_GatherTimeStats) { m_TicksTotalFrame += (xTSC() - T); m_TotalSynthIters++; }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void xRenderer::xRenderFrameST(xPicP* DstPic)
{
  const uintSize NumSplats = m_Cloud->getNumSplats();
  if(m_ValidMask.size() < NumSplats) { m_ValidMask.resize(NumSplats); }
  m_ProjectedI.upsize(NumSplats);
  DstPic->clear();

  const uint64 T0 = m_GatherTimeStats ? xTSC() : 0;

  xProjectI();

  const uint64 T1 = m_GatherTimeStats ? xTSC() : 0;

  xSortI(&m_ProjectedI, m_ValidMask.data(), NumSplats);

  const uint64 T2 = m_GatherTimeStats ? xTSC() : 0;

  xRasterizer::RasterizePic(&m_RenderBuf, m_ProjectedI, m_CapEffAlpha);

  const uint64 T3 = m_GatherTimeStats ? xTSC() : 0;

  xConvertToPicP(DstPic, &m_RenderBuf);

  const uint64 T4 = m_GatherTimeStats ? xTSC() : 0;

  if(m_GatherTimeStats)
  {
    m_Ticks___Project += T1 - T0;
    m_Ticks______Sort += T2 - T1;
    m_Ticks_Rasterize += T3 - T2;
    m_Ticks___Convert += T4 - T3;
  }
}
void xRenderer::xRenderFrameMT(xPicP* DstPic)
{
  int32 NumSplats = (int32)m_Cloud->getNumSplats();
  if(m_ValidMask.size() < NumSplats) { m_ValidMask.resize(NumSplats); }
  m_ProjectedP.upsize(NumSplats);
  DstPic->clear();

  const uint64 T0 = m_GatherTimeStats ? xTSC() : 0;

  xProjectP();

  const uint64 T1 = m_GatherTimeStats ? xTSC() : 0;

  xSortP(&m_ProjectedP, m_ValidMask.data(), NumSplats);

  const uint64 T2 = m_GatherTimeStats ? xTSC() : 0;

  xAssignTile(m_ProjectedP);

  const uint64 T3 = m_GatherTimeStats ? xTSC() : 0;

  for(int32 TY = 0; TY < m_NumTilesY; TY++)
  {
    for(int32 TX = 0; TX < m_NumTilesX; TX++)
    {
      const int32 TileIdx = xTileIdx(TX, TY, m_NumTilesX);
      m_ThPI->storeTask([this, DstPic, TX, TY, TileIdx](int32)
        {
          xRasterizer::RasterizeTile(DstPic, m_ProjectedP, m_Tiles[TileIdx], { TX, TY }, m_CapEffAlpha);
        });
    }
  }
  m_ThPI->executeStoredTasks();

  const uint64 T4 = m_GatherTimeStats ? xTSC() : 0;

  if(m_GatherTimeStats)
  {
    m_Ticks___Project += T1 - T0;
    m_Ticks______Sort += T2 - T1;
    m_Ticks____Assign += T3 - T2;
    m_Ticks_Rasterize += T4 - T3;
  }
}
void xRenderer::xProjectI()
{
  const int32 NumSplats = m_Cloud->getNumSplats();
  if(m_ThPI->isActive())
  {
    const int32 MaxNumTasks   = m_ThPI->getMaxNumTasksInBatch();
    const int32 DefNumBatches = (int32)ceil((flt32)NumSplats / (flt32)c_DefProjBatchSize);
    const int32 SafeBatchSize = (int32)ceil((flt32)NumSplats / (flt32)MaxNumTasks);
    const int32 BatchSize     = DefNumBatches < MaxNumTasks ? c_DefProjBatchSize : SafeBatchSize;

    for(int32 BegIdx = 0; BegIdx < NumSplats; BegIdx += BatchSize)
    {
      const int32 EndIdx = xMin(BegIdx + BatchSize, NumSplats);
      m_ThPI->storeTask([this, BegIdx, EndIdx](int32 /*ThreadIdx*/) { xProjector::ProjectRng(&m_ProjectedI, m_ValidMask.data(), m_Cloud, m_CamParams, m_PicSize, BegIdx, EndIdx); });
    }
    m_ThPI->executeStoredTasks();
  }
  else
  {
    xProjector::ProjectRng(&m_ProjectedI, m_ValidMask.data(), m_Cloud, m_CamParams, m_PicSize, 0, NumSplats);
  }
}
void xRenderer::xProjectP()
{
  const int32 NumSplats = m_Cloud->getNumSplats();

  if(m_ThPI->isActive())
  {
    const int32 MaxNumTasks   = m_ThPI->getMaxNumTasksInBatch();
    const int32 DefNumBatches = (int32)ceil((flt32)NumSplats / (flt32)c_DefProjBatchSize);
    const int32 SafeBatchSize = (int32)ceil((flt32)NumSplats / (flt32)MaxNumTasks);
    const int32 BatchSize     = DefNumBatches < MaxNumTasks ? c_DefProjBatchSize : SafeBatchSize;

    // Submit projection tasks in batches of c_SplatBatchSize
    for(int32 BegIdx = 0; BegIdx < NumSplats; BegIdx += BatchSize)
    {
      const int32 EndIdx = xMin(BegIdx + BatchSize, NumSplats);
      m_ThPI->storeTask([this, BegIdx, EndIdx](int32 /*ThreadIdx*/) { xProjector::ProjectRng(&m_ProjectedP, m_ValidMask.data(), m_Cloud, m_CamParams, m_PicSize, BegIdx, EndIdx); });
    }
    m_ThPI->executeStoredTasks();
  }
  else
  {
    xProjector::ProjectRng(&m_ProjectedP, m_ValidMask.data(), m_Cloud, m_CamParams, m_PicSize, 0, NumSplats);
  }
}
void xRenderer::xSortP(xProjectedCloudP* Projected, const uint8* ValidMask, uint32 TotalNumSplats)
{
  int32* SortedIdx = Projected->getSortedIdx();
  const flt32* Depth = Projected->getDepth();

  //collect valid indices
  uint32 NumValid = 0;
  for(uint32 i = 0; i < TotalNumSplats; i++)
  {
    if(ValidMask[i]) { SortedIdx[NumValid++] = i; }
  }
  Projected->setNumValid(NumValid);

  if(NumValid < 2) { return; }

  // sort front-to-back: ascending depth (nearest first)
  std::sort(SortedIdx, SortedIdx + NumValid, [Depth](uint32 a, uint32 b) { return Depth[a] < Depth[b]; }); // smaller Z = nearer to camera = rendered first
}
void xRenderer::xSortI(xProjectedCloudI* Projected, const uint8* ValidMask, uint32 TotalNumSplats)
{
  int32* SortedIdx = Projected->getSortedIdx();
  const xProjectedSplatData* ProjectedData = Projected->access();

  //collect valid indices
  uint32 NumValid = 0;
  for(uint32 i = 0; i < TotalNumSplats; i++)
  {
    if(ValidMask[i]) { SortedIdx[NumValid++] = i; }
  }
  Projected->setNumValid(NumValid);

  if(NumValid < 2) { return; }

  // sort front-to-back: ascending depth (nearest first)
  std::sort(SortedIdx, SortedIdx + NumValid, [ProjectedData](uint32 a, uint32 b) {  return ProjectedData[a].Depth < ProjectedData[b].Depth; }); // smaller Z = nearer to camera = rendered first
}
void xRenderer::xAssignTile(const xProjectedCloudP& Projected)
{
  for(int32 T = 0; T < m_NumTiles; T++) { m_Tiles[T].clear(); } //xClearTiles

  const int32  NumValid  = Projected.getNumValid ();
  const int32* SortedIdx = Projected.getSortedIdx();
  const int32* AABBMinX  = Projected.getAABBMinX ();
  const int32* AABBMinY  = Projected.getAABBMinY ();
  const int32* AABBMaxX  = Projected.getAABBMaxX ();
  const int32* AABBMaxY  = Projected.getAABBMaxY ();

  // Iterate in front-to-back sorted order so per-tile lists stay sorted.
  for(int32 i = 0; i < NumValid; i++)
  {
    const int32 PojectedIdx = SortedIdx[i];

    // TODO -  if AABB are non-negative replace "/ c_TileSize" by ">> c_Log2TileSize"

    // Convert pixel AABB to tile AABB
    const int32 TileX0 = AABBMinX[PojectedIdx] / c_TileSize;
    const int32 TileY0 = AABBMinY[PojectedIdx] / c_TileSize;
    const int32 TileX1 = std::min(m_NumTilesX - 1, AABBMaxX[PojectedIdx] / c_TileSize);
    const int32 TileY1 = std::min(m_NumTilesY - 1, AABBMaxY[PojectedIdx] / c_TileSize);

    for(int32 TY = TileY0; TY <= TileY1; TY++)
    {
      for(int32 TX = TileX0; TX <= TileX1; TX++)
      {
        const int32 TileIdx = xTileIdx(TX, TY, m_NumTilesX);
        m_Tiles[TileIdx].push_back(PojectedIdx);
      }
    }
  }

  if(m_PrintDebug)
  {
    uint64 SumNumSplats = 0;
    uint64 MinNumSplats = std::numeric_limits<uint64>::max();
    uint64 MaxNumSplats = 0;
    for(const tTileSplatIdxV& Tile : m_Tiles)
    {
      SumNumSplats += Tile.size();
      MinNumSplats = xMin(MinNumSplats, Tile.size());
      MaxNumSplats = xMax(MaxNumSplats, Tile.size());
    }
    flt32 AvgNumSplats = (flt32)SumNumSplats / (flt32)m_NumTiles;
    fmt::print("TileStats Size={} Num={} Splats Avg={} Min={} Max={}\n", c_TileSize, m_NumTiles, AvgNumSplats, MinNumSplats, MaxNumSplats);
  }
}
void xRenderer::xConvertToPicP(xPicP* DstPic, const tRstBuf* SrcBuf)
{
  const int32  Width     = DstPic->getWidth ();
  const int32  Height    = DstPic->getHeight();
  const int32  SrcStride = SrcBuf->getStride();
  const int32  DstStride = DstPic->getStride();
  const int32  MaxVal    = xBitDepth2MaxValue(DstPic->getBitDepth());

  for(int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
  {
    const flt32*     SrcPtr = SrcBuf->getAddr((eCmp)CmpIdx);
    uint16* restrict DstPtr = DstPic->getAddr((eCmp)CmpIdx);

    for(int32 y = 0; y < Height; y++)
    {
      for(int32 x = 0; x < Width; x++)
      {
        // TODO - std::round is slow, use PMBB fast rounding routines

        DstPtr[x] = (uint16)xClip((int32)std::round(SrcPtr[x] * (flt32)MaxVal), 0, MaxVal);
      }
      SrcPtr += SrcStride;
      DstPtr += DstStride;
    }
  }
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR
