/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelkoi@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include "xFile.h"
#include "xSeq.h"
#include "xCfgINI.h"
#include "xFmtScn.h"
#include "xMemory.h"
#include "xMiscUtilsCORE.h"
#include "xKBNS.h"
#include "xShftCompPic.h"
#include "xTimeUtils.h"
#include "xPly.h"
#include <math.h>
#include <fstream>
#include <time.h>
#include <limits>
#include <numeric>
#include <cassert>
#include <thread>
#include <filesystem>
#include "fmt/chrono.h"
#include "xUtilsAppQM.h"
#include "xMetricsAppQM.h"
#include "xAppQM_Common.h"
#include "xCommonDefGSR.h"
#include "xGSR_Renderer.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

class xAppQMGC : public xAppQM
{
public:
  static const std::string_view c_BannerString     ;
  static const std::string_view c_HelpStringHead   ;
  static const std::string_view c_HelpStringGeneral;
  static const std::string_view c_HelpStringIV     ;
  static const std::string_view c_HelpStringExample;
  tStr formatHelp();

public:
  //basic io
  tStr        m_SynthFile[c_NumPics];
  bool        m_SynthFilePerView  = true;
  eFileFmt    m_FileFormatS       = eFileFmt::RAW;  
  eClrSpcApp  m_ColorSpaceFileS   = eClrSpcApp::YCbCr_BT709;
  eCrF        m_ChromaFormatFileS = eCrF::CF420;
  eClrSpcApp  m_ColorSpaceMetric  = eClrSpcApp::YCbCr_BT709;
  bool        m_SynthAlphaCap     = true;
  
  //iv-specific
  int32V4     m_CmpWeightsSearchRGB   ;
  int32V4     m_CmpWeightsSearchYCbCr ;
  int32V4     m_CmpWeightsAverageRGB  ;
  int32V4     m_CmpWeightsAverageYCbCr;

  //derrived
  bool        m_WriteSynth      = false;
  bool        m_WriteSynthYCbCr = false;
  bool        m_ReorderWriteRGB = false;

protected:
  //sequences and buffers
  std::array<GSC::xSeqPlyList, c_NumPics> m_SeqPlyIn; //0=Tst,1=Ref
  std::vector<xSeqPic*> m_SeqPicSyn[c_NumPics];

protected:
  //processing data
  int32 m_NumRenders = 0;
  //GSC clouds (reloaded per frame)
  GSC::xGaussianCloud                 m_Cloud[c_NumPics];
  //cam params
  GSC::xGaussianCloudCmn::tCameraData m_Cameras;
  //processors
  GSR::xRenderer* m_Renderer;
  xPicP m_PicTmpP; 

  //merics data & stats
  uint64 m_Ticks___Synth = 0;
  uint64 m_TicksWriteSyn = 0;

public:
  void    registerCmdParams  ();
  bool    readConfiguration  ();
  tStr    formatConfiguration();
  eAppRes validateInputFiles ();
  std::string formatWarnings      () { return formatWarningsCmn(); }

  virtual int32 determinePreferredNumThreads() final { return m_HardwareConcurency; };
  virtual int32 determineMaxNumTasks        () final;

  eAppRes     setupSeqs ();
  eAppRes     ceaseSeqs ();
  eAppRes     setupBuffs();
  eAppRes     ceaseBuffs();

  void        initMetricStorage();

  //processors
  void createRendererProcessor ();
  void destroyRendererProcessor();


  eAppRes processAllFrames ();

  eAppRes loadClouds     (int32 FrameIdx);
  void    synthesizePics (int32 FrameIdx, int32 ViewIdx);
  eAppRes writeSynths    (int32 FrameIdx, int32 ViewIdx);
  void    preprocessRGB  (int32 FrameIdx);
  void    preprocessYCbCr(int32 FrameIdx);

  tStr formatResultsStdOut();
  tStr formatResultsFile  ();

protected:
  void        xSetProcessorsCmpWeights(const int32V4& CmpWeightsSearch, const int32V4& CmpWeightsAverage);
  static GSR::tCamPar xCamPosToParams(const GSC::xGaussianCloudCmn::xCameraPosition& Cam, int32V2 Resolution);
  static tStr xFormatRenderFile(tStr Pattern, int32V2 PictureSize, int32 ViewIdx);
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB