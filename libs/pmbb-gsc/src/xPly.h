/*
    SPDX-FileCopyrightText: 2025-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelkoi@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSC.h"
#include "xSeq.h"
#include "xFileListUtils.h"
#include "xGaussianCloud.h"
#include "xPointCloud.h"
#include <unordered_map>
#include <map>

namespace PMBB_NAMESPACE::GSC {

//===============================================================================================================================================================================================================

class xPly2
{
public:
  using tStr  = std::string;
  using tCamD = xGaussianCloudCmn::tCameraData;
  using tPrOf = std::map<tStr    , int32>;
  using tAITO = std::map<xAttrFLT, int32>;
  using tRes  = xInOutResult;

  static constexpr std::string_view c_PropertyPattern    = "property float ";
  static constexpr int32            c_PropertyPatternLen = (int32)c_PropertyPattern.length();
  static constexpr int32            c_MaxNumSphHarm      = 45;
  static constexpr int32            c_NumSplatsInBatch   = 4 * 4096;
  static constexpr int32            c_NumPointsInBatch   = 4 * c_NumSplatsInBatch;
  static constexpr bool             c_SphHarmOrderPCO    = true; //true --> [Point, Component, Order]; false --> [Point, Order, Component]

protected:
  tCamD m_CameraData;
  int32 m_NumSplats        = NOT_VALID;
  int32 m_NumSphHarmCoeffs = 0;
  int32 m_NumSphHarm       = 0;
  tPrOf m_PropertyOffset;
  tAITO m_AttrIdToOffset;

public:
  xPly2 () {}
  ~xPly2() {}

  tRes readGC (xGaussianCloud* DstCloud, xStream* Stream, bool HeaderOnly = false);
  tRes writeGC(xStream* Stream, const xGaussianCloud* SrcCloud);
  tRes writePC(xStream* Stream, const xPointCloud*    SrcCloud, bool Binary);

protected:
  tStr        xFindHeader(xStream* Stream);
  int32       xFindPropertyOffset(const tStr& AttributeName);
  tRes        xParseHeaderGSC (const tStr& Header);
  static tStr xFormatHeaderGSC(int32 NumSplats, int32 NumSphericalHarmonics);
  static tStr xFormatHeaderPC (int32 NumUnits, bool Binary);

};

//===============================================================================================================================================================================================================

class xSeqPlyList : public xSeqFile, public xFileListUtils
{
public:
  using tStr = std::string;

protected:
  tStr  m_FileNamePattern;
  int32 m_MaxNumFiles = NOT_VALID;
  int32 m_1stFileIdx  = NOT_VALID;
  bool  m_SingleFile  = false;

  int32 m_NumOfFrames  = NOT_VALID;
  int32 m_CurrFrameIdx = NOT_VALID;

  xPly2 m_Ply;

public:
  void create (int32 MaxNumFiles = c_DefaultMaxNumFiles) { m_MaxNumFiles = MaxNumFiles; }
  void destroy() {}

  tResult readGC(xGaussianCloud* DstCloud);

  tResult seekFrame(int32 FrameIdx);

  tStr    get1stFileName () const { return xFormatFileName(m_1stFileIdx); }  
  int32   getNumOfFrames () const { return m_NumOfFrames; }
  int32   getCurrFrameIdx() const { return m_CurrFrameIdx; }

protected:
  virtual bool    xBackendAllowsRead  () const final { return true ; }
  virtual bool    xBackendAllowsWrite () const final { return false; }
  virtual bool    xBackendAllowsAppend() const final { return false; }
  virtual bool    xBackendAllowsSeek  () const final { return true ; }
  virtual tResult xBackendOpen        (tCSR FileName, eMode OpMode) final;
  virtual tResult xBackendClose       (                           ) final;

protected:
  inline tStr xFormatFileName(int32 FrameIdx) const { return formatFileName(m_FileNamePattern, FrameIdx); }
  tResult xPlyListOpenRead  ();
  tResult xPlyListOpenWrite ();
  tResult xPlyListFileVerify(tCSR FileName);
};

//===============================================================================================================================================================================================================

class xPly : public xSeqFile
{
public:
  using tStr = std::string;

  static constexpr std::string_view c_PropertyPattern    = "property float ";
  static constexpr int32            c_PropertyPatternLen = (int32)c_PropertyPattern.length();
  static constexpr int32            c_MaxNumSphHarm      = 45;
  static constexpr int32            c_NumSplatsInBatch   = 4 * 4096;
  static constexpr int32            c_NumPointsInBatch   = 4 * c_NumSplatsInBatch;
  static constexpr bool             c_SphHarmOrderPCO    = true; //true --> [Point, Component, Order]; false --> [Point, Order, Component]

protected:
  xStream* m_Stream = nullptr;

  xGaussianCloudCmn::tCameraData m_CameraData;
  int32 m_NumSplats        = NOT_VALID;
  int32 m_NumSphHarmCoeffs = 0;
  int32 m_NumSphHarm       = 0;
  std::map<tStr    , int32> m_PropertyOffset;
  std::map<xAttrFLT, int32> m_AttrIdToOffset;

public:
  xPly () {};
  ~xPly() { destroy(); }

  
  void create () {};
  void destroy();
  void reset  ();

  tResult readGC  (xGaussianCloud*       DstCloud, bool HeaderOnly = false);
  tResult writeGC (const xGaussianCloud* SrcCloud);
  tResult writePC (const xPointCloud*    SrcCloud, bool Binary);

protected:
  protected:
  virtual bool    xBackendAllowsRead  () const final { return true ; }
  virtual bool    xBackendAllowsWrite () const final { return true ; }
  virtual bool    xBackendAllowsAppend() const final { return false; }
  virtual bool    xBackendAllowsSeek  () const final { return false; }
  virtual tResult xBackendOpen        (tCSR FileName, eMode OpMode) final;
  virtual tResult xBackendClose       (                           ) final;

  tStr        xFindHeader();
  int32       xFindPropertyOffset(const tStr& AttributeName);
  tResult     xParseHeaderGSC (const tStr& Header);
  static tStr xFormatHeaderGSC(int32 NumSplats, int32 NumSphericalHarmonics);
  static tStr xFormatHeaderPC (int32 NumUnits, bool Binary);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB