/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/

//===============================================================================================================================================================================================================

#include "xAppGSPlayer.h"
#include "xErrMsg.h"
#include "xMiscUtilsBASE.h"
#include "xMiscUtilsCORE.h"
#include <filesystem>
#include <cstdio>
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
    std::setvbuf(stdout, nullptr, _IONBF, 0); //the lines reach the console (or the pipe) at once, also before a crash
    fmt::print("{}\n", xAppGSPlayer::c_BannerString);
    xAppGSPlayer App;

    //configuring
    App.registerCmdParams();
    bool CfgLoadResult = App.loadConfiguration(argc, const_cast<const char**>(argv));
    if(!CfgLoadResult) { xErrMsg::printError(App.getErrorLog() + "\n\n", App.formatHelp()); return EXIT_FAILURE; }
    bool CfgReadResult = App.readConfiguration();
    if(!CfgReadResult) { xErrMsg::printError(App.getErrorLog() + "\n\n", App.formatHelp()); return EXIT_FAILURE; }
    const int32 VerboseLevel = App.getVerboseLevel();

    if(VerboseLevel >= 1) { fmt::print("{}", xMiscUtilsBASE::formatCompileTimeSetup()); fmt::print("{}", xMiscUtilsCORE::formatCompileTimeSetup()); fmt::print("\n"); }
    if(VerboseLevel >= 1) { fmt::print("{}\n", App.formatConfiguration()); }

    eAppRes ValidFilesRes = App.validateInputFiles();
    if(ValidFilesRes == eAppRes::Error) { xErrMsg::printError(std::string("PARAMETERS ERROR: Invalid parameters\n") + App.getErrorLog()); return EXIT_FAILURE; }
    const std::string Warn = App.formatWarningsCmn();
    if(VerboseLevel >= 2 && !Warn.empty()) { fmt::print("{}", Warn); }

    //preparation and running
    eAppRes SetupRes = App.setupAll();
    if(SetupRes == eAppRes::Error) { App.ceaseAll(); return EXIT_FAILURE; }
    if(VerboseLevel >= 1) { fmt::print("{}\n", App.formatMultithreading()); }
    const int32 Rc = App.run();

    //finalizing
    App.ceaseAll();
    fmt::print("END-OF-LOG\n");
    fflush(stdout);
    return Rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

//===============================================================================================================================================================================================================