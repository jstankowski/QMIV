/*
    SPDX-FileCopyrightText: 2019-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#include "xUtilsQMIV.h"
#include "xString.h"

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================

eClrSpcApp xStr2ClrSpcApp(const std::string& ClrSpc)
{
  std::string ClrSpcU = xString::toUpper(ClrSpc);
  return ClrSpc=="RGB"             ? eClrSpcApp::RGB             :
         ClrSpc=="BGR"             ? eClrSpcApp::BGR             :
         ClrSpc=="GBR"             ? eClrSpcApp::GBR             :
         ClrSpc=="YCbCr"           ? eClrSpcApp::YCbCr           :
         ClrSpc=="YCbCr_BT601"     ? eClrSpcApp::YCbCr_BT601     :
         ClrSpc=="YCbCr_SMPTE170M" ? eClrSpcApp::YCbCr_SMPTE170M :
         ClrSpc=="YCbCr_BT709"     ? eClrSpcApp::YCbCr_BT709     :
         ClrSpc=="YCbCr_SMPTE240M" ? eClrSpcApp::YCbCr_SMPTE240M :
         ClrSpc=="YCbCr_BT2020"    ? eClrSpcApp::YCbCr_BT2020    :
                                     eClrSpcApp::INVALID;
}
std::string xClrSpcApp2Str(eClrSpcApp ClrSpc)
{
  switch(ClrSpc)
  {
   case eClrSpcApp::RGB             : return "RGB"             ;
   case eClrSpcApp::BGR             : return "BGR"             ;
   case eClrSpcApp::GBR             : return "GBR"             ;
   case eClrSpcApp::YCbCr           : return "YCbCr"           ;
   case eClrSpcApp::YCbCr_BT601     : return "YCbCr_BT601"     ;
   case eClrSpcApp::YCbCr_SMPTE170M : return "YCbCr_SMPTE170M" ;
   case eClrSpcApp::YCbCr_BT709     : return "YCbCr_BT709"     ;
   case eClrSpcApp::YCbCr_SMPTE240M : return "YCbCr_SMPTE240M" ;
   case eClrSpcApp::YCbCr_BT2020    : return "YCbCr_BT2020"    ;
   default                          : return "INVALID"         ;
  }
}
eClrSpcLC xClrSpcAppToClrSpc(eClrSpcApp ClrSpcApp)
{
  switch(ClrSpcApp)
  {
  case eClrSpcApp::YCbCr_BT601    : return eClrSpcLC::BT601    ;
  case eClrSpcApp::YCbCr_SMPTE170M: return eClrSpcLC::SMPTE170M;
  case eClrSpcApp::YCbCr_BT709    : return eClrSpcLC::BT709    ;
  case eClrSpcApp::YCbCr_SMPTE240M: return eClrSpcLC::SMPTE240M;
  case eClrSpcApp::YCbCr_BT2020   : return eClrSpcLC::BT2020   ;
  default                         : return eClrSpcLC::INVALID  ; 
  }
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB