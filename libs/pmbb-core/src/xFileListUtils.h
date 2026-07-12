/*
    SPDX-FileCopyrightText: 2019-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include "xCommonDefCORE.h"
#include "xInOutResult.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

class xFileListUtils
{
public:
  static constexpr int32 c_DefaultMaxNumFiles = std::numeric_limits<int32>::max() - 1;

  enum class eFmtSts
  {
    UNKNOWN  ,
    HasFmt   ,
    NonFmt   ,
    Malformed,
  };

  using  tIntRes = std::pair<int32, xInOutResult>;

  static bool checkIfFileExists(const std::string& FileNamePattern);

  static eFmtSts analyzeFormatString(std::string_view s);
  static tIntRes detectFirstFrame(const std::string& FileNamePattern, std::function<xInOutResult(const std::string&)>FileVerify);
  static tIntRes detectNumFrames(const std::string& FileNamePattern, int32 FirstFileIdx, int32 MaxNumFiles);

  static inline std::string formatFileName(const std::string& FileNamePattern, int32 FileIdx) { return fmt::format(fmt::runtime(FileNamePattern), FileIdx); }
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB
