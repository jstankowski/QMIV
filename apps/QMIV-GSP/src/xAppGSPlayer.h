/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include "xFile.h"
#include "xCfgINI.h"
#include "xFmtScn.h"
#include "xMemory.h"
#include "xMiscUtilsCORE.h"
#include "xTimeUtils.h"
#include "xThreadPool.h"
#include "xPly.h"
#include "xPic.h"
#include "xUtilsAppQM.h"
#include "xMetricsAppQM.h"
#include "xAppQM_Common.h"
#include "xCommonDefGSR.h"
#include "xGSR_Renderer.h"
#include "xCameraParamsManager.h"
#include "xXXH.h"
#include "xGspCamera.h"
#include "xGspCache.h"
#include "GspUi.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PMBB_NAMESPACE {

    //===============================================================================================================================================================================================================

    //GS-Player: the real-time viewer of the Gaussian-splat renderer. One cloud, or two clouds side by side with the quality metrics of the pair calculated on the renders (the same code as QMIV-GS).
    //Threads: the window (the main thread, gsp-ui), the render loop (here), the loader of the frames (xGspCache) and the workers of the pool shared by the renderer, the loader and the metrics.
    class xAppGSPlayer : public xAppQM
    {
    public:
        static const std::string_view c_BannerString;
        static const std::string_view c_HelpStringHead;
        static const std::string_view c_HelpStringGeneral;
        static const std::string_view c_HelpStringKeys;
        tStr formatHelp();

    public:
        //configuration
        tStr             m_CamParamsFile;
        FVGS::eCamParamsFormat m_CamParamsFormat = FVGS::eCamParamsFormat::PLY;
        tStr             m_CamNameTemplate = "[CamIdStr]";
        int32            m_StartFrameCfg[2] = { -1, -1 };  //-1: the first frame found
        bool             m_Sbs         = false;            //two clouds side by side (with the metrics)
        flt64            m_PlayFps     = 30.0;
        int32            m_NumAhead    = 6;                //frames read ahead
        flt64            m_MemFraction = 0.5;              //of the physical memory for the frames read ahead
        bool             m_Loop        = true;
        int32            m_MaxFps      = 60;               //render rate limit
        bool             m_DomainYCbCr = true, m_DomainRGB = false;
        int32            m_SynthAlphaCap = 1;
        flt64            m_UiScale     = 1.0;
        int32            m_UiTestFrames = 0, m_UiTestMs = 0;
        tStr             m_UiTestShot;
        int32            m_BenchFrames = 0;                //> 0: no window, renders this many frames of the input views and prints the statistics
        tStr             m_BenchShot;
        tStr             m_RecordPath  = "gsplayer_path.csv";
        tStr             m_ScriptCfg;                      //test: commands at given times (see -script)
        tStr             m_ShotPrefix  = "gsplayer";
        int32            m_StartMode   = gsp::ModeFree;
        bool             m_StartModeGiven = false;

    protected:
        //cameras
        FVGS::xCameraParamsManager<flt64> m_CamManager;
        xGspCamera                  m_Camera;
        //cache of the frames
        xGspCache                   m_Cache;
        int32                       m_NumFramesTotal = 0;
        //renderer
        GSR::xRenderer*             m_Renderer = nullptr;
        int32                       m_MaxSplatsReserved = 0;

        //the render loop and the window share these (m_Mx)
        std::mutex                  m_Mx;
        std::condition_variable     m_Cv;
        std::thread                 m_RenderThread;
        bool                        m_Quit = false;
        bool                        m_Dirty = true;
        struct xCmd { int32 Cmd, Arg; };
        std::deque<xCmd>            m_Cmds;
        int32                       m_WinW = 0, m_WinH = 0;
        std::chrono::steady_clock::time_point m_WinStamp;
        bool                        m_Playing = true;
        double                      m_T = 0.0;              //the position on the timeline: frames (or poses in the trace mode)
        int32                       m_CurFrame = NOT_VALID;
        xGspCache::xSlot*           m_CurSlot = nullptr;
        bool                        m_Buffering = false;
        bool                        m_Recording = false;
        std::vector<std::array<flt64, 6>> m_RecPoses;
        double                      m_RecAcc = 0.0;
        bool                        m_ShotReq = false;
        std::string                 m_Msg;
        std::chrono::steady_clock::time_point m_MsgUntil;
        int32                       m_ShotCount = 0;
        bool                        m_SpeedSet = false;     //the speed of the flight estimated from the first cloud
        //test script: the commands of the keyboard and the mouse at given times of the window
        struct xScriptStep { double Ms; int32 Cmd; int32 Arg; bool IsInput; gsp::Input In; int32 SizeW = 0, SizeH = 0; };
        std::vector<xScriptStep>    m_Script;
        size_t                      m_ScriptPos = 0;
        std::chrono::steady_clock::time_point m_ScriptT0;
        bool                        m_ScriptStarted = false;

        //pictures for the window
        std::vector<uint8_t>        m_Out[2];
        int32                       m_OutFront = 0, m_OutW = 0, m_OutH = 0;
        uint64_t                    m_OutId = 0, m_OutPolled = 0;
        std::unique_lock<std::mutex> m_OutLock;
        std::mutex                  m_OutMx;

        //statistics of the render
        double                      m_RenderMsEma = 0.0, m_RenderFps = 0.0;
        int32                       m_FramesInWindow = 0;
        std::chrono::steady_clock::time_point m_FpsStamp;
        //metrics of the last pair
        struct xMetricVal { int32 Metric; int32 Domain; flt64 Pic; flt64V4 Cmp; flt64 Sum = 0.0; int32 Count = 0; std::vector<float> Hist; };
        std::vector<xMetricVal>     m_MetricVals;
        int32                       m_PlotMetric = 0;
        bool                        m_NewMetrics = false;
        int32V4                     m_CmpWeightsSearchRGB, m_CmpWeightsAverageRGB, m_CmpWeightsSearchYCbCr, m_CmpWeightsAverageYCbCr;
        flt64                       m_MetricMs = 0.0;
        int32                       m_MetricEveryN = 1, m_MetricSkip = 0;

    public:
        //xAppQM
        int32 determinePreferredNumThreads() override { return m_HardwareConcurency; }
        int32 determineMaxNumTasks        () override;

        void    registerCmdParams  ();
        bool    readConfiguration  ();
        tStr    formatConfiguration();
        eAppRes validateInputFiles ();
        eAppRes setupAll           ();
        void    ceaseAll           ();
        int32   run                ();   //the window until it is closed, or the benchmark

        //called by the window (its thread)
        bool uiPollFrame   (gsp::Frame& F);
        void uiReleaseFrame();
        void uiFillHud     (gsp::Hud& H);
        void uiInput       (const gsp::Input& In);
        void uiCommand     (int32 Cmd, int32 Arg);
        void uiResize      (int32 W, int32 H);
        bool uiWritePng    (const char* Path, const uint8_t* Rgb, int32 W, int32 H);

    protected:
        //rendering
        void   xCreateRenderer      ();
        void   xDestroyRenderer     ();
        void   xSetupBuffers        ();
        void   xCeaseBuffers        ();
        void   xRenderLoop          ();
        bool   xRenderOnce          (const GSR::tCamPar& Cam, xGspCache::xSlot* Slot, std::vector<uint8_t>& Out, int32& OutW, int32& OutH);
        void   xApplyWindowSize     ();
        int32  xFrameOfT            (double T) const;
        int32  xTimelineLength      () const;
        void   xHandleCommands      ();
        void   xSetMsg              (const std::string& Msg, int32 Ms = 2500);
        void   xSaveRecordedPath    ();
        void   xPrefetch            ();
        void   xParseScript         (const std::string& S);
        void   xRunScript           ();
        //metrics (the YCbCr / RGB paths of QMIV-GS for the pair in m_PicInP)
        void   xInitMetricStorage   ();
        void   xSetProcessorsCmpWeights(const int32V4& CmpWeightsSearch, const int32V4& CmpWeightsAverage);
        void   xPreprocessRGB       ();
        void   xPreprocessYCbCr     ();
        void   xCalcMetricsDomain   (eClrSpcMtr ClrSpc);
        void   xEvalMetrics         ();
        void   xStoreMetric         (int32 Metric, eClrSpcMtr ClrSpc);
        static int64 xGetFreeMemory (int64* Total);

    public:
        const std::string& getErrorLogRef() const { return m_ErrorLog; }
        std::string        formatBenchLine(int32 i, double Ms);
    };

    //===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE