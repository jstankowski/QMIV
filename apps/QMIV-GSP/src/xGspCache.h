/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xGaussianCloud.h"
#include "xPly.h"
#include "xThreadPool.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <string>

namespace PMBB_NAMESPACE {

    //===============================================================================================================================================================================================================

    //the clouds of the frames read ahead of the player: a few slots (every slot holds the frame of every sequence: one cloud, or two for the side by side), a loader thread that fills them with the
    //frames that will be needed (setWanted, the most urgent first), non-blocking access for the renderer (acquire / release). The splats of a frame are read in parallel in ranges (as QMIV-GS does).
    class xGspCache
    {
    public:
        static constexpr int32 c_LoadRangeSplats = 32 * 1024;
        enum eState { Free = 0, Loading = 1, Ready = 2, Failed = 3 };
        struct xSlot
        {
            int32  Frame = -1;
            int32  State = Free;
            int32  Pins  = 0;
            FVGS::xGaussianCloudI Cloud[2];
        };

    protected:
        int32             m_NumClouds = 1;
        std::string       m_Pattern[2];
        FVGS::xSeqPlyList  m_Seq[2];
        int32             m_NumFrames = 0;

        std::vector<xSlot> m_Slots;
        std::vector<int32> m_Wanted;
        std::string        m_Error;

        xThreadPool* m_Pool = nullptr;
        tThPI        m_TPI;               //own client of the pool (the renderer and the metrics have theirs)

        std::mutex              m_Mx;
        std::condition_variable m_Cv;      //the loader waits here
        std::condition_variable m_CvReady; //a frame became ready
        std::thread             m_Thread;
        bool                    m_Quit = false;

        bool xLoadInto(xSlot& Slot, int32 Frame);
        void xLoaderLoop();
        int32 xFindSlot(int32 Frame) const;

    public:
        //opens the sequences: Pattern[c] with {FrameIdx}, StartFrame[c]; NumFrames <= 0: all the frames of the shorter sequence
        bool  open(int32 NumClouds, const std::string Pattern[2], const int32 StartFrame[2], int32 NumFrames, std::string& Error);
        int32 getNumFrames() const { return m_NumFrames; }
        int64 estimateFrameBytes(); //of all the clouds of the first frame, from the headers
        //slots and the loader thread
        void  start(int32 NumSlots, xThreadPool* Pool, int32 MaxNumTasks);
        void  stop ();

        //the slot of the frame if it is ready (it is pinned until release), nullptr otherwise; blocks up to TimeoutMs for it
        xSlot* acquire(int32 Frame, int32 TimeoutMs = 0);
        void   release(xSlot* Slot);
        void   setWanted(const std::vector<int32>& Frames); //the first frame is the one rendered now
        int32  getNumReady();
        int32  getNumSlots() const { return (int32)m_Slots.size(); }
        std::string getError();
    };

    //===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE