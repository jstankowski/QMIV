/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#include "xAppGSPlayer.h"
#include "xErrMsg.h"
#include "xPicHash.h"
#include "xColorSpace.h"
#include "xPixelOps.h"
#include "spng.h"
#include <fmt/format.h>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace PMBB_NAMESPACE {

    //===============================================================================================================================================================================================================

    static std::string xToLowerCase(std::string Str) { for(char& C : Str) { C = (char)std::tolower((unsigned char)C); } return Str; }
    //the values of a metric of the last calculation are protected in xMetricStat (the application reads them from the file at the end): a view of the same object that makes them public
    struct xMetricStatPeek : public xMetricStat { public: using xMetricStat::getValPic; using xMetricStat::getValCmp; };

    const std::string_view xAppGSPlayer::c_BannerString =
        R"PMBBRAWSTRING(GS-Player - real-time viewer of Gaussian-splat clouds (PMBB, QMIV-GS renderer))PMBBRAWSTRING";
    const std::string_view xAppGSPlayer::c_HelpStringHead =
        R"PMBBRAWSTRING(
usage: GS-Player -i0 <cloud0> [-i1 <cloud1>] -cpf <cameras> [options]
  one cloud: the render of the cloud in a window; two clouds: side by side, with the quality metrics of the renders calculated on the fly
)PMBBRAWSTRING";
    const std::string_view xAppGSPlayer::c_HelpStringGeneral =
        R"PMBBRAWSTRING(
usage::general --------------------------------------------------------------
 -i0   InputFile0         File path - gaussian splat, fmtlib pattern with {FrameIdx} token (a file without the token is a still scene)
 -i1   InputFile1         File path - the second gaussian splat (optional): side by side + metrics
 -cpf  CamParamsFile      Camera parameters file/directory (PLY: a cloud file with the cameras in its header; INI: file; COLMAP: directory with cameras and images files)
 -cpm  CamParamsFormat    Camera parameters format (optional, default=PLY) [PLY, COLMAP, INI]
 -cpt  CamNameTemplate    Camera name template (optional, only used when CamParamsFormat=INI, default="[CamIdStr]")
 -ps   PictureSize        Size of one picture, the window is the picture (two pictures side by side with -i1) (optional, default=960x540). The window can be resized: the render follows it.
 -sf0  StartFrame0        First frame of the sequence 0 (optional, default = the first file found)
 -sf1  StartFrame1        First frame of the sequence 1 (optional, default = the first file found)
 -nf   NumberOfFrames     Number of frames (optional, default=all)
 -fps  PlayFps            Playback rate in frames per second (optional, default=30)
 -nbuf FramesAhead        Frames read ahead (optional, default=6)
 -mem  MemoryFraction     Fraction of the physical memory the frames read ahead may take (optional, default=0.5)
 -loop Loop               Start the playback again after the last frame (optional, default=1)
 -mfps MaxFps             Limit of the render rate (optional, default=60)
 -sap  SynthAlphaCap      Cap of the effective alpha of a splat (optional, default=1)
 -mode StartMode          The camera mode at the start: 0 = free flight, 1 = input views (optional, default=0)
 -rec  RecordFile         File of the recorded flight (CSV, as PoseTraceFile) (optional, default=gsplayer_path.csv)
 -uis  UiScale            Size of the overlay (optional, default=1.0)
 -bench Frames            No window: renders this many frames of the input views, prints the times (and the metrics of the pair) (optional)
 -benchshot File          With -bench: PNG of the first render
 -uitest Frames           Window test: runs this many frames and writes the window with the overlay to UiTestShot
 -uitestms Ms             Window test: the same after this many milliseconds
 -uishot File             File of the window test
 -script "ms:cmd[:arg...];..."  Window test: the keys at the given times (ms of the window): toggle, stepf, stepb, seek:N, next, prev, mode:N, reset, record, shot, metric, loop, speed:+1,
                          move:F:R:U:ms, look:dx:dy, roll:R:ms, size:W:H
usage::metrics (with -i1) ---------------------------------------------------
 -ml   MetricList         List of quality metrics to be calculated, coma separated, quotes are required, "All" = all (optional, default="PSNR, SSIM")
 -mdom MetricDomain       Domain of the metrics: YCbCr, RGB, Both (optional, default=YCbCr, the integer BT.709 YCbCr of QMIV-GS)
 and the options of QMIV-GS of the metrics: -sr -unc -ssm -ssb -sss -ssw (IV-metrics, SSIM)
usage::software_operation ---------------------------------------------------
 -nth  NumberOfThreads    Number of worker threads (optional, default=-2 = all hardware threads, -1 = all)
 -v    VerboseLevel       Verbose level (optional, default=1)
)PMBBRAWSTRING";
    const std::string_view xAppGSPlayer::c_HelpStringKeys =
        R"PMBBRAWSTRING(
