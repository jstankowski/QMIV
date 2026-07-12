/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelkoi@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#include "xAppQMGC.h"
#include "xProcInfo.h"
#include "xFmtScn.h"
#include "xPixelOps.h"
#include "xColorSpace.h"
#include "xSeqLST.h"
#include "xPly.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

const std::string_view xAppQMGC::c_BannerString =
R"PMBBRAWSTRING(
=============================================================================
QMIV-GS software v4.0-dev   [Quality Merics for Immersive Video - Gaussian Splatting]

Copyright (c) 2020-2026, Poznan University of Technology, All rights reserved.

Developed at Poznan University of Technology, Poznan, Poland
Authors: Jakub Stankowski, Błażej Szydełko, Adrian Dziembowski

=============================================================================

)PMBBRAWSTRING";

const std::string_view xAppQMGC::c_HelpStringHead =
R"PMBBRAWSTRING(
=============================================================================
QMIV-GS software v4.0-dev
)PMBBRAWSTRING";

const std::string_view xAppQMGC::c_HelpStringGeneral =
R"PMBBRAWSTRING(
usage::general --------------------------------------------------------------
 -i0   InputFile0         File path - test gaussian splat      (fmtlib pattern, e.g. "cloudT_{:04d}.ply")
 -i1   InputFile1         File path - reference gaussian splat (fmtlib pattern, e.g. "cloudR_{:04d}.ply")
 -ps   PictureSize        Resolution of rendered image (WxH)
 -bd   BitDepth           Bit depth of rendered image (optional, default=8, up to 14)
 -sf0  StartFrame0        Start frame 0  (optional, default=0) 
 -sf1  StartFrame1        Start frame 1  (optional, default=0) 
 -nf   NumberOfFrames     Number of frames to process (optional, -1=all, default=-1)
 -r    ResultFile         Output file for metric results (optional)
 -s0   SynthFile0         Output YUV base name for rendered pictures from InputFile0 (optional)
 -s1   SynthFile1         Output YUV base name for rendered pictures from InputFile1 (optional)
                          Filename pattern has to contain {W}, {H}, and {ViewIdx} e.g. Render_{W}x{H}_V{ViewIdx:d}_Bbps.yuv
                          Both or files or none at all have to be provided.
 -sfv  SynthFilePerView   Use different files for each target view (optional, default 1)
 -ffs  FileFormatS        File format of synth output (optional, default=RAW) [RAW, PNG, BMP]
 -css  ColorSpaceFileS    Color space of synth output (optional, default=YCbCr)
                          [RGB, BGR, GBR, YCbCr_BT601, YCbCr_SMPTE170M, YCbCr_BT709, YCbCr_SMPTE240M, YCbCr_BT2020]
 -cfs  ChromaFormatFileS  Chroma format for render output (optional, default=420) [400, 420, 422, 444]
 usage::synthesizer ---------------------------------------------------------
 -sap  SynthAlphaCap      Effective alpha capped at 0.999 before compositing as in mpeg-gsc-metrics (optional, default=1)
)PMBBRAWSTRING";
const std::string_view xAppQMGC::c_HelpStringIV =
R"PMBBRAWSTRING(
       CmpWeightsSearchRGB    IV-metric component weights used during search "R:G:B:0"
                              (per component integer weights, default="1:1:1:0", quotes are mandatory)
       CmpWeightsSearchYCbCr  IV-metric component weights used during search "Lm:Cb:Cr:0"
                              (per component integer weights, default="4:1:1:0", quotes are mandatory)
       CmpWeightsAverageRGB   IV-metric component weights used during averaging "R:G:B:0"
                              (per component integer weights, default="1:1:1:0", quotes are mandatory)
       CmpWeightsAverageYCbCr IV-metric component weights used during averaging "Lm:Cb:Cr:0"
                              (per component integer weights, default="4:1:1:0", quotes are mandatory) )PMBBRAWSTRING";
const std::string_view xAppQMGC::c_HelpStringExample =
R"PMBBRAWSTRING(-----------------------------------------------------------------------------
Example:
  QMIV-GS -i0 "src_{04:d}.ply" -i1 "dec_{04:d}.ply" -ps 1920x1080 -v 3 -r "r.txt"

=============================================================================
)PMBBRAWSTRING";

//===============================================================================================================================================================================================================

std::string xAppQMGC::formatHelp()
{
  std::string Result = "";
  Result += c_HelpStringHead   ;
  Result += c_HelpStringGeneral;
  Result += c_HelpStringMetrics;
  //Result += c_HelpStringMask   ;
  Result += c_HelpStringCmnIV  ;
  Result += c_HelpStringIV     ;
  Result += c_HelpStringSSIM   ;
  Result += c_HelpStringValOp  ;
  Result += c_HelpStringExample;
  return Result;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void xAppQMGC::registerCmdParams()
{
  registerCmdParamsDispatch(); //dispatcher params to be ignored
  //basic io
  m_CfgParser.addCmdParm("i0" , "InputFile0"       , "", "InputFile0"          );
  m_CfgParser.addCmdParm("i1" , "InputFile1"       , "", "InputFile1"          );
  m_CfgParser.addCmdParm("ps" , "PictureSize"      , "", "PictureSize"         );
  m_CfgParser.addCmdParm("bd" , "BitDepth"         , "", "BitDepth"            );
  m_CfgParser.addCmdParm("sf0", "StartFrame0"      , "", "StartFrame0"         );
  m_CfgParser.addCmdParm("sf1", "StartFrame1"      , "", "StartFrame1"         );
  m_CfgParser.addCmdParm("nf" , "NumberOfFrames"   , "", "NumberOfFrames"      );
  m_CfgParser.addCmdParm("r"  , "ResultFile"       , "", "ResultFile"          );
  m_CfgParser.addCmdParm("s0" , "SynthFile0"       , "", "SynthFile0"          );
  m_CfgParser.addCmdParm("s1" , "SynthFile1"       , "", "SynthFile1"          );
  m_CfgParser.addCmdParm("sfv", "SynthFilePerView" , "", "SynthFilePerView"    );
  m_CfgParser.addCmdParm("ffs", "FileFormatS"      , "", "FileFormatS"         );
  m_CfgParser.addCmdParm("css", "ColorSpaceFileS"  , "", "ColorSpaceFileS"     );
  m_CfgParser.addCmdParm("cfs", "ChromaFormatFileS", "", "ChromaFormatFileS"   );
  //synth setup
  m_CfgParser.addCmdParm("sap", "SynthAlphaCap"    , "", "SynthAlphaCap"       );
  //metrics
  m_CfgParser.addCmdList("ml" , "MetricList"       , "", "MetricList", ','     );
  //auxliary io
  m_CfgParser.addCmdParm("cf0", "ShftCompPicFile0" , "", "OutputScpFile0"      );
  m_CfgParser.addCmdParm("cf1", "ShftCompPicFile1" , "", "OutputScpFile1"      );
  //mask io
  registerCmdParamsMask();
  //iv-specific
  registerCmdParamsCmnIV();
  //ssim specific
  registerCmdParamsSSIM();
  //validation & operation
  registerCmdParamsValOp();
}
bool xAppQMGC::readConfiguration()
{
  bool Correct = true;

  //basic io ----------------------------------------------------------------------------------------------------------
  m_InputFile[0] = m_CfgParser.getParam1stArg("InputFile0", std::string(""));
  m_InputFile[1] = m_CfgParser.getParam1stArg("InputFile1", std::string(""));
  if(m_InputFile[0].empty()) { m_ErrorLog += "!  InputFile0 is empty\n"; Correct = false; }
  if(m_InputFile[1].empty()) { m_ErrorLog += "!  InputFile1 is empty\n"; Correct = false; }

  std::string PictureSizeS = m_CfgParser.getParam1stArg("PictureSize", std::string(""));
  m_PictureSize = xFmtScn::scanResolution(PictureSizeS);
  if(m_PictureSize[0] <= 0 || m_PictureSize[1] <= 0) { m_ErrorLog += "!  Invalid PictureSize value\n"; Correct = false; }
  m_BitDepth          = m_CfgParser.getParam1stArg("BitDepth"        , 8 );
  m_StartFrame[0]     = m_CfgParser.getParam1stArg("StartFrame0", 0);
  m_StartFrame[1]     = m_CfgParser.getParam1stArg("StartFrame1", 0);
  if(m_StartFrame[0] < 0 || m_StartFrame[1] < 0) { m_ErrorLog += "!  StartFrame value cannot be negative\n"; Correct = false; }
  m_NumberOfFrames    = m_CfgParser.getParam1stArg("NumberOfFrames"  , -1);
  m_ResultFile        = m_CfgParser.getParam1stArg("ResultFile"      , std::string(""));
  m_SynthFile[0]      = m_CfgParser.getParam1stArg("SynthFile0"      , std::string(""));
  m_SynthFile[1]      = m_CfgParser.getParam1stArg("SynthFile1"      , std::string(""));
  m_WriteSynth        = !m_SynthFile[0].empty() && !m_SynthFile[1].empty();
  m_SynthFilePerView  = m_CfgParser.getParam1stArg("SynthFilePerView", 1 );
  m_FileFormatS       = m_CfgParser.cvtParam1stArg("FileFormatS"     , eFileFmt::RAW          , xStr2FileFmt  );
  m_ColorSpaceFileS   = m_CfgParser.cvtParam1stArg("ColorSpaceFileS" , eClrSpcApp::YCbCr_BT709, xStr2ClrSpcApp);
  m_ChromaFormatFileS = m_CfgParser.cvtParam1stArg("ChromaFormatS"   , eCrF::CF420            , xStr2CrF      );
  m_WriteSynthYCbCr   = isDefinedYCbCr(m_ColorSpaceFileS);
  m_ReorderWriteRGB   = isRGB(m_ColorSpaceFileS) && m_ColorSpaceFileS != eClrSpcApp::RGB;
  //synthesizer -------------------------------------------------------------------------------------------------------
  m_SynthAlphaCap     = m_CfgParser.getParam1stArg("SynthAlphaCap"   , 1);
  //metrics -----------------------------------------------------------------------------------------------------------
  Correct = Correct && readConfigurationMetric();
  //mask io -----------------------------------------------------------------------------------------------------------
  Correct = Correct && readConfigurationMask();
  //iv-specific -------------------------------------------------------------------------------------------------------
  Correct = Correct && readConfigurationCmnIV();
  std::string CmpWeightsSearchRGB_S    = m_CfgParser.getParam1stArg("CmpWeightsSearchRGB"   , xFmtScn::formatIntWeights(xCorrespPixelShiftPrms::c_EqualCmpWeights  ));
  std::string CmpWeightsSearchYCbCr_S  = m_CfgParser.getParam1stArg("CmpWeightsSearchYCbCr" , xFmtScn::formatIntWeights(xCorrespPixelShiftPrms::c_DefaultCmpWeights));
  std::string CmpWeightsAverageRGB_S   = m_CfgParser.getParam1stArg("CmpWeightsAverageRGB"  , xFmtScn::formatIntWeights(xCorrespPixelShiftPrms::c_EqualCmpWeights  ));
  std::string CmpWeightsAverageYCbCr_S = m_CfgParser.getParam1stArg("CmpWeightsAverageYCbCr", xFmtScn::formatIntWeights(xCorrespPixelShiftPrms::c_DefaultCmpWeights));
  m_CmpWeightsSearchRGB    = xFmtScn::scanIntWeights(CmpWeightsSearchRGB_S   );
  m_CmpWeightsSearchYCbCr  = xFmtScn::scanIntWeights(CmpWeightsSearchYCbCr_S );
  m_CmpWeightsAverageRGB   = xFmtScn::scanIntWeights(CmpWeightsAverageRGB_S  );
  m_CmpWeightsAverageYCbCr = xFmtScn::scanIntWeights(CmpWeightsAverageYCbCr_S);
  //ssim specific -----------------------------------------------------------------------------------------------------
  Correct = Correct && readConfigurationSSIM();
  //validation & operation --------------------------------------------------------------------------------------------
  Correct = Correct && readConfigurationValOp();
  //derrived ----------------------------------------------------------------------------------------------------------  
  derriveConfiguration();

  //post-validation ---------------------------------------------------------------------------------------------------
  if(m_WriteSynth && m_ChromaFormat == eCrF::CF420 && ((m_PictureSize.getX() & 0x1) || (m_PictureSize.getY() & 0x1)))
  {
    m_ErrorLog += "! Chroma format 420 requires PictureWidth and PictureHeight to be even\n"; Correct = false;
  }
  if(m_WriteSynth && m_ChromaFormat == eCrF::CF422 && (m_PictureSize.getX() & 0x1))
  {
    m_ErrorLog += "! Chroma format 422 requires PictureWidth to be even\n"; Correct = false;
  }
  if(m_UseMask && m_CalcSSIMs)
  {
    m_ErrorLog += "! Structural Similarity metrics cannot be combined with Mask mode\n"; Correct = false;
  }
  if(m_UseMask && !xSSIM::isRegularMode(m_StructSimMode))
  {
    m_ErrorLog += "! Mask mode requires regular SSIM mode\n"; Correct = false;
  }
  if(m_UseMask && m_StructSimStride != 1)
  {
    m_ErrorLog += "! Mask mode requires StructSimStride=1\n"; Correct = false;
  }
  if(m_UseMask && m_CalcMSs)
  {
    m_ErrorLog += "! MS-SSIM and IV-MS-SSIM does not support mask mode\n"; Correct = false;
  }

  return Correct;
}
std::string xAppQMGC::formatConfiguration()
{
  std::string Config; Config.reserve(xMemory::getBestEffortSizePageBase());
  Config += "Run-time configuration:\n";
  Config += fmt::format("InputFile0        = {}\n"  , m_InputFile[0]);
  Config += fmt::format("InputFile1        = {}\n"  , m_InputFile[1]);
  Config += fmt::format("PictureSize       = {}\n"  , xFmtScn::formatResolution(m_PictureSize) );
  Config += fmt::format("BitDepth          = {}\n"  , m_BitDepth);
  Config += fmt::format("StartFrame0       = {}\n"  , m_StartFrame[0]    );
  Config += fmt::format("StartFrame1       = {}\n"  , m_StartFrame[1]    );
  Config += fmt::format("NumberOfFrames    = {}{}\n", m_NumberOfFrames, m_NumberOfFrames==NOT_VALID ? "  (all)" : "");
  Config += fmt::format("ResultFile        = {}\n"  , m_ResultFile.empty() ? "(unused)" : m_ResultFile);
  Config += fmt::format("SynthFile0        = {}\n"  , m_SynthFile[0]);
  Config += fmt::format("SynthFile1        = {}\n"  , m_SynthFile[1]);
  Config += fmt::format("SynthFilePerView  = {}\n"  , m_SynthFilePerView);
  Config += fmt::format("FileFormatS       = {}\n"  , xFileFmt2Str(m_FileFormatS));
  Config += fmt::format("ColorSpaceFileS   = {}\n"  , xClrSpcApp2Str(m_ColorSpaceMetric));
  Config += fmt::format("ChromaFormatFileS = {}{}\n", xCrF2Str(m_ChromaFormat), !m_WriteSynthYCbCr ? "  (irrelevant)" : "");
  Config += fmt::format("ColorSpaceMetric  = RGB+{}\n"  , xClrSpcApp2Str(m_ColorSpaceMetric));
  Config += fmt::format("SynthAlphaCap     = {}\n"  , m_SynthAlphaCap);
  //metrics
  formatConfigurationMetrics(Config);
  //mask io
  formatConfigurationMask(Config);
  //iv-specific
  formatConfigurationCmnIV(Config);
  Config += fmt::format("CmpWeightsSearchRGB    = {}{}\n", xFmtScn::formatIntWeights(m_CmpWeightsSearchRGB   ), m_CmpWeightsSearchRGB    == xCorrespPixelShiftPrms::c_EqualCmpWeights   ? "  (default)" : "  (custom)");
  Config += fmt::format("CmpWeightsSearchYCbCr  = {}{}\n", xFmtScn::formatIntWeights(m_CmpWeightsSearchYCbCr ), m_CmpWeightsSearchYCbCr  == xCorrespPixelShiftPrms::c_DefaultCmpWeights ? "  (default)" : "  (custom)");
  Config += fmt::format("CmpWeightsAverageRGB   = {}{}\n", xFmtScn::formatIntWeights(m_CmpWeightsAverageRGB  ), m_CmpWeightsAverageRGB   == xCorrespPixelShiftPrms::c_EqualCmpWeights   ? "  (default)" : "  (custom)");
  Config += fmt::format("CmpWeightsAverageYCbCr = {}{}\n", xFmtScn::formatIntWeights(m_CmpWeightsAverageYCbCr), m_CmpWeightsAverageYCbCr == xCorrespPixelShiftPrms::c_DefaultCmpWeights ? "  (default)" : "  (custom)");
  //ssim specific
  formatConfigurationSSIM(Config);
  //validation & operation
  formatConfigurationValOp(Config);
  Config += "\n";
  //derrived
  Config += fmt::format("Run-time derrived parameters:\n");
  Config += fmt::format("WindowSize        = {}x{}\n", m_WindowSize, m_WindowSize);
  Config += fmt::format("PictureMargin     = {}\n", m_PicMargin);
  Config += fmt::format("UseMask           = {:d}\n", m_UseMask);
  Config += "\n";
  //metric description
  Config += fmt::format("Selected metrics:\n");
  for(int32 m = 0; m < c_MetricsNum; m++) { if(m_CalcMetric[m]) { Config += fmt::format("{:<9} - {}\n", xMetricToStr((eMetric)m), xMetricInfo::Metrics[m].Description); } }
  Config += "\n";

  return Config;
}
eAppRes xAppQMGC::validateInputFiles()
{
  QMAU_TRACE(2, "");
  return eAppRes::Good;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int32 xAppQMGC::determineMaxNumTasks()
{
  QMAU_TRACE(3, "");
  int32 NumTiles    = GSR::calcNumTiles(m_PictureSize);
  int32 MaxNumTasks = xMax(4096, m_PictureSize.getY(), NumTiles);
  return MaxNumTasks;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

eAppRes xAppQMGC::setupSeqs()
{
  QMAU_TRACE(2, "");

  //check if file exists
  for(int32 i = 0; i < c_NumPics; i++)
  {
    if(!xFileListUtils::checkIfFileExists(m_InputFile[i])) { xErrMsg::printError(fmt::format("ERROR --> InputFile{} does not exist ({}) [Checked 0 & 1]", i, m_InputFile[i])); return eAppRes::Error; }
  }

  if(m_UseMask)
  {
    if(m_FileFormatM == eFileFmt::RAW)
    {
      if(!xFile::exists(m_InputFileM)) { xErrMsg::printError(fmt::format("ERROR --> InputFile{} does not exist ({})", "M", m_InputFileM)); return eAppRes::Error; }
    }
    else
    {
      if(!xFileListUtils::checkIfFileExists(m_InputFileM)) { xErrMsg::printError(fmt::format("ERROR --> InputFile{} does not exist ({}) [Checked 0 & 1]", "M", m_InputFileM)); return eAppRes::Error; }
    }
  }

  //create input sequences 
  for(int32 i = 0; i < c_NumPics; i++) { m_SeqPlyIn[i].create(); }

  if(m_UseMask)
  {
    switch(m_FileFormatM)
    {
    case eFileFmt::RAW: m_SeqMskIn = new xSeq(m_PictureSize, m_BitDepthM, m_ChromaFormatM); break;
    case eFileFmt::PNG: m_SeqMskIn = new xSeqPNG(m_PictureSize, std::numeric_limits<uint16>::max()); break;
    case eFileFmt::BMP: m_SeqMskIn = new xSeqBMP(m_PictureSize, std::numeric_limits<uint16>::max()); break;
    default: xErrMsg::printError(fmt::format("ERROR --> unsupported FileFormat ({})", xFileFmt2Str(m_FileFormatM))); return eAppRes::Error;
    }
  }

  //open input sequences 
  for(int32 i = 0; i < c_NumPics; i++)
  {
    xSeqPic::tResult Result = m_SeqPlyIn[i].openFile(m_InputFile[i], xSeq::eMode::Read);
    if(!Result) { xErrMsg::printError(fmt::format("ERROR --> InputFile opening failure ({}) {}", m_InputFile[i], Result.format())); return eAppRes::Error; }
  }

  // load first frame of both sequences to discover and validate camera lists
  GSC::xGaussianCloudCmn::tCameraData Cameras[c_NumPics];
  for(int32 i = 0; i < c_NumPics; i++)
  {
    std::string FirstPath = m_SeqPlyIn[i].get1stFileName();
    if(!xFile::exists(FirstPath)) { xErrMsg::printError(fmt::format("ERROR --> InputFile{} first frame does not exist ({})", i, FirstPath)); return eAppRes::Error; }

    GSC::xPly Ply;
    if(!Ply.openFile(FirstPath, xSeqFile::eMode::Read)) { xErrMsg::printError(fmt::format("ERROR --> cannot open InputFile{} ({})", i, FirstPath)); return eAppRes::Error; }
    GSC::xGaussianCloud TmpCloud;
    GSC::xPly::tResult ReadRes = Ply.readGC(&TmpCloud, true);
    if(!ReadRes) { TmpCloud.destroy(); Ply.closeFile(); xErrMsg::printError(fmt::format("ERROR --> cannot read InputFile{} ({}): {}", i, FirstPath, ReadRes.format())); return eAppRes::Error; }
    Ply.closeFile();
    Cameras[i] = TmpCloud.getCameraData();
    TmpCloud.destroy();
  }

  // resolve camera list: copy from the other if one is empty; error if both non-empty and differ
  if(Cameras[0].empty() && Cameras[1].empty())
  {
    xErrMsg::printError("ERROR --> no camera_position entries found in either PLY file");
    return eAppRes::Error;
  }
  else if(Cameras[0].empty()) { Cameras[0] = Cameras[1]; }
  else if(Cameras[1].empty()) { Cameras[1] = Cameras[0]; }
  else if(Cameras[0].size() != Cameras[1].size())
  {
    xErrMsg::printError(fmt::format("ERROR --> camera_position count mismatch: InputFile0 has {}, InputFile1 has {}", Cameras[0].size(), Cameras[1].size()));
    return eAppRes::Error;
  }
  else
  {
    for(int32 c = 0; c < (int32)Cameras[0].size(); c++)
    {
      const auto& C0 = Cameras[0][c];
      const auto& C1 = Cameras[1][c];
      if(C0.m_CamIdx != C1.m_CamIdx || C0.m_Name != C1.m_Name)
      {
        xErrMsg::printError(fmt::format("ERROR --> camera_position mismatch at index {}: InputFile0 camId={} name={}, InputFile1 camId={} name={}", c, C0.m_CamIdx, C0.m_Name, C1.m_CamIdx, C1.m_Name));
        return eAppRes::Error;
      }
    }
  }

  m_Cameras  = Cameras[0];
  m_NumViews = (int32)m_Cameras.size();
  if(m_VerboseLevel >= 1) { fmt::print("NumViews         = {}\n", m_NumViews); }

  //num of frames per input file
  int32 NumOfFrames[c_NumPics] = { 0 };
  int32 NumOfFramesM = 0;
  for(int32 i = 0; i < c_NumPics; i++)
  {
    NumOfFrames[i] = m_SeqPlyIn[i].getNumOfFrames();
    if(m_VerboseLevel >= 1) { fmt::print("DetectedFrames{}  = {}\n", i, NumOfFrames[i]); }
    if(m_StartFrame[i] >= NumOfFrames[i]) { xErrMsg::printError(fmt::format("ERROR --> StartFrame{} >= DetectedFrames{} for ({})", i, i, m_InputFile[i])); return eAppRes::Error; }
  }
  if(m_UseMask)
  {
    NumOfFramesM = m_SeqMskIn->getNumOfFrames();
    if(m_VerboseLevel >= 1) { fmt::print("DetectedFrames{}  = {}\n", "M", NumOfFramesM); }
    if(m_StartFrameM >= NumOfFramesM) { xErrMsg::printError(fmt::format("ERROR --> StartFrame{} >= DetectedFrames{} for ({})", "M", "M", m_InputFileM)); return eAppRes::Error; }
  }

  //num of frames to process - TODO - include mask
  int32 MinSeqNumFrames = xMin(NumOfFrames[0], NumOfFrames[1]);
  int32 MinSeqRemFrames = xMin(NumOfFrames[0] - m_StartFrame[0], NumOfFrames[1] - m_StartFrame[1]);
  m_NumFrames = xMin(m_NumberOfFrames > 0 ? m_NumberOfFrames : MinSeqNumFrames, MinSeqRemFrames);

  int32 FirstFrame[c_NumPics] = { 0 };
  for(int32 i = 0; i < c_NumPics; i++) { FirstFrame[i] = xMin(m_StartFrame[i], NumOfFrames[i] - 1); }
  if(m_VerboseLevel >= 1) { fmt::print("FramesToProcess  = {}\n", m_NumFrames); }

  if(m_UseMask && (m_NumFrames > NumOfFramesM)) { xErrMsg::printError(fmt::format("ERROR --> FramesToProcess > NumOfFramesM")); return eAppRes::Error; }

  //seek sequences - TODO - include mask
  for(int32 i = 0; i < c_NumPics; i++)
  {
    if(FirstFrame[i] != 0)
    {
      xSeqPic::tResult Result = m_SeqPlyIn[i].seekFrame(FirstFrame[i]);
      if(!Result) { xErrMsg::printError(fmt::format("ERROR --> InputFile seeking failure ({}) {}", m_InputFile[i], Result.format())); return eAppRes::Error; }
    }
  }
  m_NumRenders = m_NumFrames * m_NumViews;
  if(m_VerboseLevel >= 1) { fmt::print("NumRenders       = {}\n", m_NumRenders); }
  fmt::print("\n\n");


  // rendered sequences
  if(m_WriteSynth)
  {
    const int32 NumViewsToOpen = m_SynthFilePerView ? m_NumViews : 1;
    for(int32 i = 0; i < c_NumPics; i++)
    {
      m_SeqPicSyn[i].resize(NumViewsToOpen);
      for(int32 v = 0; v < NumViewsToOpen; v++)
      {
        switch(m_FileFormatS)
        {
        case eFileFmt::RAW: m_SeqPicSyn[i][v] = new xSeq   (m_PictureSize, m_BitDepth, m_WriteSynthYCbCr ? m_ChromaFormatFileS : eCrF::CF444) ; break;
        case eFileFmt::PNG: m_SeqPicSyn[i][v] = new xSeqPNG(m_PictureSize, std::numeric_limits<uint16>::max()); break;
        case eFileFmt::BMP: m_SeqPicSyn[i][v] = new xSeqBMP(m_PictureSize, std::numeric_limits<uint16>::max()); break;
        default: xErrMsg::printError(fmt::format("ERROR --> unsupported FileFormatS ({})", xFileFmt2Str(m_FileFormatS))); return eAppRes::Error;
        }
        std::string FileName = xFormatRenderFile(m_SynthFile[i], m_PictureSize, v);
        if(FileName.empty()) { xErrMsg::printError(fmt::format("ERROR --> SynthFile formating failure ({})", m_SynthFile[i])); return eAppRes::Error; }
        bool OpenSucces = (bool)(m_SeqPicSyn[i][v]->openFile(FileName, xSeq::eMode::Write));
        if(!OpenSucces) { xErrMsg::printError(fmt::format("ERROR --> SynthFile opening failure ({})", FileName)); return eAppRes::Error; }
      }
    }
  }

  return eAppRes::Good;
}
eAppRes xAppQMGC::ceaseSeqs()
{
  QMAU_TRACE(2, "");

  //input sequences
  for(int32 i = 0; i < c_NumPics; i++)
  {
    m_SeqPlyIn[i].closeFile();
    m_SeqPlyIn[i].destroy  ();
  }
  //rendered sequences
  for(int32 i = 0; i < c_NumPics; i++)
  {
    for(xSeqPic* Seq : m_SeqPicSyn[i]) { Seq->closeFile(); Seq->destroy(); }
    m_SeqPicSyn[i].clear();
  }
  //scp sequences
  if(m_WriteSCP)
  {
    for(int32 i = 0; i < c_NumPics; i++) { m_SeqPicSCP[i].closeFile(); }
    for(int32 i = 0; i < c_NumPics; i++) { m_SeqPicSCP[i].destroy  (); }
  }
  //mask sequences
  if(m_UseMask)
  {
    m_SeqMskIn->closeFile(); m_SeqMskIn->destroy(); m_SeqMskIn = nullptr;
  }

  return eAppRes::Good;
}
eAppRes xAppQMGC::setupBuffs()
{
  QMAU_TRACE(2, "");
  //input buffers
  for(int32 i = 0; i < c_NumPics; i++) { m_PicInP[i].create(m_PictureSize, m_BitDepth, m_PicMargin); }
  if(m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicInI[i].create(m_PictureSize, m_BitDepth, m_PicMargin); } }
  //SCP buffers
  if(m_CalcSCP)
  {
    for(int32 i = 0; i < c_NumPics; i++) { m_PicSCP[i].create(m_PictureSize, m_BitDepth, m_PicMargin); }
    if(m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicSCI[i].create(m_PictureSize, m_BitDepth, m_PicMargin); } }
  }
  //output buffers
  if(m_WriteSynth)
  { 
    for(int32 i = 0; i < c_NumPics; i++) { m_PicOutP[i].create(m_PictureSize, m_BitDepth, m_PicMargin); }
    if(m_WriteSynthYCbCr || m_ReorderWriteRGB) { m_PicTmpP.create(m_PictureSize, m_BitDepth, m_PicMargin); }
  }
  //mask buffer
  if(m_UseMask) { m_PicMskInP.create(m_PictureSize, m_BitDepthM, m_PicMargin); }

  return eAppRes::Good;
}
eAppRes xAppQMGC::ceaseBuffs()
{
  QMAU_TRACE(2, "");
  //input buffers
  for(int32 i = 0; i < c_NumPics; i++) { m_PicInP[i].destroy(); }
  if(m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicInI[i].destroy(); } }
  //SCP buffers
  if(m_CalcSCP)
  {
    for(int32 i = 0; i < c_NumPics; i++) { m_PicSCP[i].destroy(); }
    if(m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicSCI[i].destroy(); } }
  }
  //output buffers
  if(m_WriteSynth)
  { 
    for(int32 i = 0; i < c_NumPics; i++) { m_PicOutP[i].destroy(); }
    if(m_WriteSynthYCbCr || m_ReorderWriteRGB) { m_PicTmpP.destroy(); }
  }
  //mask buffer
  if(m_UseMask) { m_PicMskInP.destroy(); }

  return eAppRes::Good;
}
void xAppQMGC::initMetricStorage()
{
  QMAU_TRACE(2, "");

  for(int32 m = 0; m < c_MetricsNum; m++)
  {
    if(m_CalcMetric[m]) 
    {
      m_MetricData[m].initMetric  ((eMetric)m, m_NumFrames, m_NumViews);
      m_MetricData[m].initSuffixes(m_UseMask);
      m_MetricData[m].initCmpWeightsAverage(m_CmpWeightsAverageYCbCr, eClrSpcMtr::YCbCr);
      m_MetricData[m].initCmpWeightsAverage(m_CmpWeightsAverageRGB  , eClrSpcMtr::RGB  );
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void xAppQMGC::createRendererProcessor()
{
  QMAU_TRACE(2, "");
  // create renderers - both share the main thread pool (each gets its own client ID)
    m_Renderer = new GSR::xRenderer();
    m_Renderer->create(m_PictureSize, m_ThreadPool);
    m_Renderer->setCapEffAlpha(m_SynthAlphaCap);
    m_Renderer->setGatherTimeStats(m_GatherTime);
}
void xAppQMGC::destroyRendererProcessor()
{
  QMAU_TRACE(2, "");
  m_Renderer->destroy(); delete m_Renderer; m_Renderer = nullptr;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

eAppRes xAppQMGC::processAllFrames()
{
  QMAU_TRACE(2, "");
  m_TimeStamp.sampleBeg();

  for(int32 f = 0; f < m_NumFrames; f++)
  {
    eAppRes LoadRes = loadClouds(f); if(LoadRes != eAppRes::Good) { return LoadRes; }   

    for(int32 v = 0; v < m_NumViews; v++)
    {
      synthesizePics (f, v);
      if(m_WriteSynth) { writeSynths(f, v); }
      preprocessMask (f);
      //RGB colorspace path
      xSetProcessorsCmpWeights(m_CmpWeightsSearchRGB, m_CmpWeightsAverageRGB);
      preprocessRGB    (f);
      rearrangePictures(f);
      if(m_CalcGCD) { calcFrameGCD(f, v); }
      if(m_CalcSCP) { calcFrameSCP(f, v); }
      //TODO add WriteSCP
      if(getCalcMetric(eMetric::MSE     )) { calcFrame_____MSE(f, v, eClrSpcMtr::RGB); }
      if(getCalcMetric(eMetric::PSNR    )) { calcFrame____PSNR(f, v, eClrSpcMtr::RGB); }
      if(getCalcMetric(eMetric::WSPSNR  )) { calcFrame__WSPSNR(f, v, eClrSpcMtr::RGB); }
      if(getCalcMetric(eMetric::IVPSNR  )) { calcFrame__IVPSNR(f, v, eClrSpcMtr::RGB); }
      if(m_StructSimBrdExt != eMrgExt::None) { addStructSimMargs(f); }
      if(getCalcMetric(eMetric::SSIM    )) { calcFrame____SSIM(f, v, eClrSpcMtr::RGB); }
      if(getCalcMetric(eMetric::MSSSIM  )) { calcFrame__MSSSIM(f, v, eClrSpcMtr::RGB); }
      if(getCalcMetric(eMetric::IVSSIM  )) { calcFrame__IVSSIM(f, v, eClrSpcMtr::RGB); }
      if(getCalcMetric(eMetric::IVMSSSIM)) { calcFrameIVMSSSIM(f, v, eClrSpcMtr::RGB); }
#if X_PMBB_STALLED
      if(getCalcMetric(eMetric::MSIVSSIM)) { calcFrameMSIVSSIM(f, v, eClrSpcMtr::RGB); }
#endif //X_PMBB_STALLED
      if(getCalcMetric(eMetric::PVD     )) { calcFrame_____PVD(f, v, eClrSpcMtr::RGB); }

      //YCbCr colorspace path
      xSetProcessorsCmpWeights(m_CmpWeightsSearchYCbCr, m_CmpWeightsAverageYCbCr);
      preprocessYCbCr  (f);
      rearrangePictures(f);
      if(m_CalcGCD) { calcFrameGCD(f, v); }
      if(m_CalcSCP) { calcFrameSCP(f, v); }
      //TODO add WriteSCP
      if(getCalcMetric(eMetric::MSE     )) { calcFrame_____MSE(f, v, eClrSpcMtr::YCbCr); }
      if(getCalcMetric(eMetric::PSNR    )) { calcFrame____PSNR(f, v, eClrSpcMtr::YCbCr); }
      if(getCalcMetric(eMetric::WSPSNR  )) { calcFrame__WSPSNR(f, v, eClrSpcMtr::YCbCr); }
      if(getCalcMetric(eMetric::IVPSNR  )) { calcFrame__IVPSNR(f, v, eClrSpcMtr::YCbCr); }
      if(m_StructSimBrdExt != eMrgExt::None) { addStructSimMargs(f); }
      if(getCalcMetric(eMetric::SSIM    )) { calcFrame____SSIM(f, v, eClrSpcMtr::YCbCr); }
      if(getCalcMetric(eMetric::MSSSIM  )) { calcFrame__MSSSIM(f, v, eClrSpcMtr::YCbCr); }
      if(getCalcMetric(eMetric::IVSSIM  )) { calcFrame__IVSSIM(f, v, eClrSpcMtr::YCbCr); }
      if(getCalcMetric(eMetric::IVMSSSIM)) { calcFrameIVMSSSIM(f, v, eClrSpcMtr::YCbCr); }
#if X_PMBB_STALLED
      if(getCalcMetric(eMetric::MSIVSSIM)) { calcFrameMSIVSSIM(f, v, eClrSpcMtr::YCbCr); }
#endif //X_PMBB_STALLED
      if(getCalcMetric(eMetric::PVD     )) { calcFrame_____PVD(f, v, eClrSpcMtr::YCbCr); }
    } //end of loop over views
  } //end of loop over frames

  m_TimeStamp.sampleEnd();

  return eAppRes::Good;
}

eAppRes xAppQMGC::loadClouds(int32 f)
{
  QMAU_TRACE(3, "");
  uint64 T = m_GatherTime ? xTSC() : 0;

  xSeqPic::tResult ReadResult[c_NumPics] = { eRetv::Success, eRetv::Success };
  xSeqPic::tResult ReadResultM = eRetv::Success;

  for(int32 i = 0; i < c_NumPics; i++) { m_TPI.storeTask([this, &ReadResult, i](int32 /*ThId*/) { ReadResult[i] = m_SeqPlyIn[i].readGC(&(m_Cloud[i])); }); }
  if(m_UseMask) { m_TPI.storeTask([this, &ReadResultM](int32 /*ThId*/) { ReadResultM = m_SeqMskIn->readFrame(&(m_PicMskInP)); }); }
  m_TPI.executeStoredTasks();

  for(int32 i = 0; i < c_NumPics; i++) { if(!ReadResult[i]) { xErrMsg::printError(fmt::format("Frame {:08d} ERROR --> InputFile read error ({}) {}", f, m_InputFile[i], ReadResult[i].format())); return eAppRes::Error; } }
  if(m_UseMask) { if(!ReadResultM) { xErrMsg::printError(fmt::format("Frame {:08d} ERROR --> InputFile read error ({}) {}", f, m_InputFileM, ReadResultM.format())); return eAppRes::Error; } }

  if(m_GatherTime) { m_Ticks____Load += (xTSC() - T); }
  return eAppRes::Good;
}
void xAppQMGC::synthesizePics(int32 FrameIdx, int32 ViewIdx)
{
  QMAU_TRACE(3, "");
  uint64 T = m_GatherTime ? xTSC() : 0;
  if(m_PrintDebug) { fmt::print("{} Sythesize\n", xFormatIdx(FrameIdx, ViewIdx)); }
  GSR::tCamPar Cam = xCamPosToParams(m_Cameras[ViewIdx], m_PictureSize);
  for(int32 i = 0; i < c_NumPics; i++)
  {
    m_Renderer->renderFrame(&m_PicInP[i], &m_Cloud[i], Cam);
  }
  if(m_GatherTime) { m_Ticks___Synth += (xTSC() - T); }
}
eAppRes xAppQMGC::writeSynths(int32 f, int32 v)
{
  QMAU_TRACE(3, "");
  uint64 T = m_GatherTime ? xTSC() : 0;

  const int32 LocalViewIdx = m_SynthFilePerView ? v : 0;
  for(int32 i = 0; i < c_NumPics; i++)
  {
    if(!m_WriteSynthYCbCr)
    {
      if(!m_ReorderWriteRGB)
      {
        xSeqPic::tResult WriteResult = m_SeqPicSyn[i][LocalViewIdx]->writeFrame(&(m_PicInP[i]));
        if(!WriteResult) { xErrMsg::printError(fmt::format("ERROR --> SynthFile write error ({}) {}", f, m_SynthFile[i], WriteResult.format())); return eAppRes::Error; }
      }
      else
      {
        m_PicTmpP.copy(&(m_PicInP[i]));
        if(m_ColorSpaceFileS == eClrSpcApp::BGR)
        {
          m_PicInP[i].swapComponents(eCmp::C0, eCmp::C2); //BGR --> RGB 
        }
        if(m_ColorSpaceFileS == eClrSpcApp::GBR)
        {
          m_PicInP[i].swapComponents(eCmp::C0, eCmp::C1); //GBR --> BGR
          m_PicInP[i].swapComponents(eCmp::C0, eCmp::C2); //BGR --> RGB
        }
        xSeqPic::tResult WriteResult = m_SeqPicSyn[i][LocalViewIdx]->writeFrame(&(m_PicInP[i]));
        if(!WriteResult) { xErrMsg::printError(fmt::format("ERROR --> SynthFile write error ({}) {}", f, m_SynthFile[i], WriteResult.format())); return eAppRes::Error; }
      }    
    }
    else
    {
      const eClrSpcLC ColorSpace = xClrSpcAppToClrSpc(m_ColorSpaceFileS);
       xColorSpace::ConvertRGB2YCbCr(m_PicTmpP.getAddr(eCmp::LM), m_PicTmpP.getAddr(eCmp::CB), m_PicTmpP.getAddr(eCmp::CR),
         m_PicInP[i].getAddr(eCmp::R ), m_PicInP[i].getAddr(eCmp::G ), m_PicInP[i].getAddr(eCmp::B ),          
         m_PicTmpP.getStride(), m_PicTmpP.getStride(), m_PicTmpP.getWidth(), m_PicTmpP.getHeight(), m_PicTmpP.getBitDepth(), ColorSpace);
      xSeqPic::tResult WriteResult = m_SeqPicSyn[i][LocalViewIdx]->writeFrame(&m_PicTmpP);
      if(!WriteResult) { xErrMsg::printError(fmt::format("ERROR --> SynthFile write error ({}) {}", f, m_SynthFile[i], WriteResult.format())); return eAppRes::Error; }
    }
  }

  if(m_GatherTime) { m_TicksWriteSyn += (xTSC() - T); }

  return eAppRes::Good;
}
void xAppQMGC::preprocessRGB(int32 FrameIdx)
{
  QMAU_TRACE(3, "");
  uint64 T = m_GatherTime ? xTSC() : 0;
  for(int32 CmpIdx = 0; CmpIdx < m_PicInP[0].getNumCmps(); CmpIdx++)
  {
    m_ExactCmps[CmpIdx] = m_PicInP[0].equalCmp(&m_PicInP[1], (eCmp)CmpIdx);
  }
  if(m_GatherTime) { m_TicksPreprocF += (xTSC() - T); }
}
void xAppQMGC::preprocessYCbCr(int32 FrameIdx)
{
  QMAU_TRACE(3, "");
  uint64 T = m_GatherTime ? xTSC() : 0;

  const eClrSpcLC ColorSpace = xClrSpcAppToClrSpc(m_ColorSpaceMetric);
  for(int32 i = 0; i < c_NumPics; i++) { m_TPI.storeTask([this, i, ColorSpace](int32) { xColorSpace::ConvertRGB2YCbCr(
    m_PicInP[i].getAddr(eCmp::R ), m_PicInP[i].getAddr(eCmp::G ), m_PicInP[i].getAddr(eCmp::B ),
    m_PicInP[i].getAddr(eCmp::LM), m_PicInP[i].getAddr(eCmp::CB), m_PicInP[i].getAddr(eCmp::CR),
    m_PicInP[i].getStride(), m_PicInP[i].getStride(), m_PicInP[i].getWidth(), m_PicInP[i].getHeight(), m_PicInP[i].getBitDepth(), ColorSpace);
  } ); }
  m_TPI.executeStoredTasks();

  for(int32 CmpIdx = 0; CmpIdx < m_PicInP[0].getNumCmps(); CmpIdx++)
  {
    m_ExactCmps[CmpIdx] = m_PicInP[0].equalCmp(&m_PicInP[1], (eCmp)CmpIdx);
  }

  if(m_GatherTime) { m_TicksPreprocF += (xTSC() - T); }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void xAppQMGC::xSetProcessorsCmpWeights(const int32V4& CmpWeightsSearch, const int32V4& CmpWeightsAverage)
{
  QMAU_TRACE(3, "");
  m_CmpWeightsSearch  = CmpWeightsSearch ;
  m_CmpWeightsAverage = CmpWeightsAverage;

  if(m_CalcSCP)
  {
    m_ProcSCP.setCmpWeightsSearch (m_CmpWeightsSearch );
    m_ProcSCP.setCmpWeightsAverage(m_CmpWeightsAverage);
  }

  if(m_CalcPSNRs)
  {
    m_ProcPSNR.setCmpWeightsSearch (m_CmpWeightsSearch );
    m_ProcPSNR.setCmpWeightsAverage(m_CmpWeightsAverage);
  }

  if(m_CalcSSIMs)
  {
    m_ProcSSIM.setCmpWeightsSearch (m_CmpWeightsSearch );
    m_ProcSSIM.setCmpWeightsAverage(m_CmpWeightsAverage);
  }
}
GSR::tCamPar xAppQMGC::xCamPosToParams(const GSC::xGaussianCloudCmn::xCameraPosition& Cam, int32V2 Resolution)
{
  flt32 qw = Cam.m_OriWXYZ[0], qx = Cam.m_OriWXYZ[1], qy = Cam.m_OriWXYZ[2], qz = Cam.m_OriWXYZ[3];
  flt32 Norm = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
  if(Norm > 0.0f) { qw /= Norm; qx /= Norm; qy /= Norm; qz /= Norm; }

  flt32 R[3][3] = {
    { 1.0f - 2.0f*(qy*qy+qz*qz),  2.0f*(qx*qy-qw*qz)       ,  2.0f*(qx*qz+qw*qy)        },
    {        2.0f*(qx*qy+qw*qz),  1.0f - 2.0f*(qx*qx+qz*qz),  2.0f*(qy*qz-qw*qx)        },
    {        2.0f*(qx*qz-qw*qy),  2.0f*(qy*qz+qw*qx)       ,  1.0f - 2.0f*(qx*qx+qy*qy) }
  };

  GSR::tCamPar Params;
  Params.setCamName   (Cam.m_Name  );
  Params.setIdx       (Cam.m_CamIdx);
  Params.setResolution(Resolution);
  Params.setProjType  (eProjType::Perspective);
  Params.setZnear     (0.1f); //TODO -inf
  Params.setZfar      (1000.0f); //TODO inf

  const flt32 cx = Resolution.getX() * 0.5f;
  const flt32 cy = Resolution.getY() * 0.5f;
  auto& K = Params.getIntrinsics();
  K.zero();
  K[0][0] = Cam.m_FocalXY[0]; K[0][2] = cx;
  K[1][1] = Cam.m_FocalXY[1]; K[1][2] = cy;
  K[2][2] = 1.0f;
  K[3][3] = 1.0f;

  auto& E = Params.getExtrinsics();
  E.zero();
  for(int32 w = 0; w < 3; w++) { E[0][w] = R[0][w]; }
  for(int32 w = 0; w < 3; w++) { E[1][w] = R[1][w]; }
  for(int32 w = 0; w < 3; w++) { E[2][w] = R[2][w]; }
  E[0][3] = Cam.m_PosXYZ[0];
  E[1][3] = Cam.m_PosXYZ[1];
  E[2][3] = Cam.m_PosXYZ[2];
  E[3][3] = 1.0f;

  Params.calculateDerrived();
  return Params;
}
std::string xAppQMGC::xFormatRenderFile(std::string Pattern, int32V2 PictureSize, int32 ViewIdx)
{
  std::string Result;
  try
  {
    Result = fmt::format(fmt::runtime(Pattern), fmt::arg("W", PictureSize.getX()), fmt::arg("H", PictureSize.getY()), fmt::arg("ViewIdx", ViewIdx));
  }
  catch(const fmt::format_error& Err)
  {
    xErrMsg::printError(fmt::format("Invalid format string provided = {}\n", Err.what()));
    return "";
  }
  return Result;
}


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

std::string xAppQMGC::formatResultsFile()
{
  QMAU_TRACE(2, "");
  std::time_t TimeStamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  std::string Result; Result.reserve(xMemory::getBestEffortSizePageBase());
  
  Result += fmt::format("FILE0  \"{}\"\n", m_InputFile[0]);
  Result += fmt::format("FILE1  \"{}\"\n", m_InputFile[1]);
  if(m_UseMask) { Result += fmt::format("FILEM  \"{}\"\n", m_InputFile[2]); }
  Result += fmt::format ("TIME   {:%Y-%m-%d  %H:%M:%S}\n", *std::localtime(&TimeStamp));

  for(int32 m = 0; m < c_MetricsNum; m++)
  {
    xMetricStat& MD = m_MetricData[m];
    if(MD.getEnabled()) { Result += MD.formatAvgMetric("", eClrSpcMtr::RGB  ) + "\n"; }
  }

  for(int32 m = 0; m < c_MetricsNum; m++)
  {
    xMetricStat& MD = m_MetricData[m];
    if(MD.getEnabled()) { Result += MD.formatAvgMetric("", eClrSpcMtr::YCbCr) + "\n"; }
  }

  return Result;
}
std::string xAppQMGC::formatResultsStdOut()
{
  QMAU_TRACE(2, "");
  std::string Result; Result.reserve(xMemory::getBestEffortSizePageBase());

  for(int32 m = 0; m < c_MetricsNum; m++)
  {
    xMetricStat& MD = m_MetricData[m];
    if(MD.getEnabled()) { Result += MD.formatAvgMetric("Average      ", eClrSpcMtr::RGB  ) + "\n"; }
  }

  for(int32 m = 0; m < c_MetricsNum; m++)
  {
    xMetricStat& MD = m_MetricData[m];
    if(MD.getEnabled()) { Result += MD.formatAvgMetric("Average      ", eClrSpcMtr::YCbCr) + "\n"; }
  }

  if(m_GatherTime)
  {
    tDurationMS AvgDuration____Load = tDurationMS((flt64)m_Ticks____Load * m_InvDurationDenominatorF );
    tDurationMS AvgDuration___Synth = tDurationMS((flt64)m_Ticks___Synth * m_InvDurationDenominatorFV);
    tDurationMS AvgDurationWriteSyn = tDurationMS((flt64)m_TicksWriteSyn * m_InvDurationDenominatorFV);
    Result += "\n";
    Result += fmt::format("AvgTime          LOAD {:9.2f} ms\n", AvgDuration____Load.count());
    Result += fmt::format("AvgTime         SYNTH {:9.2f} ms\n", AvgDuration___Synth.count());
    Result += fmt::format("AvgTime   WRITE_SYNTH {:9.2f} ms\n", AvgDurationWriteSyn.count());
    Result += formatMetricTimeStats();
    Result += "\n";
    Result += "RENDERER";
    Result += m_Renderer->formatAndResetStats("  ", m_TimeStamp.getTicksPerMiliSec());
  }
  return Result;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB