/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: If you use this software, please cite the following paper: A. Dziembowski, D. Mieloch, J. Stankowski and A. Grzelka, "IV-PSNR—The Objective Quality Metric for Immersive Video Applications," in IEEE Transactions on Circuits and Systems for Video Technology, vol. 32, no. 11, pp. 7575-7591, Nov. 2022, doi: 10.1109/TCSVT.2022.3179575.
    SPDX-License-Identifier: BSD-3-Clause
*/

//===============================================================================================================================================================================================================

#include "xAppQMGC.h"
#include "xFile.h"
#include "xErrMsg.h"
#include "xFmtScn.h"
#include "xMemory.h"
#include "xMiscUtilsBASE.h"
#include "xMiscUtilsCORE.h"
#include <fstream>
#include <cassert>
#include <filesystem>
#include "fmt/chrono.h"

using namespace PMBB_NAMESPACE;

//===============================================================================================================================================================================================================
// Main
//===============================================================================================================================================================================================================
#ifndef APP_MAIN
#define APP_MAIN main
#endif

int32 APP_MAIN(int argc, char *argv[], char* /*envp*/[])
{
  fmt::print("{}\n", xAppQMGC::c_BannerString);
  tTimePoint AppBeg = tClock::now();
  xAppQMGC AppQMGC;

  //===================================================================================================================
  //configuring
  //===================================================================================================================

  //parsing configuration
  AppQMGC.registerCmdParams();
  bool CfgLoadResult = AppQMGC.loadConfiguration(argc, const_cast<const char**>(argv));
  if(!CfgLoadResult) { xErrMsg::printError(AppQMGC.getErrorLog() + "\n\n", AppQMGC.formatHelp()); return EXIT_FAILURE; }
  bool CfgReadResult = AppQMGC.readConfiguration();
  if(!CfgReadResult) { xErrMsg::printError(AppQMGC.getErrorLog() + "\n\n", AppQMGC.formatHelp()); return EXIT_FAILURE; }
  const int32 VerboseLevel = AppQMGC.getVerboseLevel();

  if(VerboseLevel >= 2)
  { 
    fmt::print("WorkingDir = {}\n\n", std::filesystem::current_path().string());    
    fmt::print("Commandline args:\n"); xCfgINI::printCommandlineArgs(argc, const_cast<const char**>(argv));
  }

  //print compile time setup
  if (VerboseLevel >= 1)
  {
    fmt::print("{}", xMiscUtilsBASE::formatCompileTimeSetup());
    fmt::print("{}", xMiscUtilsCORE::formatCompileTimeSetup());
    fmt::print("\n");
  }

  if(VerboseLevel >= 2)
  {
    fmt::print("{}\n", xMiscUtilsBASE::formatBuildInfo());
  }

  //print config
  if(VerboseLevel >= 1) { fmt::print("{}\n", AppQMGC.formatConfiguration()); }

  //validate file names against input parameters
  eAppRes ValidFilesRes = AppQMGC.validateInputFiles();
  if(ValidFilesRes == eAppRes::Warning) { xErrMsg::printError(std::string("PARAMETERS WARNING: Invalid parameters\n") + AppQMGC.getErrorLog()); }
  if(ValidFilesRes == eAppRes::Error  ) { xErrMsg::printError(std::string("PARAMETERS WARNING: Invalid parameters\n") + AppQMGC.getErrorLog()); return EXIT_FAILURE; }

  //print configuration warnings
  std::string ConfigWarnings = AppQMGC.formatWarnings();
  if(!ConfigWarnings.empty()) { fmt::print("{}", ConfigWarnings); }

  //hardware concurency
  AppQMGC.setupMultithreading();
  if(VerboseLevel >= 1) { fmt::print("{}\n", AppQMGC.formatMultithreading()); }

  //spacer
  fmt::print("\n\n\n");


  //===================================================================================================================
  // preparation
  //===================================================================================================================
  if(VerboseLevel >= 2) { fmt::print("Initializing:\n"); }

  eAppRes SeqRes = AppQMGC.setupSeqs (); if(SeqRes == eAppRes::Error) { return EXIT_FAILURE; }
  eAppRes BufRes = AppQMGC.setupBuffs(); if(BufRes == eAppRes::Error) { return EXIT_FAILURE; }

  AppQMGC.createMetricProcessors ();
  AppQMGC.initMetricStorage      ();
  AppQMGC.createRendererProcessor();


  //===================================================================================================================
  //running
  //===================================================================================================================
  tTimePoint PrcBeg = tClock::now();
  eAppRes ClcRes = AppQMGC.processAllFrames();
  if(ClcRes == eAppRes::Error) { return EXIT_FAILURE; }
  tTimePoint PrcEnd = tClock::now();

  //===================================================================================================================
  //finalizing
  //===================================================================================================================
  if(VerboseLevel >= 1) { fmt::print("\n"); fmt::print("{}", AppQMGC.calibrateTimeStamp()); }
  fmt::print("\n\n");

  AppQMGC.combineFrameStats();

  //output file
  if(!AppQMGC.getResultFile().empty())
  {
    std::ofstream ResultStream(AppQMGC.getResultFile(), std::ios::app);
    ResultStream << AppQMGC.formatResultsFile();
    ResultStream.close();
  }

  //printout results
  fmt::print("{}", AppQMGC.formatResultsStdOut());
  fmt::print("\n");

  AppQMGC.ceaseSeqs               ();
  AppQMGC.ceaseBuffs              ();
  AppQMGC.ceaseMultithreading     ();
  AppQMGC.destroyMetricProcessors ();
  AppQMGC.destroyRendererProcessor();

  tTimePoint AppEnd = tClock::now();
  fmt::print("TotalProcessingTime  = {:.3f} s\n", std::chrono::duration_cast<tDurationS>(PrcEnd - PrcBeg).count());
  fmt::print("TotalApplicationTime = {:.3f} s\n", std::chrono::duration_cast<tDurationS>(AppEnd - AppBeg).count());
  fmt::print("END-OF-LOG\n");
  fflush(stdout);

  return EXIT_SUCCESS;
}

//===============================================================================================================================================================================================================