keys (along the axes of the camera): A D left / right, W S up / down, E Q forward / back, Z X roll, right mouse button look, wheel speed (Shift / Ctrl faster / slower), Home reset the camera
      F1 free flight, F2 input views (Left / Right), Space play / pause, , . step, + - playback rate, L loop
      R record the flight to a file, F12 screenshot, M metric of the plot, H overlay on / off, F5 help, Esc quit
)PMBBRAWSTRING";

    std::string xAppGSPlayer::formatHelp()
    {
        std::string Result;
        Result += c_HelpStringHead; Result += c_HelpStringGeneral; Result += c_HelpStringKeys;
        return Result;
    }

    //===============================================================================================================================================================================================================
    // configuration
    //===============================================================================================================================================================================================================

    void xAppGSPlayer::registerCmdParams()
    {
        registerCmdParamsDispatch(); //dispatcher params to be ignored
        m_CfgParser.addCmdParm("i0"   , "InputFile0"    , "", "InputFile0"    );
        m_CfgParser.addCmdParm("i1"   , "InputFile1"    , "", "InputFile1"    );
        m_CfgParser.addCmdParm("cpf"  , "CamParamsFile" , "", "CamParamsFile" );
        m_CfgParser.addCmdParm("cpm"  , "CamParamsFormat", "", "CamParamsFormat");
        m_CfgParser.addCmdParm("cpt"  , "CamNameTemplate", "", "CamNameTemplate");
        m_CfgParser.addCmdParm("ps"   , "PictureSize"   , "", "PictureSize"   );
        m_CfgParser.addCmdParm("sf0"  , "StartFrame0"   , "", "StartFrame0"   );
        m_CfgParser.addCmdParm("sf1"  , "StartFrame1"   , "", "StartFrame1"   );
        m_CfgParser.addCmdParm("nf"   , "NumberOfFrames", "", "NumberOfFrames");
        m_CfgParser.addCmdParm("fps"  , "PlayFps"       , "", "PlayFps"       );
        m_CfgParser.addCmdParm("nbuf" , "FramesAhead"   , "", "FramesAhead"   );
        m_CfgParser.addCmdParm("mem"  , "MemoryFraction", "", "MemoryFraction");
        m_CfgParser.addCmdParm("loop" , "Loop"          , "", "Loop"          );
        m_CfgParser.addCmdParm("mfps" , "MaxFps"        , "", "MaxFps"        );
        m_CfgParser.addCmdParm("sap"  , "SynthAlphaCap" , "", "SynthAlphaCap" );
        m_CfgParser.addCmdParm("mode" , "StartMode"     , "", "StartMode"     );
        m_CfgParser.addCmdParm("rec"  , "RecordFile"    , "", "RecordFile"    );
        m_CfgParser.addCmdParm("uis"  , "UiScale"       , "", "UiScale"       );
        m_CfgParser.addCmdParm("bench", "BenchFrames"   , "", "BenchFrames"   );
        m_CfgParser.addCmdParm("benchshot", "BenchShot" , "", "BenchShot"     );
        m_CfgParser.addCmdParm("uitest", "UiTestFrames" , "", "UiTestFrames"  );
        m_CfgParser.addCmdParm("uishot", "UiTestShot"   , "", "UiTestShot"    );
        m_CfgParser.addCmdParm("uitestms", "UiTestMs"    , "", "UiTestMs"      );
        m_CfgParser.addCmdParm("script", "TestScript"   , "", "TestScript"    );
        m_CfgParser.addCmdList("ml"   , "MetricList"    , "", "MetricList", ',');
        m_CfgParser.addCmdParm("mdom" , "MetricDomain"  , "", "MetricDomain"  );
        registerCmdParamsCmnIV();
        registerCmdParamsSSIM();
        registerCmdParamsValOp();
    }
    bool xAppGSPlayer::readConfiguration()
    {
        bool Correct = true;
        m_InputFile[0] = m_CfgParser.getParam1stArg("InputFile0", std::string(""));
        m_InputFile[1] = m_CfgParser.getParam1stArg("InputFile1", std::string(""));
        if(m_InputFile[0].empty()) { m_ErrorLog += "!  InputFile0 is empty\n"; Correct = false; }
        m_Sbs = !m_InputFile[1].empty();

        m_CamParamsFile   = m_CfgParser.getParam1stArg("CamParamsFile", std::string(""));
        m_CamParamsFormat = m_CfgParser.cvtParam1stArg("CamParamsFormat", FVGS::eCamParamsFormat::PLY, FVGS::xStrToCamParamsFormat);
        m_CamNameTemplate = m_CfgParser.getParam1stArg("CamNameTemplate", std::string("[CamIdStr]"));
        if(m_CamParamsFormat == FVGS::eCamParamsFormat::INVALID) { m_ErrorLog += "!  Invalid CamParamsFormat value\n"; Correct = false; }
        if(m_CamParamsFile.empty()) { m_ErrorLog += "!  CamParamsFile is empty\n"; Correct = false; }

        m_PictureSize = xFmtScn::scanResolution(m_CfgParser.getParam1stArg("PictureSize", std::string("960x540")));
        if(m_PictureSize[0] < 64 || m_PictureSize[1] < 64) { m_ErrorLog += "!  Invalid PictureSize value (at least 64x64)\n"; Correct = false; }
        m_PictureSize = int32V2(m_PictureSize[0] & ~1, m_PictureSize[1] & ~1);
        m_BitDepth = 8; m_ChromaFormat = eCrF::CF444;
        m_StartFrameCfg[0] = m_CfgParser.getParam1stArg("StartFrame0", -1);
        m_StartFrameCfg[1] = m_CfgParser.getParam1stArg("StartFrame1", -1);
        m_NumberOfFrames   = m_CfgParser.getParam1stArg("NumberOfFrames", -1);
        m_PlayFps          = m_CfgParser.getParam1stArg("PlayFps", 30.0);
        m_NumAhead         = xMax(1, (int32)m_CfgParser.getParam1stArg("FramesAhead", 6));
        m_MemFraction      = m_CfgParser.getParam1stArg("MemoryFraction", 0.5);
        m_Loop             = m_CfgParser.getParam1stArg("Loop", 1) != 0;
        m_MaxFps           = xMax(1, (int32)m_CfgParser.getParam1stArg("MaxFps", 60));
        m_SynthAlphaCap    = m_CfgParser.getParam1stArg("SynthAlphaCap", 1);
        m_StartMode        = m_CfgParser.getParam1stArg("StartMode", 0);
        m_StartModeGiven   = m_CfgParser.findParam("StartMode");
        m_RecordPath       = m_CfgParser.getParam1stArg("RecordFile", std::string("gsplayer_path.csv"));
        m_UiScale          = m_CfgParser.getParam1stArg("UiScale", 1.0);
        m_BenchFrames      = m_CfgParser.getParam1stArg("BenchFrames", 0);
        m_BenchShot        = m_CfgParser.getParam1stArg("BenchShot", std::string(""));
        m_UiTestFrames     = m_CfgParser.getParam1stArg("UiTestFrames", 0);
        m_UiTestMs         = m_CfgParser.getParam1stArg("UiTestMs", 0);
        m_UiTestShot       = m_CfgParser.getParam1stArg("UiTestShot", std::string("gsplayer_uitest.png"));
        m_ScriptCfg        = m_CfgParser.getParam1stArg("TestScript", std::string(""));
        if(!m_ScriptCfg.empty()) { xParseScript(m_ScriptCfg); }
        if(m_PlayFps <= 0.0) { m_ErrorLog += "!  PlayFps has to be positive\n"; Correct = false; }
        if(m_StartMode < 0 || m_StartMode > 1) { m_ErrorLog += "!  StartMode has to be 0 or 1\n"; Correct = false; }

        //metrics: only for two clouds
        if(m_Sbs)
        {
            if(m_CfgParser.findParam("MetricList")) { Correct = readConfigurationMetric() && Correct; }
            else { m_CalcMetric[(int32)eMetric::PSNR] = true; m_CalcMetric[(int32)eMetric::SSIM] = true; }
            const std::string Dom = xToLowerCase(m_CfgParser.getParam1stArg("MetricDomain", std::string("YCbCr")));
            if     (Dom == "ycbcr") { m_DomainYCbCr = true ; m_DomainRGB = false; }
            else if(Dom == "rgb"  ) { m_DomainYCbCr = false; m_DomainRGB = true ; }
            else if(Dom == "both" ) { m_DomainYCbCr = true ; m_DomainRGB = true ; }
            else                    { m_ErrorLog += "!  MetricDomain is not valid (YCbCr, RGB, Both)\n"; Correct = false; }
        }
        Correct = readConfigurationCmnIV  () && Correct;
        m_CmpWeightsSearchRGB    = xCorrespPixelShiftPrms::c_EqualCmpWeights  ;
        m_CmpWeightsAverageRGB   = xCorrespPixelShiftPrms::c_EqualCmpWeights  ;
        m_CmpWeightsSearchYCbCr  = xCorrespPixelShiftPrms::c_DefaultCmpWeights;
        m_CmpWeightsAverageYCbCr = xCorrespPixelShiftPrms::c_DefaultCmpWeights;
        Correct = readConfigurationSSIM   () && Correct;
        Correct = readConfigurationValOp  () && Correct;
        derriveConfigurationCmn();
        m_UseMask = false;
        m_NumViews = 1; m_NumFrames = 1;
        return Correct;
    }
    std::string xAppGSPlayer::formatConfiguration()
    {
        std::string C;
        C += "Run-time configuration:\n";
        C += fmt::format("InputFile0        = {}\n", m_InputFile[0]);
        C += fmt::format("InputFile1        = {}\n", m_Sbs ? m_InputFile[1] : std::string("(unused)"));
        C += fmt::format("CamParamsFile     = {}\n", m_CamParamsFile);
        C += fmt::format("CamParamsFormat   = {}\n", xCamParamsFormatToStr(m_CamParamsFormat));
        C += fmt::format("PictureSize       = {}x{}{}\n", m_PictureSize[0], m_PictureSize[1], m_Sbs ? " (each of the two pictures)" : "");
        C += fmt::format("PlayFps           = {}\n", m_PlayFps);
        C += fmt::format("FramesAhead       = {}\n", m_NumAhead);
        C += fmt::format("MemoryFraction    = {}\n", m_MemFraction);
        C += fmt::format("MaxFps            = {}\n", m_MaxFps);
        if(m_Sbs) { formatConfigurationMetrics(C); C += fmt::format("MetricDomain      = {}\n", m_DomainYCbCr && m_DomainRGB ? "Both" : m_DomainRGB ? "RGB" : "YCbCr"); }
        C += fmt::format("NumberOfThreads   = {}\n", m_NumberOfThreads);
        return C;
    }
    eAppRes xAppGSPlayer::validateInputFiles()
    {
        //the cameras in their own resolution (a camera file without the size gets the picture size)
        if(!m_CamManager.loadRealCams(m_CamParamsFormat, m_CamParamsFile, m_CamNameTemplate, m_PictureSize))
        {
            m_ErrorLog += fmt::format("! failed to load {} camera parameters from CamParamsFile ({})\n", FVGS::xCamParamsFormatToStr(m_CamParamsFormat), m_CamParamsFile);
            return eAppRes::Error;
        }
        std::vector<GSR::tCamPar> Views;
        for(const int32 Idx : m_CamManager.getRealCamIdx()) { Views.push_back(m_CamManager.getCamParams(Idx)); }
        if(Views.empty()) { m_ErrorLog += "! no cameras in CamParamsFile\n"; return eAppRes::Error; }
        m_Camera.setViews(Views);

        m_Camera.setMode(m_StartMode);
        return eAppRes::Good;
    }
    int32 xAppGSPlayer::determineMaxNumTasks()
    {
        const int32 NumTiles = GSR::calcNumTiles(int32V2(8192, 8192)); //the window can be resized up to this
        return xMax(4096, 8192, NumTiles);
    }

    //===============================================================================================================================================================================================================
    // setup
    //===============================================================================================================================================================================================================

    int64 xAppGSPlayer::xGetFreeMemory(int64* Total)
    {
        #if defined(_WIN32)
        MEMORYSTATUSEX St; St.dwLength = sizeof(St);
        if(GlobalMemoryStatusEx(&St)) { if(Total) { *Total = (int64)St.ullTotalPhys; } return (int64)St.ullAvailPhys; }
        #else
        const long Pages = sysconf(_SC_PHYS_PAGES), Avail = sysconf(_SC_AVPHYS_PAGES), PageSize = sysconf(_SC_PAGE_SIZE);
        if(Pages > 0 && PageSize > 0) { if(Total) { *Total = (int64)Pages * PageSize; } return (int64)Avail * PageSize; }
        #endif
        if(Total) { *Total = 0; }
        return 0;
    }
    void xAppGSPlayer::xSetupBuffers()
    {
        for(int32 i = 0; i < c_NumPics; i++) { if(i == 0 || m_Sbs) { m_PicInP[i].create(m_PictureSize, m_BitDepth, m_PicMargin); m_PicInP[i].clear(); } }
        if(m_Sbs && m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicInI[i].create(m_PictureSize, m_BitDepth, m_PicMargin); } }
        if(m_Sbs && m_CalcSCP)
        {
            for(int32 i = 0; i < c_NumPics; i++) { m_PicSCP[i].create(m_PictureSize, m_BitDepth, m_PicMargin); }
            if(m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicSCI[i].create(m_PictureSize, m_BitDepth, m_PicMargin); } }
        }
    }
    void xAppGSPlayer::xCeaseBuffers()
    {
        for(int32 i = 0; i < c_NumPics; i++) { if(i == 0 || m_Sbs) { m_PicInP[i].destroy(); } }
        if(m_Sbs && m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicInI[i].destroy(); } }
        if(m_Sbs && m_CalcSCP)
        {
            for(int32 i = 0; i < c_NumPics; i++) { m_PicSCP[i].destroy(); }
            if(m_UsePicI) { for(int32 i = 0; i < c_NumPics; i++) { m_PicSCI[i].destroy(); } }
        }
    }
    void xAppGSPlayer::xCreateRenderer()
    {
        m_Renderer = new GSR::xRenderer();
        m_Renderer->create(m_PictureSize, m_ThreadPool);
        m_Renderer->setCapEffAlpha(m_SynthAlphaCap != 0);
        m_MaxSplatsReserved = 0;
        if(m_Sbs) { createMetricProcessors(); }
    }
    void xAppGSPlayer::xDestroyRenderer()
    {
        if(m_Sbs) { destroyMetricProcessors(); }
        if(m_Renderer) { m_Renderer->destroy(); delete m_Renderer; m_Renderer = nullptr; }
    }
    void xAppGSPlayer::xInitMetricStorage()
    {
        m_MetricVals.clear();
        for(int32 m = 0; m < c_MetricsNum; m++)
        {
            if(!m_CalcMetric[m]) { continue; }
            m_MetricData[m].initMetric((eMetric)m, 1, 1);
            m_MetricData[m].initSuffixes(false);
            m_MetricData[m].initCmpWeightsAverage(m_CmpWeightsAverageYCbCr, eClrSpcMtr::YCbCr);
            m_MetricData[m].initCmpWeightsAverage(m_CmpWeightsAverageRGB  , eClrSpcMtr::RGB  );
            for(eClrSpcMtr Cs : { eClrSpcMtr::YCbCr, eClrSpcMtr::RGB })
            {
                if((Cs == eClrSpcMtr::YCbCr && !m_DomainYCbCr) || (Cs == eClrSpcMtr::RGB && !m_DomainRGB)) { continue; }
                xMetricVal V; V.Metric = m; V.Domain = (int32)Cs; V.Pic = 0.0; V.Cmp = xMakeVec4(0.0); m_MetricVals.push_back(V);
            }
        }
    }
    eAppRes xAppGSPlayer::setupAll()
    {
        //multithreading: the pool of the renderer, the metrics and the loader
        m_HardwareConcurency = std::thread::hardware_concurrency();
        setupMultithreading();

        //the sequences and the frames read ahead
        std::string Err; const int32 NumClouds = m_Sbs ? 2 : 1;
        if(!m_Cache.open(NumClouds, m_InputFile, m_StartFrameCfg, m_NumberOfFrames, Err)) { xErrMsg::printError("ERROR --> " + Err); return eAppRes::Error; }
        m_NumFramesTotal = m_Cache.getNumFrames();
        const int64 FrameBytes = m_Cache.estimateFrameBytes();
        int64 TotalMem = 0; const int64 FreeMem = xGetFreeMemory(&TotalMem);
        int64 Budget = (int64)((flt64)TotalMem * m_MemFraction); if(FreeMem > 0) { Budget = std::min(Budget, (int64)((flt64)FreeMem * 0.85)); }
        int32 NumSlots = FrameBytes > 0 && Budget > 0 ? (int32)std::min<int64>(Budget / FrameBytes, m_NumAhead + 2) : (m_NumAhead + 2);
        NumSlots = std::max(NumSlots, std::min(2, m_NumFramesTotal));
        if(m_VerboseLevel >= 1) { fmt::print("Frames           = {}  (one frame of the clouds {:.1f} MB, {} read ahead, memory: free {:.1f} GB of {:.1f} GB)\n", m_NumFramesTotal, (flt64)FrameBytes / 1048576.0, std::min(NumSlots, m_NumFramesTotal) - 1, (flt64)FreeMem / 1073741824.0, (flt64)TotalMem / 1073741824.0); }
        if(NumSlots < 3 && m_NumFramesTotal > 2) { fmt::print("WARNING: the memory allows {} frame(s) at a time: the playback will stall waiting for the frames\n", NumSlots); }
        m_Cache.start(NumSlots, m_ThreadPool, determineMaxNumTasks());

        //the picture size of the window
        m_WinW = m_PictureSize[0] * (m_Sbs ? 2 : 1); m_WinH = m_PictureSize[1]; m_WinStamp = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        xSetupBuffers();
        xCreateRenderer();
        if(m_Sbs) { xInitMetricStorage(); }
        m_Playing = m_NumFramesTotal > 1;
        m_T = 0.0; m_FpsStamp = std::chrono::steady_clock::now();
        return eAppRes::Good;
    }
    void xAppGSPlayer::ceaseAll()
    {
        { std::lock_guard<std::mutex> Lk(m_Mx); m_Quit = true; }
        m_Cv.notify_all();
        if(m_RenderThread.joinable()) { m_RenderThread.join(); }
        if(m_CurSlot) { m_Cache.release(m_CurSlot); m_CurSlot = nullptr; }
        m_Cache.stop();
        xDestroyRenderer();
        xCeaseBuffers();
        ceaseMultithreading();
    }

    //===============================================================================================================================================================================================================
    // the timeline
    //===============================================================================================================================================================================================================

    int32 xAppGSPlayer::xTimelineLength() const
    {
        return xMax(1, m_NumFramesTotal);
    }
    int32 xAppGSPlayer::xFrameOfT(double T) const
    {
        const int32 F = (int32)std::floor(T);
        return xMax(0, std::min(F, m_NumFramesTotal - 1));
    }
    void xAppGSPlayer::xPrefetch() //m_Mx held
    {
        std::vector<int32> W; const int32 L = xTimelineLength();
        const int32 N = std::min(m_Cache.getNumSlots(), m_NumAhead + 1);
        for(int32 k = 0; k < N; k++)
        {
            int32 P = (int32)std::floor(m_T) + k;
            if(m_Loop) { P %= L; } else { P = std::min(P, L - 1); }
            const int32 F = xFrameOfT((double)P);
            if(std::find(W.begin(), W.end(), F) == W.end()) { W.push_back(F); }
        }
        m_Cache.setWanted(W);
    }
    void xAppGSPlayer::xSetMsg(const std::string& Msg, int32 Ms)
    {
        m_Msg = Msg; m_MsgUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(Ms);
    }

    //===============================================================================================================================================================================================================
    // the metrics of the pair (the code of QMIV-GS calcMetrics for the clouds of one view)
    //===============================================================================================================================================================================================================

    void xAppGSPlayer::xSetProcessorsCmpWeights(const int32V4& CmpWeightsSearch, const int32V4& CmpWeightsAverage)
    {
        m_CmpWeightsSearch  = CmpWeightsSearch ;
        m_CmpWeightsAverage = CmpWeightsAverage;
        if(m_CalcSCP  ) { m_ProcSCP.setCmpWeightsSearch (m_CmpWeightsSearch ); m_ProcSCP.setCmpWeightsAverage(m_CmpWeightsAverage); }
        if(m_CalcPSNRs) { m_ProcPSNR.setCmpWeightsSearch(m_CmpWeightsSearch ); m_ProcPSNR.setCmpWeightsAverage(m_CmpWeightsAverage); }
        if(m_CalcSSIMs) { m_ProcSSIM.setCmpWeightsSearch(m_CmpWeightsSearch ); m_ProcSSIM.setCmpWeightsAverage(m_CmpWeightsAverage); }
    }
    //bands of rows of the pictures for the tasks of the pool (about 2 tasks per thread)
    static int32 xNumBandsPl(const tThPI& TPI, int32 Parts, int32 Height)
    {
        return std::max(1, std::min((2 * std::max(1, TPI.getNumThreads()) + Parts - 1) / Parts, Height / 64));
    }
    static void xEqualCmpsParallelPl(tThPI& TPI, const xPicP& A, const xPicP& B, boolV4& Exact)
    {
        const int32 NumCmps = A.getNumCmps(), Height = A.getHeight(), NumBands = xNumBandsPl(TPI, NumCmps, Height);
        std::vector<uint8> Eq((size_t)NumCmps * NumBands, 1);
        for(int32 c = 0; c < NumCmps; c++)
        {
            for(int32 b = 0; b < NumBands; b++)
            {
                TPI.storeTask([&Eq, &A, &B, c, b, NumBands, Height](int32)
                    {
                        const int32 y0 = (int32)((int64)Height * b / NumBands), y1 = (int32)((int64)Height * (b + 1) / NumBands);
                        Eq[(size_t)c * NumBands + b] = xPixelOps::CompareEqual(B.getAddr((eCmp)c) + (size_t)y0 * B.getStride(), A.getAddr((eCmp)c) + (size_t)y0 * A.getStride(), B.getStride(), A.getStride(), A.getWidth(), y1 - y0) ? 1 : 0;
                    });
            }
        }
        TPI.executeStoredTasks();
        for(int32 c = 0; c < NumCmps; c++) { bool All = true; for(int32 b = 0; b < NumBands; b++) { All = All && Eq[(size_t)c * NumBands + b] != 0; } Exact[c] = All; }
    }
    void xAppGSPlayer::xPreprocessRGB()
    {
        xEqualCmpsParallelPl(m_TPI, m_PicInP[0], m_PicInP[1], m_ExactCmps);
    }
    void xAppGSPlayer::xPreprocessYCbCr()
    {
        const eClrSpcLC ColorSpace = xClrSpcAppToClrSpc(eClrSpcApp::YCbCr_BT709);
        const int32 Height = m_PicInP[0].getHeight(), NumBands = xNumBandsPl(m_TPI, c_NumPics, Height);
        for(int32 i = 0; i < c_NumPics; i++)
        {
            for(int32 b = 0; b < NumBands; b++)
            {
                m_TPI.storeTask([this, i, b, NumBands, Height, ColorSpace](int32)
                    {
                        const int32 y0 = (int32)((int64)Height * b / NumBands), y1 = (int32)((int64)Height * (b + 1) / NumBands); const size_t Ofs = (size_t)y0 * m_PicInP[i].getStride();
                        xColorSpace::ConvertRGB2YCbCr(
                            m_PicInP[i].getAddr(eCmp::R ) + Ofs, m_PicInP[i].getAddr(eCmp::G ) + Ofs, m_PicInP[i].getAddr(eCmp::B ) + Ofs,
                            m_PicInP[i].getAddr(eCmp::LM) + Ofs, m_PicInP[i].getAddr(eCmp::CB) + Ofs, m_PicInP[i].getAddr(eCmp::CR) + Ofs,
                            m_PicInP[i].getStride(), m_PicInP[i].getStride(), m_PicInP[i].getWidth(), y1 - y0, m_PicInP[i].getBitDepth(), ColorSpace);
                    });
            }
        }
        m_TPI.executeStoredTasks();
        xEqualCmpsParallelPl(m_TPI, m_PicInP[0], m_PicInP[1], m_ExactCmps);
    }
    void xAppGSPlayer::xStoreMetric(int32 Metric, eClrSpcMtr ClrSpc)
    {
        for(xMetricVal& V : m_MetricVals)
        {
            if(V.Metric != Metric || V.Domain != (int32)ClrSpc) { continue; }
            const xMetricStatPeek& Peek = static_cast<const xMetricStatPeek&>(m_MetricData[Metric]);
            if(xMetricInfo::Metrics[Metric].IsPerCmp) { V.Cmp = Peek.getValCmp(0, 0, ClrSpc); V.Pic = Peek.getValPic(0, 0, ClrSpc); } //the picture value of a per-component metric: the weighted mean
            else                                      { V.Pic = Peek.getValPic(0, 0, ClrSpc); V.Cmp = xMakeVec4(0.0); }
            V.Sum += V.Pic; V.Count++;
            V.Hist.push_back((float)V.Pic); if((int32)V.Hist.size() > gsp::MaxPlot) { V.Hist.erase(V.Hist.begin()); }
        }
    }
    void xAppGSPlayer::xCalcMetricsDomain(eClrSpcMtr ClrSpc)
    {
        constexpr int32 F = 0, V = 0;
        if(m_CalcGCD) { calcFrameGCD(F, V); }
        if(m_CalcSCP) { calcFrameSCP(F, V); }
        if(getCalcMetric(eMetric::MSE     )) { calcFrame_____MSE(F, V, ClrSpc); xStoreMetric((int32)eMetric::MSE     , ClrSpc); }
        if(getCalcMetric(eMetric::PSNR    )) { calcFrame____PSNR(F, V, ClrSpc); xStoreMetric((int32)eMetric::PSNR    , ClrSpc); }
        if(getCalcMetric(eMetric::WSPSNR  )) { calcFrame__WSPSNR(F, V, ClrSpc); xStoreMetric((int32)eMetric::WSPSNR  , ClrSpc); }
        if(getCalcMetric(eMetric::IVPSNR  )) { calcFrame__IVPSNR(F, V, ClrSpc); xStoreMetric((int32)eMetric::IVPSNR  , ClrSpc); }
        if(m_StructSimBrdExt != eMrgExt::None) { addStructSimMargs(F); }
        if(getCalcMetric(eMetric::SSIM    )) { calcFrame____SSIM(F, V, ClrSpc); xStoreMetric((int32)eMetric::SSIM    , ClrSpc); }
        if(getCalcMetric(eMetric::MSSSIM  )) { calcFrame__MSSSIM(F, V, ClrSpc); xStoreMetric((int32)eMetric::MSSSIM  , ClrSpc); }
        if(getCalcMetric(eMetric::IVSSIM  )) { calcFrame__IVSSIM(F, V, ClrSpc); xStoreMetric((int32)eMetric::IVSSIM  , ClrSpc); }
        if(getCalcMetric(eMetric::IVMSSSIM)) { calcFrameIVMSSSIM(F, V, ClrSpc); xStoreMetric((int32)eMetric::IVMSSSIM, ClrSpc); }
        if(getCalcMetric(eMetric::PVD     )) { calcFrame_____PVD(F, V, ClrSpc); xStoreMetric((int32)eMetric::PVD     , ClrSpc); }
    }
    void xAppGSPlayer::xEvalMetrics()
    {
        const auto T0 = std::chrono::steady_clock::now();
        addStructSimMargs(0); //(the name of the QMIV-GS step) extends the margins of the pictures, which the IV-metrics and SSIM search
        if(m_DomainRGB)
        {
            xSetProcessorsCmpWeights(m_CmpWeightsSearchRGB, m_CmpWeightsAverageRGB);
            xPreprocessRGB(); rearrangePictures(0);
            xCalcMetricsDomain(eClrSpcMtr::RGB);
        }
        if(m_DomainYCbCr)
        {
            xSetProcessorsCmpWeights(m_CmpWeightsSearchYCbCr, m_CmpWeightsAverageYCbCr);
            xPreprocessYCbCr(); rearrangePictures(0);
            xCalcMetricsDomain(eClrSpcMtr::YCbCr);
        }
        m_MetricMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
    }

    //===============================================================================================================================================================================================================
    // rendering
    //===============================================================================================================================================================================================================

    //renders the pair (or the cloud) of the slot with the camera into m_PicInP and converts the pictures side by side to RGB8
    bool xAppGSPlayer::xRenderOnce(const GSR::tCamPar& Cam, xGspCache::xSlot* Slot, std::vector<uint8_t>& Out, int32& OutW, int32& OutH)
    {
        const int32 NumClouds = m_Sbs ? 2 : 1;
        for(int32 c = 0; c < NumClouds; c++)
        {
            FVGS::xGaussianCloudI& Cloud = Slot->Cloud[c];
            if(Cloud.getNumValidSplats() > m_MaxSplatsReserved) { m_MaxSplatsReserved = Cloud.getNumValidSplats(); }
            m_Renderer->renderFrame(&m_PicInP[c], nullptr, &Cloud, Cam);
        }
        const int32 W = m_PictureSize[0], H = m_PictureSize[1];
        OutW = W * NumClouds; OutH = H;
        Out.resize((size_t)OutW * OutH * 3);
        for(int32 c = 0; c < NumClouds; c++)
        {
            const xPicP& P = m_PicInP[c];
            const uint16* R = P.getAddr(eCmp::R), *G = P.getAddr(eCmp::G), *B = P.getAddr(eCmp::B); const int32 St = P.getStride();
            for(int32 y = 0; y < H; y++)
            {
                uint8_t* D = Out.data() + ((size_t)y * OutW + (size_t)c * W) * 3;
                const uint16* Rr = R + (size_t)y * St, *Gg = G + (size_t)y * St, *Bb = B + (size_t)y * St;
                for(int32 x = 0; x < W; x++) { D[3 * x] = (uint8_t)std::min<uint16>(Rr[x], 255); D[3 * x + 1] = (uint8_t)std::min<uint16>(Gg[x], 255); D[3 * x + 2] = (uint8_t)std::min<uint16>(Bb[x], 255); }
            }
        }
        return true;
    }
    void xAppGSPlayer::xHandleCommands() //m_Mx held
    {
        static const double c_Rates[] = { 1, 2, 5, 10, 15, 24, 25, 30, 48, 50, 60, 90, 120 };
        while(!m_Cmds.empty())
        {
            const xCmd C = m_Cmds.front(); m_Cmds.pop_front(); m_Dirty = true;
            switch(C.Cmd)
            {
            case gsp::CmdTogglePlay: m_Playing = !m_Playing; break;
            case gsp::CmdStepFwd:  m_Playing = false; m_T = std::min(m_T + 1.0, (double)xTimelineLength() - 1.0); break;
            case gsp::CmdStepBack: m_Playing = false; m_T = std::max(m_T - 1.0, 0.0); break;
            case gsp::CmdSeek:     m_T = (double)std::max(0, std::min(C.Arg, xTimelineLength() - 1)); for(auto& V : m_MetricVals) { V.Sum = 0; V.Count = 0; } break;
            case gsp::CmdNextView: if(m_Camera.getNumViews() > 0) { if(m_Camera.getMode() != gsp::ModeViews) { m_Camera.setMode(gsp::ModeViews); } else { m_Camera.nextView(+1); } } break;
            case gsp::CmdPrevView: if(m_Camera.getNumViews() > 0) { if(m_Camera.getMode() != gsp::ModeViews) { m_Camera.setMode(gsp::ModeViews); } else { m_Camera.nextView(-1); } } break;
            case gsp::CmdMode:
            {
                const int32 Old = m_Camera.getMode();
                m_Camera.setMode(C.Arg);
                if(m_Camera.getMode() != Old)
                {
                    for(auto& V : m_MetricVals) { V.Sum = 0; V.Count = 0; }
                }
                break;
            }
            case gsp::CmdResetCamera: m_Camera.resetFree(); if(m_Camera.getMode() != gsp::ModeFree) { m_Camera.setMode(gsp::ModeFree); } break;
            case gsp::CmdToggleRecord:
                if(!m_Recording) { if(m_Camera.getMode() != gsp::ModeFree) { xSetMsg("recording works in the free flight (F1)"); break; } m_Recording = true; m_RecPoses.clear(); m_RecAcc = 0.0; xSetMsg("recording the flight: press R to stop", 60000); }
                else { m_Recording = false; xSaveRecordedPath(); }
                break;
            case gsp::CmdScreenshot: m_ShotReq = true; break;
            case gsp::CmdCycleMetric: if(!m_MetricVals.empty()) { m_PlotMetric = (m_PlotMetric + 1) % (int32)m_MetricVals.size(); } break;
            case gsp::CmdLoop: m_Loop = !m_Loop; xSetMsg(m_Loop ? "loop on" : "loop off"); break;
            case gsp::CmdPlaySpeed:
            {
                int32 k = 0; for(int32 i = 0; i < (int32)(sizeof(c_Rates) / sizeof(c_Rates[0])); i++) { if(c_Rates[i] <= m_PlayFps + 1e-9) { k = i; } }
                k = std::max(0, std::min((int32)(sizeof(c_Rates) / sizeof(c_Rates[0])) - 1, k + C.Arg)); m_PlayFps = c_Rates[k]; xSetMsg(fmt::format("playback {} fps", m_PlayFps)); break;
            }
            default: break;
            }
        }
    }
    void xAppGSPlayer::xSaveRecordedPath() //m_Mx held
    {
        if(m_RecPoses.empty()) { xSetMsg("nothing recorded"); return; }
        FILE* F = std::fopen(m_RecordPath.c_str(), "w");
        if(!F) { xSetMsg("cannot write " + m_RecordPath); return; }
        std::fprintf(F, "X,Y,Z,Yaw,Pitch,Roll\n");
        for(const auto& P : m_RecPoses) { std::fprintf(F, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", P[0], P[1], P[2], P[3], P[4], P[5]); }
        std::fclose(F);
        xSetMsg(fmt::format("path saved: {} ({} poses)", m_RecordPath, (int32)m_RecPoses.size()), 4000);
    }
    void xAppGSPlayer::xApplyWindowSize()
    {
        //the picture size from the window (stable for a moment): the renderer and the buffers are made again
        int32 W, H; bool Stable;
        {
            std::lock_guard<std::mutex> Lk(m_Mx);
            W = (std::max(64, m_Sbs ? m_WinW / 2 : m_WinW)) & ~1; H = std::max(64, m_WinH) & ~1;
            Stable = std::chrono::steady_clock::now() - m_WinStamp > std::chrono::milliseconds(200);
        }
        if(!Stable || (W == m_PictureSize[0] && H == m_PictureSize[1])) { return; }
        xDestroyRenderer(); xCeaseBuffers();
        { std::lock_guard<std::mutex> Lk(m_Mx); m_PictureSize = int32V2(W, H); }
        xSetupBuffers(); xCreateRenderer();
        { std::lock_guard<std::mutex> Lk(m_Mx); m_Dirty = true; }
    }
    void xAppGSPlayer::xRenderLoop()
    {
        using Clk = std::chrono::steady_clock;
        auto Last = Clk::now(); int32 Iter = 0; bool FirstDone = false;
        std::unique_lock<std::mutex> Lk(m_Mx);
        while(true)
        {
            m_Cv.wait_for(Lk, std::chrono::milliseconds(m_Playing ? 4 : 50), [&]() { return m_Quit || m_Dirty || !m_Cmds.empty(); });
            if(m_Quit) { break; }
            const auto Now = Clk::now(); double Dt = std::chrono::duration<double>(Now - Last).count(); Last = Now; Dt = std::min(Dt, 0.25);
            const int32 OldFrameT = (int32)std::floor(m_T);
            xHandleCommands();

            //recording of the flight at the rate of the playback
            if(m_Recording && m_Camera.getMode() == gsp::ModeFree)
            {
                m_RecAcc += Dt; const double Step = 1.0 / m_PlayFps;
                while(m_RecAcc >= Step) { m_RecPoses.push_back(m_Camera.getPoseYPR()); m_RecAcc -= Step; }
            }

            //the position on the timeline
            const int32 L = xTimelineLength();
            const bool Moving = m_Playing && m_NumFramesTotal > 1;
            double Cand = m_T; if(Moving) { Cand += Dt * m_PlayFps; }
            if(Cand >= (double)L) { if(m_Loop) { Cand = std::fmod(Cand, (double)L); } else { Cand = (double)L - 1e-6; m_Playing = false; } }
            int32 WantFrame = xFrameOfT(Cand);
            xGspCache::xSlot* Slot = (WantFrame == m_CurFrame && m_CurSlot) ? m_CurSlot : m_Cache.acquire(WantFrame, 0);
            bool NewFrame = false;
            if(Slot == nullptr)
            {
                m_Buffering = true; Cand = m_T; WantFrame = m_CurFrame;   //the frame is not ready: the clock waits
                Slot = m_CurSlot;
            }
            else
            {
                m_Buffering = false;
                if(Slot != m_CurSlot) { if(m_CurSlot) { m_Cache.release(m_CurSlot); } m_CurSlot = Slot; m_CurFrame = WantFrame; NewFrame = true; }
                if(!m_SpeedSet) { m_Camera.estimateSpeed(Slot->Cloud[0].getGaussianObjects(), Slot->Cloud[0].getNumValidSplats()); m_SpeedSet = true; }
            }
            m_T = Cand;
            xPrefetch();
            const bool NeedRender = Slot != nullptr && (NewFrame || m_Dirty);
            if(Slot == nullptr) { m_Dirty = true; Lk.unlock(); std::this_thread::sleep_for(std::chrono::milliseconds(5)); Lk.lock(); continue; }
            if(!NeedRender) { continue; }
            m_Dirty = false;
            const int32 PoseIdx = (int32)std::floor(m_T);
            const GSR::tCamPar Cam = m_Camera.build(m_PictureSize[0], m_PictureSize[1], PoseIdx);
            const bool Force = !m_Playing || !FirstDone;
            const bool ShotReq = m_ShotReq; m_ShotReq = false;
            Lk.unlock();

            xApplyWindowSize();
            const auto T0 = Clk::now();
            std::vector<uint8_t>& Back = m_Out[1 - m_OutFront]; int32 OutW = 0, OutH = 0;
            //after a resize the camera is made again for the new size
            const GSR::tCamPar Cam2 = (Cam.getResolution().getX() == m_PictureSize[0] && Cam.getResolution().getY() == m_PictureSize[1]) ? Cam : xGspCamera::xRescale(Cam, m_PictureSize[0], m_PictureSize[1]);
            xRenderOnce(Cam2, Slot, Back, OutW, OutH);
            const double RenderMs = std::chrono::duration<double, std::milli>(Clk::now() - T0).count();
            { std::lock_guard<std::mutex> OLk(m_OutMx); m_OutFront = 1 - m_OutFront; m_OutW = OutW; m_OutH = OutH; m_OutId++; }
            FirstDone = true;
            if(ShotReq)
            {
                std::string Path; for(int32 n = m_ShotCount; ; n++) { Path = fmt::format("{}_{:04d}.png", m_ShotPrefix, n); if(!std::filesystem::exists(Path)) { m_ShotCount = n + 1; break; } }
                const bool Ok = uiWritePng(Path.c_str(), m_Out[m_OutFront].data(), OutW, OutH);
                std::lock_guard<std::mutex> MLk(m_Mx); xSetMsg(Ok ? "screenshot: " + Path : "screenshot failed: " + Path);
            }

            //the metrics of the pair, as often as they leave room for the renders
            if(m_Sbs && !m_MetricVals.empty())
            {
                if(Force || (m_MetricSkip++ % m_MetricEveryN) == 0)
                {
                    xEvalMetrics();
                    m_MetricEveryN = std::max(1, std::min(30, (int32)std::ceil(m_MetricMs / std::max(1.0, 1.5 * (m_RenderMsEma > 0.0 ? m_RenderMsEma : RenderMs)))));
                }
            }

            //pacing and statistics
            m_RenderMsEma = m_RenderMsEma > 0.0 ? 0.9 * m_RenderMsEma + 0.1 * RenderMs : RenderMs;
            m_FramesInWindow++;
            const double Win = std::chrono::duration<double>(Clk::now() - m_FpsStamp).count();
            if(Win >= 0.5) { m_RenderFps = (double)m_FramesInWindow / Win; m_FramesInWindow = 0; m_FpsStamp = Clk::now(); }
            const double MinPeriodMs = 1000.0 / (double)m_MaxFps, Spent = std::chrono::duration<double, std::milli>(Clk::now() - T0).count();
            if(Spent < MinPeriodMs) { std::this_thread::sleep_for(std::chrono::microseconds((int64)((MinPeriodMs - Spent) * 1000.0))); }
            Iter++;
            Lk.lock();
            if(m_Camera.getMode() == gsp::ModeFree && m_Playing == false) { /* a still image: nothing to do until something changes */ }
        }
    }

    //===============================================================================================================================================================================================================
    // the window (its thread)
    //===============================================================================================================================================================================================================

    void xAppGSPlayer::xParseScript(const std::string& S)
    {
        auto Split = [](const std::string& Str, char Sep) { std::vector<std::string> V; size_t B = 0; while(true) { const size_t E = Str.find(Sep, B); V.push_back(Str.substr(B, E == std::string::npos ? E : E - B)); if(E == std::string::npos) { break; } B = E + 1; } return V; };
        static const struct { const char* Name; int32 Cmd; } c_Names[] = { { "toggle", gsp::CmdTogglePlay }, { "stepf", gsp::CmdStepFwd }, { "stepb", gsp::CmdStepBack }, { "seek", gsp::CmdSeek }, { "next", gsp::CmdNextView }, { "prev", gsp::CmdPrevView },
            { "mode", gsp::CmdMode }, { "reset", gsp::CmdResetCamera }, { "record", gsp::CmdToggleRecord }, { "shot", gsp::CmdScreenshot }, { "metric", gsp::CmdCycleMetric }, { "loop", gsp::CmdLoop }, { "speed", gsp::CmdPlaySpeed } };
        for(const std::string& Item : Split(S, ';'))
        {
            if(Item.empty()) { continue; }
            const std::vector<std::string> P = Split(Item, ':'); if(P.size() < 2) { continue; }
            xScriptStep St; St.Ms = std::atof(P[0].c_str()); St.Cmd = gsp::CmdNone; St.Arg = 0; St.IsInput = false;
            const std::string& N = P[1];
            if(N == "move" && P.size() >= 6) { St.IsInput = true; St.In.Move[0] = (float)std::atof(P[2].c_str()); St.In.Move[1] = (float)std::atof(P[3].c_str()); St.In.Move[2] = (float)std::atof(P[4].c_str()); St.In.Dt = (float)std::atof(P[5].c_str()) / 1000.0f; St.In.Speed = 1.0f; }
            else if(N == "look" && P.size() >= 4) { St.IsInput = true; St.In.Look[0] = (float)std::atof(P[2].c_str()); St.In.Look[1] = (float)std::atof(P[3].c_str()); St.In.Dt = 0.016f; St.In.Speed = 1.0f; }
            else if(N == "size" && P.size() >= 4) { St.SizeW = std::atoi(P[2].c_str()); St.SizeH = std::atoi(P[3].c_str()); } //the window gets another size (the UI is told as by the window)
            else if(N == "roll" && P.size() >= 4) { St.IsInput = true; St.In.Roll = (float)std::atof(P[2].c_str()); St.In.Dt = (float)std::atof(P[3].c_str()) / 1000.0f; St.In.Speed = 1.0f; }
            else { for(const auto& E : c_Names) { if(N == E.Name) { St.Cmd = E.Cmd; } } if(P.size() >= 3) { St.Arg = std::atoi(P[2].c_str()); } if(St.Cmd == gsp::CmdNone) { continue; } }
            m_Script.push_back(St);
        }
        std::stable_sort(m_Script.begin(), m_Script.end(), [](const xScriptStep& A, const xScriptStep& B) { return A.Ms < B.Ms; });
    }
    void xAppGSPlayer::xRunScript()
    {
        if(m_Script.empty()) { return; }
        if(!m_ScriptStarted) { m_ScriptStarted = true; m_ScriptT0 = std::chrono::steady_clock::now(); }
        const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_ScriptT0).count();
        while(m_ScriptPos < m_Script.size() && m_Script[m_ScriptPos].Ms <= Ms)
        {
            const xScriptStep& St = m_Script[m_ScriptPos++];
            if(St.SizeW > 0) { uiResize(St.SizeW, St.SizeH); } else if(St.IsInput) { uiInput(St.In); } else { uiCommand(St.Cmd, St.Arg); }
        }
    }

    bool xAppGSPlayer::uiPollFrame(gsp::Frame& F)
    {
        std::unique_lock<std::mutex> Lk(m_OutMx);
        if(m_OutId == m_OutPolled || m_OutW == 0) { return false; }
        m_OutPolled = m_OutId;
        F.Rgb = m_Out[m_OutFront].data(); F.W = m_OutW; F.H = m_OutH; F.Id = m_OutId;
        m_OutLock = std::move(Lk);
        return true;
    }
    void xAppGSPlayer::uiReleaseFrame()
    {
        if(m_OutLock.owns_lock()) { m_OutLock.unlock(); }
    }
    void xAppGSPlayer::uiInput(const gsp::Input& In)
    {
        std::lock_guard<std::mutex> Lk(m_Mx);
        if(m_Camera.update(In)) { m_Dirty = true; m_Cv.notify_all(); }
    }
    void xAppGSPlayer::uiCommand(int32 Cmd, int32 Arg)
    {
        if(Cmd == gsp::CmdQuit) { return; }
        { std::lock_guard<std::mutex> Lk(m_Mx); m_Cmds.push_back({ Cmd, Arg }); }
        m_Cv.notify_all();
    }
    void xAppGSPlayer::uiResize(int32 W, int32 H)
    {
        { std::lock_guard<std::mutex> Lk(m_Mx); m_WinW = W; m_WinH = H; m_WinStamp = std::chrono::steady_clock::now(); m_Dirty = true; }
        m_Cv.notify_all();
    }
    void xAppGSPlayer::uiFillHud(gsp::Hud& H)
    {
        xRunScript();
        std::lock_guard<std::mutex> Lk(m_Mx);
        const int32 Mode = m_Camera.getMode();
        H.Mode = Mode; H.Playing = m_Playing; H.Buffering = m_Buffering; H.Sbs = m_Sbs; H.Recording = m_Recording; H.Loop = m_Loop;
        H.NumFrames = m_NumFramesTotal; H.CurFrame = (int32)std::floor(m_T);
        H.NumViews = m_Camera.getNumViews(); H.CurView = m_Camera.getViewIdx();
        H.RenderMs = (float)m_RenderMsEma; H.RenderFps = (float)m_RenderFps; H.PlayFps = (float)m_PlayFps;
        const char* ModeName = Mode == gsp::ModeFree ? "FREE" : Mode == gsp::ModeViews ? "VIEWS" : "TRACE";
        std::snprintf(H.Line1, sizeof(H.Line1), "%s   frame %d/%d   %s %.0f fps%s   %dx%d%s%s", ModeName, (int32)std::floor(m_T) + 1, H.NumFrames, m_Playing ? "play" : "pause", m_PlayFps, m_Loop ? " loop" : "",
            m_PictureSize[0], m_PictureSize[1], m_Sbs ? " x2" : "", m_Buffering ? "   BUFFERING" : "");
        std::snprintf(H.Line2, sizeof(H.Line2), "%s%s", m_Camera.formatInfo((int32)std::floor(m_T)).c_str(), m_Recording ? "   [REC]" : "");
        if(!m_Msg.empty() && std::chrono::steady_clock::now() < m_MsgUntil) { std::snprintf(H.Message, sizeof(H.Message), "%s", m_Msg.c_str()); }
        else { H.Message[0] = 0; }
        const std::string CacheErr = m_Cache.getError(); if(!CacheErr.empty() && H.Message[0] == 0) { std::snprintf(H.Message, sizeof(H.Message), "%s", CacheErr.c_str()); }
        //metrics
        H.NumRows = 0; H.PlotN = 0; H.PlotName[0] = 0;
        for(int32 i = 0; i < (int32)m_MetricVals.size() && H.NumRows < gsp::MaxRows; i++)
        {
            const xMetricVal& V = m_MetricVals[(size_t)i]; if(V.Count == 0) { continue; }
            gsp::MetricRow& R = H.Rows[H.NumRows++];
            const bool Ps = xMetricInfo::Metrics[V.Metric].Unit == "dB";
            std::snprintf(R.Name, sizeof(R.Name), "%s (%s)", xMetricToStr((eMetric)V.Metric).c_str(), V.Domain == (int32)eClrSpcMtr::YCbCr ? "YCbCr" : "RGB");
            std::snprintf(R.Value, sizeof(R.Value), Ps ? "%.3f dB" : "%.5f", V.Pic);
            std::snprintf(R.Avg  , sizeof(R.Avg  ), Ps ? "%.3f dB" : "%.5f", V.Sum / (double)V.Count);
            const char* N0 = V.Domain == (int32)eClrSpcMtr::YCbCr ? "Y" : "R"; const char* N1 = V.Domain == (int32)eClrSpcMtr::YCbCr ? "Cb" : "G"; const char* N2 = V.Domain == (int32)eClrSpcMtr::YCbCr ? "Cr" : "B";
            if(xMetricInfo::Metrics[V.Metric].IsPerCmp) { std::snprintf(R.Comp, sizeof(R.Comp), Ps ? "%s %.2f  %s %.2f  %s %.2f" : "%s %.4f  %s %.4f  %s %.4f", N0, V.Cmp[0], N1, V.Cmp[1], N2, V.Cmp[2]); }
            else { R.Comp[0] = 0; }
        }
        if(!m_MetricVals.empty())
        {
            const xMetricVal& V = m_MetricVals[(size_t)(m_PlotMetric % (int32)m_MetricVals.size())];
            H.PlotN = (int32)std::min<size_t>(V.Hist.size(), gsp::MaxPlot);
            if(H.PlotN > 1)
            {
                float Mn = 1e30f, Mx = -1e30f;
                for(int32 i = 0; i < H.PlotN; i++) { H.Plot[i] = V.Hist[V.Hist.size() - (size_t)H.PlotN + (size_t)i]; Mn = std::min(Mn, H.Plot[i]); Mx = std::max(Mx, H.Plot[i]); }
                const float Pad = std::max(1e-6f, 0.1f * (Mx - Mn)); H.PlotMin = Mn - Pad; H.PlotMax = Mx + Pad;
                std::snprintf(H.PlotName, sizeof(H.PlotName), "%s", xMetricToStr((eMetric)V.Metric).c_str());
            }
        }
    }
    bool xAppGSPlayer::uiWritePng(const char* Path, const uint8_t* Rgb, int32 W, int32 H)
    {
        spng_ctx* Ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        if(!Ctx) { return false; }
        spng_set_option(Ctx, SPNG_ENCODE_TO_BUFFER, 1);
        spng_ihdr Ihdr; std::memset(&Ihdr, 0, sizeof(Ihdr)); Ihdr.width = (uint32_t)W; Ihdr.height = (uint32_t)H; Ihdr.bit_depth = 8; Ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR;
        bool Ok = spng_set_ihdr(Ctx, &Ihdr) == 0 && spng_encode_image(Ctx, Rgb, (size_t)W * H * 3, SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE) == 0;
        if(Ok)
        {
            int Err = 0; size_t Len = 0; void* Buf = spng_get_png_buffer(Ctx, &Len, &Err);
            if(Buf && Err == 0) { FILE* F = std::fopen(Path, "wb"); if(F) { Ok = std::fwrite(Buf, 1, Len, F) == Len; std::fclose(F); } else { Ok = false; } } else { Ok = false; }
            std::free(Buf);
        }
        spng_ctx_free(Ctx);
        return Ok;
    }

    //===============================================================================================================================================================================================================
    // run
    //===============================================================================================================================================================================================================

    namespace {
        class xGspHost : public gsp::Host
        {
            xAppGSPlayer& m_App;
        public:
            xGspHost(xAppGSPlayer& App) : m_App(App) {}
            bool pollFrame   (gsp::Frame& F) override { return m_App.uiPollFrame(F); }
            void releaseFrame()              override { m_App.uiReleaseFrame(); }
            void fillHud     (gsp::Hud& H)   override { m_App.uiFillHud(H); }
            void onInput     (const gsp::Input& I) override { m_App.uiInput(I); }
            void onCommand   (int Cmd, int Arg)    override { m_App.uiCommand(Cmd, Arg); }
            void onResize    (int W, int H)        override { m_App.uiResize(W, H); }
            bool writePng    (const char* Path, const uint8_t* Rgb, int W, int H) override { return m_App.uiWritePng(Path, Rgb, W, H); }
        };
    }

    std::string xAppGSPlayer::formatBenchLine(int32 i, double Ms) { return fmt::format("render {:4d}  {:7.2f} ms", i, Ms); }

    int32 xAppGSPlayer::run()
    {
        if(m_BenchFrames > 0)
        {
            //no window: the input views one after another (the frames in order), the render, the display conversion and the metrics as in the window
            const int32 NumViews = xMax(1, m_Camera.getNumViews());
            const int32 BenchMode = m_StartModeGiven ? m_StartMode : (int32)gsp::ModeViews; //0: the free camera at the pose of the first view, 1 (default): the input views, 2: the poses of the posetrace (the cloud frames mirrored as in QMIV-GS)
            m_Camera.setMode(BenchMode);
            const int32 Period = NumViews;
            xGspCache::xSlot* Slot = nullptr; int32 Frame = NOT_VALID;
            xXXH128 HashSynth; HashSynth.init();
            double SumMs = 0.0, SumMetricMs = 0.0; int32 NumMetric = 0; std::vector<uint8_t> Out; int32 OutW = 0, OutH = 0;
            const auto TAll = std::chrono::steady_clock::now();
            for(int32 i = 0; i < m_BenchFrames; i++)
            {
                const int32 F = BenchMode == (i / Period) % m_NumFramesTotal;
                if(BenchMode == gsp::ModeViews) { m_Camera.setViewIdx(i % NumViews); }
                if(F != Frame)
                {
                    if(Slot) { m_Cache.release(Slot); }
                    std::vector<int32> W; for(int32 k = 0; k < std::min(m_Cache.getNumSlots(), m_NumAhead + 1); k++) { const int32 Fk = (F + k) % m_NumFramesTotal; if(std::find(W.begin(), W.end(), Fk) == W.end()) { W.push_back(Fk); } }
                    m_Cache.setWanted(W);
                    Slot = m_Cache.acquire(F, 120000);
                    if(!Slot) { xErrMsg::printError("ERROR --> frame " + std::to_string(F) + " was not loaded: " + m_Cache.getError()); return 1; }
                    Frame = F;
                }
                const GSR::tCamPar Cam = m_Camera.build(m_PictureSize[0], m_PictureSize[1], i);
                const auto T0 = std::chrono::steady_clock::now();
                xRenderOnce(Cam, Slot, Out, OutW, OutH);
                const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count(); SumMs += Ms;
                { const xXXH128::tHash Hs = xPicHash::CalcPicHash(m_PicInP[0]); HashSynth.update(Hs.data(), Hs.size()); }
                if(i == 0 && !m_BenchShot.empty()) { uiWritePng(m_BenchShot.c_str(), Out.data(), OutW, OutH); }
                if(m_Sbs && !m_MetricVals.empty()) { xEvalMetrics(); SumMetricMs += m_MetricMs; NumMetric++; }
                if(m_VerboseLevel >= 2) { fmt::print("{}\n", formatBenchLine(i, Ms)); }
            }
            const double Total = std::chrono::duration<double>(std::chrono::steady_clock::now() - TAll).count();
            if(Slot) { m_Cache.release(Slot); }
            fmt::print("\nBENCH  frames {}  views {}  mode {}  clouds {}  picture {}x{}\n", m_BenchFrames, NumViews, BenchMode, m_Sbs ? 2 : 1, m_PictureSize[0], m_PictureSize[1]);
            fmt::print("BENCH  render {:.2f} ms per frame ({:.1f} fps){}\n", SumMs / m_BenchFrames, 1000.0 * m_BenchFrames / SumMs, m_Sbs ? " (both clouds)" : "");
            if(NumMetric > 0) { fmt::print("BENCH  metrics {:.2f} ms per frame\n", SumMetricMs / NumMetric); }
            fmt::print("BENCH  wall {:.3f} s\n", Total);
            fmt::print("HashSynth0 {}\n", xXXH128::format(HashSynth.finish()));
            for(const xMetricVal& V : m_MetricVals)
            {
                if(V.Count == 0) { continue; }
                fmt::print("BENCH  {} ({}) last {:.5f}  mean {:.5f}\n", xMetricToStr((eMetric)V.Metric), V.Domain == (int32)eClrSpcMtr::YCbCr ? "YCbCr" : "RGB", V.Pic, V.Sum / (double)V.Count);
            }
            return 0;
        }

        //the window
        m_RenderThread = std::thread([this]() { xRenderLoop(); });
        xGspHost Host(*this);
        gsp::UiOptions O;
        O.Title = "GS-Player"; O.WinW = m_PictureSize[0] * (m_Sbs ? 2 : 1); O.WinH = m_PictureSize[1]; O.Scale = (float)m_UiScale;
        O.TestFrames = m_UiTestFrames; O.TestMs = m_UiTestMs; O.TestShot = m_UiTestShot.c_str();
        const int32 Rc = gsp::runUi(Host, O);
        return Rc;
    }

    //===============================================================================================================================================================================================================
    
    } //end of namespace PMBB_NAMESPACE