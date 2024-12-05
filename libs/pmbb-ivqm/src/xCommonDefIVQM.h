/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski   <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2018-2019 Adrian Dziembowski <adrian.dziembowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

//===============================================================================================================================================================================================================
// PMBB-core
//===============================================================================================================================================================================================================
#include "xCommonDefCORE.h"
#include "xThreadPool.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================
// Threading
//===============================================================================================================================================================================================================
using tThPI = xThreadPoolInterfaceFunction;

class xMultiThreaded
{
protected:
  tThPI* m_ThPI     = nullptr; //thread pool interface
  bool   m_OwnsThPI = false  ;

public:
  void  createThrdPoolIntf (xThreadPool* ThreadPool, int32 Height)
  { 
    m_ThPI = new tThPI;
    m_OwnsThPI = true;
    if(ThreadPool) { m_ThPI->init(ThreadPool, Height, Height); }
  }
  void  destroyThrdPoolIntf()
  {
    if(!m_OwnsThPI) { return; }
    m_ThPI->uininit();
    delete m_ThPI; m_ThPI = nullptr;
  }
  bool bindThrdPoolIntf(tThPI* ThPI)
  {
    if(m_ThPI != nullptr) { return false; }
    m_ThPI     = ThPI ;
    m_OwnsThPI = false;
    return true;
  }
  tThPI* unbindThrdPoolIntf()
  {
    if(m_OwnsThPI || m_ThPI == nullptr) { return nullptr; }
    tThPI* Tmp = m_ThPI; 
    m_ThPI = nullptr;
    return Tmp;
  }
};

//===============================================================================================================================================================================================================
// Compile time settings
//===============================================================================================================================================================================================================
static constexpr bool xc_USE_RUNTIME_CMPWEIGHTS  = true ; // use component weights provided at runtime
static constexpr bool xc_CLIP_CURR_TST_RANGE     = false; // introduces consistency beetwen both IVPSNR methods, breaks compatibility

//===============================================================================================================================================================================================================

} //end of namespace PMBB
