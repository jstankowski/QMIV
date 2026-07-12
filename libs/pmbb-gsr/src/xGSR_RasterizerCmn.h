/*
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xProjectedCloud.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

class xRasterizerCmn
{
public:
  using tTileCache = std::array<std::array<std::array<flt32, c_TileSize>, c_TileSize>, 4>; //[RGBA][y][x]
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE::GSR
