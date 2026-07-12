/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
//GS-Player user interface: the window, the keyboard and mouse, the picture and the overlay. Plain types only (no PMBB types, no std containers): the application is compiled once per
//microarchitecture level, this library once, and they meet only here.
#include <cstdint>

namespace gsp {

    //===============================================================================================================================================================================================================

    enum eMode { ModeFree = 0, ModeViews = 1 };

    enum eCmd
    {
        CmdNone = 0,
        CmdTogglePlay,    // Space
        CmdStepFwd,       // .
        CmdStepBack,      // ,
        CmdSeek,          // slider: Arg = frame
        CmdNextView,      // Right
        CmdPrevView,      // Left
        CmdMode,          // F1 / F2: Arg = eMode
        CmdResetCamera,   // Home
        CmdToggleRecord,  // R
        CmdScreenshot,    // F12
        CmdCycleMetric,   // M: the metric of the plot
        CmdPlaySpeed,     // + / -: Arg = +1 / -1 (playback frame rate)
        CmdLoop,          // O
        CmdQuit           // Esc
    };

    struct Frame
    {
        const uint8_t* Rgb = nullptr; // R, G, B per pixel, W * 3 bytes per row
        int      W = 0, H = 0;
        uint64_t Id = 0;              // changes with every new picture
    };

    struct Input
    {
        float Move[3] = { 0, 0, 0 };  // forward, right, up along the axes of the camera: -1..1 (E Q, D A, W S)
        float Look[2] = { 0, 0 };     // mouse movement in pixels since the previous Input (while the right button is held)
        float Roll    = 0;            // -1..1 (Z X)
        float Speed   = 1;            // multiplier: the wheel, Shift (faster), Ctrl (slower)
        float Dt      = 0;            // seconds since the previous Input
    };

    struct MetricRow { char Name[24]; char Value[40]; char Avg[40]; char Comp[72]; };

    enum { MaxRows = 12, MaxPlot = 240 };

    struct Hud
    {
        char  Line1[240];             // status line
        char  Line2[240];             // camera
        char  Message[160];           // transient message (empty: none)
        int   NumFrames = 0, CurFrame = 0;
        int   NumViews  = 0, CurView  = 0;
        int   Mode = ModeFree;
        bool  Playing = false, Buffering = false, Sbs = false, Recording = false, Loop = true;
        float RenderMs = 0, RenderFps = 0, PlayFps = 0;
        int   NumRows = 0;  MetricRow Rows[MaxRows];
        int   PlotN = 0;    float Plot[MaxPlot]; char PlotName[40]; float PlotMin = 0, PlotMax = 0;
    };

    struct UiOptions
    {
        const char* Title      = "GS-Player";
        int         WinW       = 960, WinH = 540;
        float       Scale      = 1.0f;     // the size of the overlay
        bool        VSync      = true;
        int         TestFrames = 0;        // > 0: runs this many frames (no waiting for the user), writes the picture of the window with the overlay to TestShot and returns
        int         TestMs     = 0;        // > 0: the same after this many milliseconds (whichever comes first)
        const char* TestShot   = nullptr;
        const char* TestKeys   = nullptr;   // the keys of the window at given times: "ms:K;ms:d:+;ms:c:V;ms:split:0.3:0.5" (d / c: the key in the difference / split window)
    };

    class Host
    {
    public:
        virtual ~Host() {}
        virtual bool pollFrame   (Frame& F)                 = 0; // the newest picture, if there is one newer than the previous poll; valid until releaseFrame
        virtual void releaseFrame()                         = 0;
        virtual void fillHud     (Hud& H)                   = 0;
        virtual void onInput     (const Input& I)           = 0;
        virtual void onCommand   (int Cmd, int Arg)         = 0;
        virtual void onResize    (int W, int H)             = 0; // client size of the window in pixels
        virtual bool writePng    (const char* Path, const uint8_t* Rgb, int W, int H) = 0;
    };

    //runs the window until it is closed (or the test frames are done); returns 0 when ok
    int runUi(Host& H, const UiOptions& O);

    //===============================================================================================================================================================================================================

} //end of namespace gsp