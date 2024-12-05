/*
    SPDX-FileCopyrightText: 2019-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#include "xSSIM.h"
#include "xKBNS.h"
#include "xPixelOps.h"
#include "xString.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================
// xSSIM
//===============================================================================================================================================================================================================
xSSIM::eMode xSSIM::xStrToMode(const std::string& Mode)
{
  std::string ModeL = xString::toLower(Mode);
  return ModeL == "regulargaussianflt" ? eMode::RegularGaussianFlt :
         ModeL == "regulargaussianint" ? eMode::RegularGaussianInt :
         ModeL == "regularaveraged"    ? eMode::RegularAveraged    :
         ModeL == "blockgaussianint"   ? eMode::BlockGaussianInt   :
         ModeL == "blockaveraged"      ? eMode::BlockAveraged      :
                                         eMode::INVALID;
}
std::string xSSIM::xModeToStr(eMode Mode)
{
  return Mode == eMode::RegularGaussianFlt ? "RegularGaussianFlt" :
         Mode == eMode::RegularGaussianInt ? "RegularGaussianInt" :
         Mode == eMode::RegularAveraged    ? "RegularAveraged"    :
         Mode == eMode::BlockGaussianInt   ? "BlockGaussianInt"   :
         Mode == eMode::BlockAveraged      ? "BlockAveraged"      :
                                             "INVALID";
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void xSSIM::create(int32V2 PicSize, int32 BitDepth, int32 /*Margin*/, bool EnableMS)
{
  m_PicSize   = PicSize;
  m_BitDepth  = BitDepth;
  m_Mode      = eMode::INVALID;
  m_IsRegular = true;
  m_WndSize   = NOT_VALID;
  m_WndStride = NOT_VALID;

  flt64 MaxValue = (flt64)xBitDepth2MaxValue(BitDepth);
  m_C1 = xPow2(c_K1<flt64> * MaxValue);
  m_C2 = xPow2(c_K2<flt64> * MaxValue);

  for(int32 CmpIdx = 0; CmpIdx < 4; CmpIdx++)
  { 
    m_RowSums[CmpIdx].resize(m_PicSize.getY(), 0.0);
  }

  if(EnableMS)
  {
    int32V2 LastSize = m_PicSize;
    for(int32 i = 1; i < c_NumMultiScales; i++)
    {
      int32V2 NewSize = LastSize >> 1;
      m_SubPicTst[i] = new xPicP(NewSize, BitDepth, 0);
      m_SubPicRef[i] = new xPicP(NewSize, BitDepth, 0);
      LastSize = NewSize;
    }
  }
}
void xSSIM::destroy()
{
  m_PicSize = { NOT_VALID, NOT_VALID };
  m_Mode    = eMode::INVALID;
  m_C1      = std::numeric_limits<flt64>::quiet_NaN();
  m_C2      = std::numeric_limits<flt64>::quiet_NaN();

  for(int32 i = 1; i < c_NumMultiScales; i++)
  {
    if(m_SubPicTst[i]) { m_SubPicTst[i]->destroy(); delete m_SubPicTst[i]; m_SubPicTst[i] = nullptr; }
    if(m_SubPicRef[i]) { m_SubPicRef[i]->destroy(); delete m_SubPicRef[i]; m_SubPicRef[i] = nullptr; }
  }
}
void xSSIM::setStructSimParams(eMode Mode, bool UseMargin, int32 BlockSize, int32 WndStride)
{
  m_Mode      = Mode;  
  m_IsRegular = isRegularMode      (m_Mode           );
  m_UseMargin = m_IsRegular ? UseMargin : false;
  m_WndSize   = determineWindowSize(m_Mode, BlockSize);
  m_WndStride = WndStride;

  switch(m_Mode)
  {
  case eMode::RegularGaussianFlt: m_CalcPtr = xStructSim::CalcRglrFlt; break;
  case eMode::RegularGaussianInt: m_CalcPtr = xStructSim::CalcRglrInt; break;
  case eMode::RegularAveraged   : m_CalcPtr = xStructSim::CalcRglrAvg; break;
  case eMode::BlockGaussianInt  : m_CalcPtr = xStructSim::CalcBlckInt; break;
  case eMode::BlockAveraged     : m_CalcPtr = xStructSim::CalcBlckAvg; break;
  default: assert(0); break;
  }
}
flt64V4 xSSIM::calcPicMSSSIM(const xPicP* Tst, const xPicP* Ref)
{
  assert(Ref != nullptr && Tst != nullptr);
  assert(Ref->isCompatible(Tst) && Ref->isSameSize(m_PicSize) && Ref->isSameBitDepth(m_BitDepth));

  std::array<flt64V4, c_NumMultiScales> m_SubScores = { xMakeVec4<flt64>(0) };

  m_SubScores[0] = xCalcPicSSIM(Tst, Ref, false, false);
  for(int32 i = 1; i < c_NumMultiScales; i++)
  {
    if(i == 1) { xDownsamplePic(m_SubPicTst[i], Tst             ); xDownsamplePic(m_SubPicRef[i], Ref             ); }
    else       { xDownsamplePic(m_SubPicTst[i], m_SubPicTst[i-1]); xDownsamplePic(m_SubPicRef[i], m_SubPicRef[i-1]); }

    m_SubScores[i] = xCalcPicSSIM(m_SubPicTst[i], m_SubPicRef[i], false, i == c_NumMultiScales-1);
  }

  //hint: sometimes SubScore can be negative so do the same as pytorch - use ReLU to avoid (-0.sth)^Scale
  std::array<flt64V4, c_NumMultiScales> m_CorrectedSubScores;
  for(int32 i = 0; i < c_NumMultiScales; i++) { m_CorrectedSubScores[i] = m_SubScores[i].getVecReLU(); }

  flt64V4 CompoundScore = xMakeVec4<flt64>(1);
  for(int32 i = 0; i < c_NumMultiScales; i++)
  { 
    CompoundScore = CompoundScore * m_CorrectedSubScores[i].getVecPow1(c_MultiScaleWghts<flt64>[i]);
  }

  return CompoundScore;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

flt64V4 xSSIM::xCalcPicSSIM(const xPicP* Tst, const xPicP* Ref, bool UseWS, bool CalcL)
{
  assert(Ref != nullptr && Tst != nullptr);
  assert(Ref->isCompatible(Tst) && Ref->getHeight() <= m_PicSize.getY() && Ref->isSameBitDepth(m_BitDepth));

  if(m_IsRegular)
  {
    if(m_UseMargin)
    {
      m_LoopBegY = 0;
      m_LoopEndY = Ref->getHeight();
      m_LoopBegX = 0;
      m_LoopEndX = Ref->getWidth();
    }
    else
    {
      m_LoopBegY = c_FilterRange;
      m_LoopEndY = Ref->getHeight() - c_FilterRange;
      m_LoopBegX = c_FilterRange;
      m_LoopEndX = Ref->getWidth() - c_FilterRange;
    }
    m_NumUnitY = (m_LoopEndY - m_LoopBegY + m_WndStride - 1) / m_WndStride;
    m_NumUnitX = (m_LoopEndX - m_LoopBegX + m_WndStride - 1) / m_WndStride;
  }
  else //block based
  {
    m_LoopBegY = 0;
    m_LoopEndY = Ref->getHeight() - m_WndSize + 1;
    m_NumUnitY = xCalcNumBlocks(Ref->getHeight(), m_WndSize, m_WndStride);
    m_LoopBegX = 0;
    m_LoopEndX = Ref->getWidth() - m_WndSize + 1;
    m_NumUnitX = xCalcNumBlocks(Ref->getWidth(), m_WndSize, m_WndStride);
  }

  flt64V4 SSIM = xMakeVec4<flt64>(0.0);
  for(int32 CmpIdx = 0; CmpIdx < m_NumComponents; CmpIdx++)
  {
    SSIM[CmpIdx] = xCalcCmpSSIM(Tst, Ref, (eCmp)CmpIdx, UseWS, CalcL);
  }

  return SSIM;
}

flt64 xSSIM::xCalcCmpSSIM(const xPicP* Tst, const xPicP* Ref, eCmp CmpId, bool UseWS, bool CalcL)
{
  memset(m_RowSums[(int32)CmpId].data(), 0, m_RowSums[(int32)CmpId].size() * sizeof(flt64));

  for(int32 y = m_LoopBegY; y < m_LoopEndY; y+=m_WndStride)
  {
    m_ThPI->addWaitingTask([this, &Tst, &Ref, CmpId, y, CalcL](int32 ) { m_RowSums[(int32)CmpId][y] = xCalcRowSSIM(Tst, Ref, CmpId, y, CalcL); });
  }
  m_ThPI->waitUntilTasksFinished(m_NumUnitY);

  if(UseWS)
  {
    xKBNS WeightsAcc;
    for(int32 y = m_LoopBegY; y < m_LoopEndY; y += m_WndStride)
    {
      int32 Offset = m_IsRegular ? y : y + (m_WndSize >> 1);
      flt64 Weight = m_EquirectangularWeights[Offset];
      m_RowSums[(int32)CmpId][y] = m_RowSums[(int32)CmpId][y] * Weight;
      WeightsAcc += Weight;
    }
    flt64 WeightsSum  = WeightsAcc.result();
    flt64 WeightsCorr = WeightsSum / m_NumUnitY;

    flt64 PicSumSSIM  = xKBNS::Accumulate(m_RowSums[(int32)CmpId]);
    int64 NumActive   = (int64)m_NumUnitY * (int64)m_NumUnitX;
    flt64 SSIM        = PicSumSSIM / ((flt64)NumActive * WeightsCorr);
    return SSIM;
  }
  else
  {
    flt64 PicSumSSIM = xKBNS::Accumulate(m_RowSums[(int32)CmpId]);
    int64 NumActive  = (int64)m_NumUnitY * (int64)m_NumUnitX;
    flt64 SSIM       = PicSumSSIM / (flt64)NumActive;
    return SSIM;
  }  
}
flt64 xSSIM::xCalcRowSSIM(const xPicP* Tst, const xPicP* Ref, eCmp CmpId, const int32 y, bool CalcL) const
{
  const int32   TstStride = Tst->getStride();
  const int32   RefStride = Ref->getStride();
  const uint16* TstPtr    = Tst->getAddr(CmpId) + y * TstStride;
  const uint16* RefPtr    = Ref->getAddr(CmpId) + y * RefStride;
  
  xKBNS RowAccSSIM;

  for(int32 x = m_LoopBegX; x < m_LoopEndX; x += m_WndStride)
  {
    RowAccSSIM += m_CalcPtr(TstPtr + x, RefPtr + x, TstStride, RefStride, m_WndSize, m_C1, m_C2, CalcL);
  }

  flt64 RowSumSSIM = RowAccSSIM.result();
  return RowSumSSIM;
}
int32 xSSIM::xCalcNumBlocks(int32 Length, int32 BlockSize, int32 BlockStride)
{
  int32 NumActive = 0;
  assert(BlockSize >= BlockStride);

  if(xIsPowerOf2(BlockSize) && xIsPowerOf2(BlockStride))
  {
    const int32 Log2BlkSize     = xFastLog2((uint32)BlockSize  );
    const int32 Log2BlkStride   = xFastLog2((uint32)BlockStride);
    int32 NumBlocksIn1stRow     = Length >> Log2BlkSize;
    int32 RemainPixelsIn1stRow  = Length - (NumBlocksIn1stRow << Log2BlkSize);
    int32 Log2NumStridesInBlock = Log2BlkSize - Log2BlkStride;
    NumActive = ((NumBlocksIn1stRow - 1) << Log2NumStridesInBlock) + (RemainPixelsIn1stRow >> Log2BlkStride) + 1;
  }
  else
  {
    int32 End = Length - BlockSize + 1;
    int32 Cnt = 0;
    for(int32 i = 0; i < End; i += BlockStride) { Cnt++; }
    return Cnt;
  }

  return NumActive;

}
void xSSIM::xDownsamplePic(xPicP* Dst, const xPicP* Src)
{
  for(int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
  {
    xPixelOps::DownsampleHV(Dst->getAddr((eCmp)CmpIdx), Src->getAddr((eCmp)CmpIdx), Dst->getStride(), Src->getStride(), Dst->getWidth(), Dst->getHeight());
  }
}

//===============================================================================================================================================================================================================
// xIVSSIM
//===============================================================================================================================================================================================================

void xIVSSIM::create(int32V2 Size, int32 BitDepth, int32 Margin, bool EnableMS)
{
  xSSIM::create(Size, BitDepth, Margin, EnableMS);
  m_TstSCP = new xPicP(Size, BitDepth, Margin);
  m_RefSCP = new xPicP(Size, BitDepth, Margin);
}
void xIVSSIM::destroy()
{
  m_TstSCP->destroy(); delete m_TstSCP; m_TstSCP = nullptr;
  m_RefSCP->destroy(); delete m_RefSCP; m_RefSCP = nullptr;
  xSSIM::destroy();
}
flt64 xIVSSIM::calcPicIVSSIM(const xPicP* Tst, const xPicP* Ref)
{
  assert(Tst != nullptr && Ref != nullptr && Ref->isCompatible(Tst));
  assert(m_RefSCP->isCompatible(Ref) && m_TstSCP->isCompatible(Tst)); 

  int32V4 GlobalColorDiffRef2Tst = xGlobClrDiff::CalcGlobalColorDiff(Ref, Tst, m_CmpUnntcbCoef, m_ThPI);

  xShftCompPic::GenShftCompPics(m_RefSCP, m_TstSCP, Ref, Tst, GlobalColorDiffRef2Tst, m_SearchRange, m_CmpWeightsSearch, m_ThPI);

  flt64V4 SSIMs_T2R = xCalcPicSSIM(Tst, m_RefSCP, m_UseWS, true);
  flt64V4 SSIMs_R2T = xCalcPicSSIM(Ref, m_TstSCP, m_UseWS, true);

  const int32V4 CmpWeightsAverage             = m_CmpWeightsAverage;
  const int32   SumCmpWeight                  = CmpWeightsAverage.getSum();
  const flt64   ComponentWeightInvDenominator = 1.0 / (flt64)SumCmpWeight;

  flt64 SSIM_T2R = (SSIMs_T2R * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;
  flt64 SSIM_R2T = (SSIMs_R2T * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;

  flt64 IVSSIM = xMin(SSIM_T2R, SSIM_R2T);

  if(m_DebugCallbackGCS) { m_DebugCallbackGCS(GlobalColorDiffRef2Tst); }
  if(m_DebugCallbackQAP) { m_DebugCallbackQAP(SSIM_R2T, SSIM_T2R); }
  
  return IVSSIM;
}
flt64 xIVSSIM::calcPicIVSSIM(const xPicP* Tst, const xPicP* Ref, const xPicP* TstSCP, const xPicP* RefSCP)
{
  assert(Tst != nullptr && Ref != nullptr && Ref->isCompatible(Tst));
  assert(TstSCP != nullptr && TstSCP->isCompatible(Ref) && RefSCP != nullptr && RefSCP->isCompatible(Tst));

  flt64V4 SSIMs_T2R = xCalcPicSSIM(Tst, RefSCP, m_UseWS, true);
  flt64V4 SSIMs_R2T = xCalcPicSSIM(Ref, TstSCP, m_UseWS, true);

  const int32V4 CmpWeightsAverage             = m_CmpWeightsAverage;
  const int32   SumCmpWeight                  = CmpWeightsAverage.getSum();
  const flt64   ComponentWeightInvDenominator = 1.0 / (flt64)SumCmpWeight;

  flt64 SSIM_T2R = (SSIMs_T2R * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;
  flt64 SSIM_R2T = (SSIMs_R2T * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;

  flt64 IVSSIM = xMin(SSIM_T2R, SSIM_R2T);

  if(m_DebugCallbackQAP) { m_DebugCallbackQAP(SSIM_R2T, SSIM_T2R); }
  
  return IVSSIM;
}
flt64 xIVSSIM::calcPicIVMSSSIM(const xPicP* Tst, const xPicP* Ref)
{
  assert(Tst != nullptr && Ref != nullptr && Ref->isCompatible(Tst));
  assert(m_RefSCP->isCompatible(Ref) && m_TstSCP->isCompatible(Tst)); 

  int32V4 GlobalColorDiffRef2Tst = xGlobClrDiff::CalcGlobalColorDiff(Ref, Tst, m_CmpUnntcbCoef, m_ThPI);

  xShftCompPic::GenShftCompPics(m_RefSCP, m_TstSCP, Ref, Tst, GlobalColorDiffRef2Tst, m_SearchRange, m_CmpWeightsSearch, m_ThPI);

  flt64V4 MSSSIMs_T2R = calcPicMSSSIM(Tst, m_RefSCP);
  flt64V4 MSSSIMs_R2T = calcPicMSSSIM(Ref, m_TstSCP);

  const int32V4 CmpWeightsAverage             = m_CmpWeightsAverage;
  const int32   SumCmpWeight                  = CmpWeightsAverage.getSum();
  const flt64   ComponentWeightInvDenominator = 1.0 / (flt64)SumCmpWeight;

  flt64 MSSSIM_T2R = (MSSSIMs_T2R * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;
  flt64 MSSSIM_R2T = (MSSSIMs_R2T * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;

  flt64 IVMSSSIM = xMin(MSSSIM_T2R, MSSSIM_R2T);

  if(m_DebugCallbackGCS) { m_DebugCallbackGCS(GlobalColorDiffRef2Tst); }
  if(m_DebugCallbackQAP) { m_DebugCallbackQAP(MSSSIM_R2T, MSSSIM_T2R); }
  
  return IVMSSSIM;
}
flt64 xIVSSIM::calcPicIVMSSSIM(const xPicP* Tst, const xPicP* Ref, const xPicP* TstSCP, const xPicP* RefSCP)
{
  assert(Tst != nullptr && Ref != nullptr && Ref->isCompatible(Tst));
  assert(TstSCP != nullptr && TstSCP->isCompatible(Ref) && RefSCP != nullptr && RefSCP->isCompatible(Tst));

  flt64V4 MSSSIMs_T2R = calcPicMSSSIM(Tst, RefSCP);
  flt64V4 MSSSIMs_R2T = calcPicMSSSIM(Ref, TstSCP);

  const int32V4 CmpWeightsAverage             = m_CmpWeightsAverage;
  const int32   SumCmpWeight                  = CmpWeightsAverage.getSum();
  const flt64   ComponentWeightInvDenominator = 1.0 / (flt64)SumCmpWeight;

  flt64 MSSSIM_T2R = (MSSSIMs_T2R * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;
  flt64 MSSSIM_R2T = (MSSSIMs_R2T * (flt64V4)CmpWeightsAverage).getSum() * ComponentWeightInvDenominator;

  flt64 IVMSSSIM = xMin(MSSSIM_T2R, MSSSIM_R2T);

  if(m_DebugCallbackQAP) { m_DebugCallbackQAP(MSSSIM_R2T, MSSSIM_T2R); }
  
  return IVMSSSIM;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB