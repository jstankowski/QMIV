/*
    SPDX-FileCopyrightText: 2019-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#include "xCommonDefIVQM.h"
#include "xPic.h"
#include "xPlane.h"
#include "xMetricCommon.h"
#include "xStructSim.h"
#include "xStructSimConsts.h"
#include "xWeightedSpherically.h"
#include "xGlobClrDiff.h"
#include "xShftCompPic.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

class xSSIM : public xMetricCommon, public xStructSimConsts, public xWeightedSpherically
{
public:
  enum class eMode // struct mode
  {
    INVALID            = NOT_VALID,
    RegularGaussianFlt = 0,
    RegularGaussianInt = 1,
    RegularAveraged    = 2,
    BlockGaussianInt   = 3,
    BlockAveraged      = 4,
  };
  static eMode       xStrToMode(const std::string& Mode);
  static std::string xModeToStr(eMode              Mode);

protected:
  int32V2 m_PicSize     = { NOT_VALID, NOT_VALID };
  int32   m_BitDepth    = NOT_VALID;
  eMode   m_Mode        = eMode::INVALID;
  bool    m_IsRegular   = false;
  bool    m_UseMargin   = false;
  int32   m_WndSize     = NOT_VALID; //window size
  int32   m_WndStride   = NOT_VALID; //window stride

  int32   m_LoopBegY    = NOT_VALID;
  int32   m_LoopEndY    = NOT_VALID;
  int32   m_NumUnitY    = NOT_VALID;
  int32   m_LoopBegX    = NOT_VALID;
  int32   m_LoopEndX    = NOT_VALID;
  int32   m_NumUnitX    = NOT_VALID;

  flt64   m_C1        = std::numeric_limits<flt64>::quiet_NaN();
  flt64   m_C2        = std::numeric_limits<flt64>::quiet_NaN();

  //function pointer
  flt64(*m_CalcPtr) (const uint16*, const uint16*, int32, int32, int32, flt64, flt64, bool) = nullptr;

  std::vector<flt64> m_RowSums[4];

protected: //MSSSIM 
  xPicP* m_SubPicTst[c_NumMultiScales] = { nullptr };
  xPicP* m_SubPicRef[c_NumMultiScales] = { nullptr };

public:
  virtual void create (int32V2 PicSize, int32 BitDepth, int32 Margin, bool EnableMS);
  virtual void destroy();

  void setStructSimParams(eMode Mode, bool UseMargin, int32 BlockSize, int32 WndStride);

  flt64V4 calcPicSSIM  (const xPicP* Tst, const xPicP* Ref) { return xCalcPicSSIM(Tst, Ref, m_UseWS, false); }
  flt64V4 calcPicWSSSIM(const xPicP* Tst, const xPicP* Ref) { return xCalcPicSSIM(Tst, Ref, false  , false); }
  flt64V4 calcPicMSSSIM(const xPicP* Tst, const xPicP* Ref);

  static bool  isRegularMode(eMode Mode) { return Mode == eMode::RegularGaussianFlt || Mode == eMode::RegularGaussianInt || Mode == eMode::RegularAveraged; }
  static int32 determineWindowSize(eMode Mode, int32 BlockSize) { return isRegularMode(Mode) ? 11 : BlockSize; }

protected:  
  flt64V4 xCalcPicSSIM(const xPicP* Tst, const xPicP* Ref,             bool UseWS, bool CalcL);
  flt64   xCalcCmpSSIM(const xPicP* Tst, const xPicP* Ref, eCmp CmpId, bool UseWS, bool CalcL);
  flt64   xCalcRowSSIM(const xPicP* Tst, const xPicP* Ref, eCmp CmpId, const int32 y, bool CalcL) const;

  static int32 xCalcNumBlocks(int32 Length, int32 BlockSize, int32 BlockStride);
  static int32 xNumOverlappingBlocksInSize(int32 Length, int32 Log2BlockSize) { return (Length >> Log2BlockSize) + ((Length - xLog2SizeToSize(Log2BlockSize - 1)) >> Log2BlockSize); }
  static void  xDownsamplePic(xPicP* Dst, const xPicP* Src);
};

//===============================================================================================================================================================================================================

class xIVSSIM : public xSSIM, public xGlobClrDiffPrms, public xCorrespPixelShiftPrms
{
public:  
  using tDCfGCS = std::function<void(const int32V4&)>; //GCS = GlobalColorShift
  using tDCfQAP = std::function<void(flt64, flt64)>;   //QAP = QualAsymmetricPic
protected:
  tDCfGCS m_DebugCallbackGCS;
  tDCfQAP m_DebugCallbackQAP;
public:
  void  setDebugCallbackGCS(tDCfGCS DebugCallbackGCS) { m_DebugCallbackGCS = DebugCallbackGCS; }
  void  setDebugCallbackQAP(tDCfQAP DebugCallbackQAP) { m_DebugCallbackQAP = DebugCallbackQAP; }

//IVSSIM
protected:
  //(corresponding pixel) Shift Compensated Pictures (SCP)
  xPicP* m_TstSCP = nullptr; 
  xPicP* m_RefSCP = nullptr;

public:
  virtual void create (int32V2 Size, int32 BitDepth, int32 Margin, bool EnableMS);
  virtual void destroy();

  flt64 calcPicIVSSIM  (const xPicP* Tst, const xPicP* Ref);
  flt64 calcPicIVSSIM  (const xPicP* Tst, const xPicP* Ref, const xPicP* TstSCP, const xPicP* RefSCP);
  flt64 calcPicIVMSSSIM(const xPicP* Tst, const xPicP* Ref);
  flt64 calcPicIVMSSSIM(const xPicP* Tst, const xPicP* Ref, const xPicP* TstSCP, const xPicP* RefSCP);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB