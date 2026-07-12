/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#include "xGspCache.h"
#include "xFile.h"
#include <fmt/format.h>
#include <algorithm>
#include <chrono>

namespace PMBB_NAMESPACE {

    //===============================================================================================================================================================================================================

    //the index of the first frame of a sequence (a pattern with {FrameIdx}): the first existing file
    static int32 xDetectSeqBase(const std::string& Pattern)
    {
        constexpr int32 c_MaxProbe = 100000;
        for(int32 i = 0; i < c_MaxProbe; i++)
        {
            if(xFile::exists(xFileListUtils::formatNamedPattern(Pattern, i, fmt::arg("FrameIdx", i)))) { return i; }
        }
        return NOT_VALID;
    }

    bool xGspCache::open(int32 NumClouds, const std::string Pattern[2], const int32 StartFrame[2], int32 NumFrames, std::string& Error)
    {
        m_NumClouds = NumClouds; m_NumFrames = 0;
        int32 Min = std::numeric_limits<int32>::max();
        for(int32 c = 0; c < m_NumClouds; c++)
        {
            m_Pattern[c] = Pattern[c];
            int32 Start = StartFrame[c];
            if(Start < 0) { const int32 Base = xDetectSeqBase(Pattern[c]); Start = Base != NOT_VALID ? Base : 0; }
            m_Seq[c].create(false, Start, xFileListUtils::c_DefaultMaxNumFiles);
            FVGS::xSeqPlyList::tResult R = m_Seq[c].openFile(Pattern[c], xSeq::eMode::Read);
            if(!R) { Error = fmt::format("InputFile opening failure ({}) {}", Pattern[c], R.format()); return false; }
            const int32 N = m_Seq[c].getNumOfFrames();
            if(N <= 0) { Error = fmt::format("no frames found from the start frame for ({})", Pattern[c]); return false; }
            Min = std::min(Min, N);
        }
        m_NumFrames = NumFrames > 0 ? std::min(NumFrames, Min) : Min;
        return true;
    }
    int64 xGspCache::estimateFrameBytes()
    {
        int64 Bytes = 0;
        for(int32 c = 0; c < m_NumClouds; c++)
        {
            std::error_code Ec;
            const int64 FileBytes = (int64)std::filesystem::file_size(m_Seq[c].get1stFileName(), Ec);
            if(!Ec) { Bytes += FileBytes / 248 * (int64)sizeof(FVGS::xGaussianObject); } //248 B = one splat with SH degree 3 in the PLY
        }
        return Bytes;
    }
    void xGspCache::start(int32 NumSlots, xThreadPool* Pool, int32 MaxNumTasks)
    {
        m_Pool = Pool;
        if(m_Pool) { m_TPI.init(m_Pool, MaxNumTasks, MaxNumTasks); }
        m_Slots.clear(); m_Slots.resize((size_t)std::max(1, std::min(NumSlots, m_NumFrames)));
        m_Quit = false;
        m_Thread = std::thread([this]() { xLoaderLoop(); });
    }
    void xGspCache::stop()
    {
        { std::lock_guard<std::mutex> Lk(m_Mx); m_Quit = true; }
        m_Cv.notify_all();
        if(m_Thread.joinable()) { m_Thread.join(); }
        if(m_Pool) { m_TPI.uninit(); m_Pool = nullptr; }
        for(int32 c = 0; c < m_NumClouds; c++) { m_Seq[c].closeFile(); m_Seq[c].destroy(); }
        m_Slots.clear();
    }

    int32 xGspCache::xFindSlot(int32 Frame) const
    {
        for(int32 i = 0; i < (int32)m_Slots.size(); i++) { if(m_Slots[(size_t)i].Frame == Frame && m_Slots[(size_t)i].State != Free) { return i; } }
        return NOT_VALID;
    }
    //the frame of every cloud read into the slot (the splats in parallel ranges, as xLoadCloud of AppGSR)
    bool xGspCache::xLoadInto(xSlot& Slot, int32 Frame)
    {
        for(int32 c = 0; c < m_NumClouds; c++)
        {
            FVGS::xSeqPlyList& Seq = m_Seq[c];
            FVGS::xSeqPlyList::tResult R = Seq.seekFrame(Frame);
            if(R) { R = Seq.readGC(&Slot.Cloud[c]); }
            if(!R) { std::lock_guard<std::mutex> Lk(m_Mx); m_Error = fmt::format("frame {} of {}: {}", Frame, m_Pattern[c], R.format()); return false; }
        }
        return true;
    }
    void xGspCache::xLoaderLoop()
    {
        std::unique_lock<std::mutex> Lk(m_Mx);
        while(!m_Quit)
        {
            //the first wanted frame that is not in a slot
            int32 Target = NOT_VALID;
            for(int32 f : m_Wanted) { if(xFindSlot(f) == NOT_VALID) { Target = f; break; } }
            int32 Dst = NOT_VALID;
            if(Target != NOT_VALID)
            {
                for(int32 i = 0; i < (int32)m_Slots.size() && Dst == NOT_VALID; i++) { if(m_Slots[(size_t)i].State == Free) { Dst = i; } }
                for(int32 i = 0; i < (int32)m_Slots.size() && Dst == NOT_VALID; i++) //evict a frame that is not wanted any more (not pinned)
                {
                    const xSlot& S = m_Slots[(size_t)i];
                    if(S.Pins == 0 && (S.State == Ready || S.State == Failed) && std::find(m_Wanted.begin(), m_Wanted.end(), S.Frame) == m_Wanted.end()) { Dst = i; }
                }
            }
            if(Target == NOT_VALID || Dst == NOT_VALID) { m_Cv.wait_for(Lk, std::chrono::milliseconds(100)); continue; }

            xSlot& S = m_Slots[(size_t)Dst];
            S.Frame = Target; S.State = Loading;
            Lk.unlock();
            const bool Ok = xLoadInto(S, Target);
            Lk.lock();
            S.State = Ok ? Ready : Failed;
            m_CvReady.notify_all();
        }
    }

    xGspCache::xSlot* xGspCache::acquire(int32 Frame, int32 TimeoutMs)
    {
        std::unique_lock<std::mutex> Lk(m_Mx);
        auto Try = [&]() -> xSlot*
            {
                const int32 i = xFindSlot(Frame);
                if(i == NOT_VALID || m_Slots[(size_t)i].State != Ready) { return nullptr; }
                m_Slots[(size_t)i].Pins++;
                return &m_Slots[(size_t)i];
            };
        xSlot* S = Try();
        if(S == nullptr && TimeoutMs > 0)
        {
            const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(TimeoutMs);
            while(S == nullptr && std::chrono::steady_clock::now() < Deadline) { m_CvReady.wait_until(Lk, Deadline); S = Try(); }
        }
        return S;
    }
    void xGspCache::release(xSlot* Slot)
    {
        if(Slot == nullptr) { return; }
        { std::lock_guard<std::mutex> Lk(m_Mx); Slot->Pins--; }
        m_Cv.notify_all();
    }
    void xGspCache::setWanted(const std::vector<int32>& Frames)
    {
        { std::lock_guard<std::mutex> Lk(m_Mx); m_Wanted = Frames; if((int32)m_Wanted.size() > (int32)m_Slots.size()) { m_Wanted.resize(m_Slots.size()); } }
        m_Cv.notify_all();
    }
    int32 xGspCache::getNumReady()
    {
        std::lock_guard<std::mutex> Lk(m_Mx);
        int32 N = 0; for(const xSlot& S : m_Slots) { if(S.State == Ready) { N++; } }
        return N;
    }
    std::string xGspCache::getError()
    {
        std::lock_guard<std::mutex> Lk(m_Mx);
        return m_Error;
    }

    //===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE