/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#include "xCommonDefCORE.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================
// Time is money
//===============================================================================================================================================================================================================
using tClock      = std::chrono::high_resolution_clock       ;
using tTimePoint  = tClock::time_point                       ;
using tDuration   = tClock::duration                         ;
using tDurationUS = std::chrono::duration<double, std::micro>;
using tDurationMS = std::chrono::duration<double, std::milli>;
using tDurationS  = std::chrono::duration<double            >;

// Time Stamp Counter
#if (X_ARCHITECTURE_AMD64)
  #define NoBarierRDTSC 0
  #if NoBarierRDTSC
    static inline uint64 xTSC() { return __rdtsc(); }
    #define X_TSC_IMPLEMENTATION "RDTSC"
  #else
    static inline uint64 xTSC() { uint32 T;  return __rdtscp(&T); }
    #define X_TSC_IMPLEMENTATION "RDTSCP"
  #endif
#else
  static inline uint64 xTSC() { return (uint64)(tClock::now().time_since_epoch().count()); }
  #define X_TSC_IMPLEMENTATION "std::chrono::high_resolution_clock"
#endif


//===============================================================================================================================================================================================================

class xTimeStamp
{
protected:
  tTimePoint m_ProcBegTime  = tTimePoint::min();
  tTimePoint m_ProcEndTime  = tTimePoint::min();
  uint64     m_ProcBegTicks = 0;
  uint64     m_ProcEndTicks = 0;

  flt64 m_TicksPerMicroSec = 0.0;
  flt64 m_TicksPerMiliSec  = 0.0;
  flt64 m_TicksPerSec      = 0.0;

public:
  void sampleBeg() { m_ProcBegTime = tClock::now(); m_ProcBegTicks = xTSC(); }
  void sampleEnd() { m_ProcEndTime = tClock::now(); m_ProcEndTicks = xTSC(); }

  void        calibrateTimeStamp();
  std::string formatCalibration () const;

  flt64 getTicksPerMicroSec() const { return m_TicksPerMicroSec; }
  flt64 getTicksPerMiliSec () const { return m_TicksPerMiliSec ; }
  flt64 getTicksPerSec     () const { return m_TicksPerSec     ; }
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB