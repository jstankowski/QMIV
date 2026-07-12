/*
    SPDX-FileCopyrightText: 2019-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#define PMBB_xCameraParamsManager_IMPLEMENTATION
#include "xCameraParamsManager.h"
#include "xFile.h"
#include "xStream.h"
#include "xPly.h"
#include "xCfgINI.h"
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace PMBB_NAMESPACE::FVGS {

//===============================================================================================================================================================================================================

template <class MatrixType> void xCameraParamsManager<MatrixType>::initAllCams()
{
  uint64 MaxNumCams = 0;
  for(int32 CamGroupIdx = 0; CamGroupIdx < c_MaxNumCamGroups; CamGroupIdx++) { MaxNumCams += m_SelCamIdStr[CamGroupIdx].size(); }
  m_AllCamIdStr.reserve(MaxNumCams);

  //combine cam ids into all cameras ids
  m_AllCamIdStr.clear();
  for(int32 CamGroupIdx = 0; CamGroupIdx < c_MaxNumCamGroups; CamGroupIdx++)
  {
    for(const std::string& CamIdStr : m_SelCamIdStr[CamGroupIdx])
    {
      auto it = std::find(m_AllCamIdStr.begin(), m_AllCamIdStr.end(), CamIdStr);
      if(it == m_AllCamIdStr.end() || !(*it == CamIdStr)) { m_AllCamIdStr.emplace_back(CamIdStr); }
    }
  }

  //assign CamIdx - numbered from 0
  m_CamIdxToCamIdStr.clear();
  m_CamIdStrToCamIdx.clear();
  int32 CamIdx = 0;
  for(const std::string& CamIdStr : m_AllCamIdStr) { m_CamIdxToCamIdStr[CamIdx] = CamIdStr; m_CamIdStrToCamIdx[CamIdStr] = CamIdx; CamIdx++; }

  //find cam Idx
  for(int32 CamGroupIdx = 0; CamGroupIdx < c_MaxNumCamGroups; CamGroupIdx++)
  {
    m_SelCamIdx[CamGroupIdx].clear();
    m_SelCamIdx[CamGroupIdx].reserve(m_SelCamIdStr[CamGroupIdx].size());
    for(const std::string& CamIdStr : m_SelCamIdStr[CamGroupIdx]) { m_SelCamIdx[CamGroupIdx].push_back(m_CamIdStrToCamIdx[CamIdStr]); }
  }
}
template <class MatrixType> void xCameraParamsManager<MatrixType>::loadParams(int32 CamGroupIdx, std::string CamParamsTemplate, std::string CamParamsFile, MatrixType Znear, MatrixType Zfar, int32V2 Resolution)
{
  //load cam parameters
  std::ifstream CamParamsStream(CamParamsFile.c_str());
  std::string   RealCamParamString((std::istreambuf_iterator<char>(CamParamsStream)), std::istreambuf_iterator<char>());
  std::string   ZnearZfarString;

  for(int32 CamIdx : m_SelCamIdx[CamGroupIdx])
  {
    const std::string& CamIdStr = m_CamIdxToCamIdStr[CamIdx];
    std::string        CamName = xString::replaceAll(CamParamsTemplate, "[CamId]", CamIdStr);
    m_AllCamParams[CamIdx] = *tCamParams::readParamsFromString(RealCamParamString, ZnearZfarString, CamName, Znear, Zfar);
    m_AllCamParams[CamIdx].setIdx(CamIdx);
    m_AllCamParams[CamIdx].setResolution(Resolution);
    m_SelCamParams[CamGroupIdx][CamIdx] = &m_AllCamParams[CamIdx];
  }
}
template <class MatrixType> bool xCameraParamsManager<MatrixType>::loadParams(int32 CamGroupIdx, const std::string& CamNameTemplate, const std::string& CamParamsFilePath)
{
  if(!xFile::exists(CamParamsFilePath)) { return false; }

  xCfgINI::xParser Parser;
  bool RarsingResult = Parser.loadFromFile(CamParamsFilePath);
  if(!RarsingResult) { return false; }

  for(int32 CamIdx : m_SelCamIdx[CamGroupIdx])
  {
    const std::string& CamIdStr = m_CamIdxToCamIdStr[CamIdx];
    std::string        CamName = xString::replaceAll(CamNameTemplate, "[CamIdStr]", CamIdStr);
    if(!Parser.findSection(CamName)) { return false; }
    const xCfgINI::xSection& CamSection = Parser.getSection(CamName);
    m_AllCamParams[CamIdx] = tCamParams();
    m_AllCamParams[CamIdx].setIdx(CamIdx);
    m_AllCamParams[CamIdx].readFromConf(CamSection);
    m_AllCamParams[CamIdx].calculateDerrived();
    m_SelCamParams[CamGroupIdx][CamIdx] = &m_AllCamParams[CamIdx];
  }
  return true;
}

namespace
{
  constexpr std::array<int32, 11> c_ColmapModelNumParams =
  {
    3,         // SIMPLE_PINHOLE
    4,         // PINHOLE
    4,         // SIMPLE_RADIAL
    5,         // RADIAL
    8,         // OPENCV
    NOT_VALID, // OPENCV_FISHEYE
    12,        // FULL_OPENCV
    NOT_VALID, // FOV
    NOT_VALID, // SIMPLE_RADIAL_FISHEYE
    NOT_VALID, // RADIAL_FISHEYE
    NOT_VALID, // THIN_PRISM_FISHEYE
  };
  static_assert(c_ColmapModelNumParams.size() == (size_t)eColmapCameraModel::THIN_PRISM_FISHEYE + 1);

  constexpr int32 xColmapModelNumParams(eColmapCameraModel Model)
  {
    const int32 Idx = static_cast<int32>(Model);
    if(Idx < 0 || Idx >= (int32)c_ColmapModelNumParams.size()) { return NOT_VALID; } // INVALID or out of range
    return c_ColmapModelNumParams[Idx];
  }

  template <class MatrixType>
  bool buildColmapCameraParams(eColmapCameraModel Model, int32 Width, int32 Height, const std::vector<flt64>& RawParams, xCameraParams<MatrixType>& OutCP)
  {
    if((int32)RawParams.size() != xColmapModelNumParams(Model)) { return false; }

    MatrixType Fx = (MatrixType)0, Fy = (MatrixType)0, Cx = (MatrixType)0, Cy = (MatrixType)0;
    std::vector<MatrixType> DistCoeffs;

    switch(Model)
    {
    case eColmapCameraModel::SIMPLE_PINHOLE:
      Fx = Fy = (MatrixType)RawParams[0]; Cx = (MatrixType)RawParams[1]; Cy = (MatrixType)RawParams[2];
      break;
    case eColmapCameraModel::PINHOLE:
      Fx = (MatrixType)RawParams[0]; Fy = (MatrixType)RawParams[1]; Cx = (MatrixType)RawParams[2]; Cy = (MatrixType)RawParams[3];
      break;
    case eColmapCameraModel::SIMPLE_RADIAL:
      Fx = Fy = (MatrixType)RawParams[0]; Cx = (MatrixType)RawParams[1]; Cy = (MatrixType)RawParams[2];
      DistCoeffs = { (MatrixType)RawParams[3], (MatrixType)0, (MatrixType)0, (MatrixType)0 };
      break;
    case eColmapCameraModel::RADIAL:
      Fx = Fy = (MatrixType)RawParams[0]; Cx = (MatrixType)RawParams[1]; Cy = (MatrixType)RawParams[2];
      DistCoeffs = { (MatrixType)RawParams[3], (MatrixType)RawParams[4], (MatrixType)0, (MatrixType)0 };
      break;
    case eColmapCameraModel::OPENCV:
      Fx = (MatrixType)RawParams[0]; Fy = (MatrixType)RawParams[1]; Cx = (MatrixType)RawParams[2]; Cy = (MatrixType)RawParams[3];
      DistCoeffs = { (MatrixType)RawParams[4], (MatrixType)RawParams[5], (MatrixType)RawParams[6], (MatrixType)RawParams[7] };
      break;
    case eColmapCameraModel::FULL_OPENCV:
      Fx = (MatrixType)RawParams[0]; Fy = (MatrixType)RawParams[1]; Cx = (MatrixType)RawParams[2]; Cy = (MatrixType)RawParams[3];
      DistCoeffs = { (MatrixType)RawParams[4], (MatrixType)RawParams[5], (MatrixType)RawParams[6], (MatrixType)RawParams[7],
                     (MatrixType)RawParams[8], (MatrixType)RawParams[9], (MatrixType)RawParams[10], (MatrixType)RawParams[11] };
      break;
    default:
      return false;
    }

    OutCP.setResolution({ Width, Height });
    OutCP.setProjType(eProjType::Perspective);
    auto& K = OutCP.getIntrinsics();
    K.zero();
    K[0][0] = Fx; K[0][2] = Cx;
    K[1][1] = Fy; K[1][2] = Cy;
    K[2][2] = (MatrixType)1;
    K[3][3] = (MatrixType)1;
    if(!DistCoeffs.empty()) { OutCP.getDistortion().importOpenCV(DistCoeffs); }

    return true;
  }

  template <class MatrixType>
  bool readColmapCamerasFile(const std::string& CamParamsDir, std::map<int32, xCameraParams<MatrixType>>& OutCameras)
  {
    const std::string TxtPath = CamParamsDir + "/cameras.txt";
    const std::string BinPath = CamParamsDir + "/cameras.bin";

    if(xFile::exists(TxtPath))
    {
      std::ifstream File(TxtPath);
      if(!File.is_open()) { return false; }

      std::string Line;
      while(std::getline(File, Line))
      {
        if(Line.empty() || Line[0] == '#') { continue; }

        std::istringstream Iss(Line);
        int32 CameraId; std::string ModelStr; int32 Width, Height;
        if(!(Iss >> CameraId >> ModelStr >> Width >> Height)) { continue; }

        std::vector<flt64> RawParams;
        flt64 Val;
        while(Iss >> Val) { RawParams.push_back(Val); }

        const eColmapCameraModel Model = xStrToColmapCameraModel(ModelStr);

        xCameraParams<MatrixType> CP;
        if(!buildColmapCameraParams(Model, Width, Height, RawParams, CP)) { continue; }
        OutCameras[CameraId] = CP;
      }
      return true;
    }
    else if(xFile::exists(BinPath))
    {
      xStream File(BinPath, xStream::eMode::Read);
      if(!File.isValid()) { return false; }

      uint64 NumCameras = 0;
      if(!File.read(&NumCameras, sizeof(NumCameras))) { return false; }

      for(uint64 i = 0; i < NumCameras; i++)
      {
        int32 CameraId, ModelId; uint64 Width, Height;
        if(!File.read(&CameraId, sizeof(CameraId))) { return false; }
        if(!File.read(&ModelId , sizeof(ModelId ))) { return false; }
        if(!File.read(&Width   , sizeof(Width   ))) { return false; }
        if(!File.read(&Height  , sizeof(Height  ))) { return false; }

        const eColmapCameraModel Model = static_cast<eColmapCameraModel>(ModelId);
        const int32 NumParams = xColmapModelNumParams(Model);
        if(NumParams < 0) { return false; }
        std::vector<flt64> RawParams(NumParams);
        if(!File.read(RawParams.data(), NumParams * sizeof(flt64))) { return false; }

        xCameraParams<MatrixType> CP;
        if(!buildColmapCameraParams(Model, (int32)Width, (int32)Height, RawParams, CP)) { return false; }
        OutCameras[CameraId] = CP;
      }
      return true;
    }
    return false;
  }

  struct xColmapImageRecord { int32 ImageId; flt64 Qw, Qx, Qy, Qz, Tx, Ty, Tz; int32 CameraId; std::string Name; };

  bool readColmapImagesFile(const std::string& CamParamsDir, std::vector<xColmapImageRecord>& OutRecords)
  {
    const std::string TxtPath = CamParamsDir + "/images.txt";
    const std::string BinPath = CamParamsDir + "/images.bin";

    if(xFile::exists(TxtPath))
    {
      std::ifstream File(TxtPath);
      if(!File.is_open()) { return false; }

      std::string Line;
      while(std::getline(File, Line))
      {
        if(Line.empty() || Line[0] == '#') { continue; }

        std::istringstream Iss(Line);
        xColmapImageRecord Rec;
        if(!(Iss >> Rec.ImageId >> Rec.Qw >> Rec.Qx >> Rec.Qy >> Rec.Qz >> Rec.Tx >> Rec.Ty >> Rec.Tz >> Rec.CameraId >> Rec.Name)) { return false; }
        OutRecords.push_back(Rec);

        // pass correspondence points
        std::string PointsLine;
        if(!std::getline(File, PointsLine)) { return false; }
      }
      return true;
    }
    else if(xFile::exists(BinPath))
    {
      xStream File(BinPath, xStream::eMode::Read);
      if(!File.isValid()) { return false; }

      uint64 NumImages = 0;
      if(!File.read(&NumImages, sizeof(NumImages))) { return false; }

      for(uint64 i = 0; i < NumImages; i++)
      {
        xColmapImageRecord Rec;
        if(!File.read(&Rec.ImageId , sizeof(Rec.ImageId ))) { return false; }
        if(!File.read(&Rec.Qw      , sizeof(Rec.Qw      ))) { return false; }
        if(!File.read(&Rec.Qx      , sizeof(Rec.Qx      ))) { return false; }
        if(!File.read(&Rec.Qy      , sizeof(Rec.Qy      ))) { return false; }
        if(!File.read(&Rec.Qz      , sizeof(Rec.Qz      ))) { return false; }
        if(!File.read(&Rec.Tx      , sizeof(Rec.Tx      ))) { return false; }
        if(!File.read(&Rec.Ty      , sizeof(Rec.Ty      ))) { return false; }
        if(!File.read(&Rec.Tz      , sizeof(Rec.Tz      ))) { return false; }
        if(!File.read(&Rec.CameraId, sizeof(Rec.CameraId))) { return false; }

        Rec.Name.clear();
        char Ch;
        while(true)
        {
          if(!File.read(&Ch, sizeof(Ch))) { return false; }
          if(Ch == '\0') { break; }
          Rec.Name.push_back(Ch);
        }

        uint64 NumPoints2D = 0;
        if(!File.read(&NumPoints2D, sizeof(NumPoints2D))) { return false; }
        if(!File.skip(NumPoints2D * 24)) { return false; }

        OutRecords.push_back(std::move(Rec));
      }
      return true;
    }
    return false;
  }
}

template <class MatrixType> bool xCameraParamsManager<MatrixType>::loadParams(int32 CamGroupIdx, const std::string& CamParamsDir)
{
  std::map<int32, tCamParams> Cameras;
  if(!readColmapCamerasFile(CamParamsDir, Cameras)) { return false; }

  std::vector<xColmapImageRecord> ImageRecords;
  if(!readColmapImagesFile(CamParamsDir, ImageRecords)) { return false; }

  m_SelCamIdx[CamGroupIdx].clear();
  m_SelCamIdx[CamGroupIdx].reserve(ImageRecords.size());

  for(int32 CamIdx = 0; CamIdx < (int32)ImageRecords.size(); CamIdx++)
  {
    const xColmapImageRecord& Rec = ImageRecords[CamIdx];
    auto CamIt = Cameras.find(Rec.CameraId);
    if(CamIt == Cameras.end()) { return false; }

    tCamParams CP = CamIt->second;
    CP.setCamName(Rec.Name);
    xQuaternion<MatrixType> Quat;
    Quat[0] = (MatrixType)Rec.Qw; Quat[1] = (MatrixType)Rec.Qx; Quat[2] = (MatrixType)Rec.Qy; Quat[3] = (MatrixType)Rec.Qz;
    xVec3<MatrixType> Trans((MatrixType)Rec.Tx, (MatrixType)Rec.Ty, (MatrixType)Rec.Tz);
    tCamParams::applyColmapPose(&CP, Quat, Trans);
    CP.setIdx(CamIdx);
    CP.calculateDerrived();

    m_AllCamParams[CamIdx] = std::move(CP);
    m_SelCamParams[CamGroupIdx][CamIdx] = &m_AllCamParams[CamIdx];
    m_SelCamIdx[CamGroupIdx].push_back(CamIdx);
  }
  return true;
}
template <class MatrixType> bool xCameraParamsManager<MatrixType>::loadParams(int32 CamGroupIdx, const std::string& PlyFilePath, int32V2 Resolution)
{
  xStream Stream(PlyFilePath, xStream::eMode::Read);
  if(!Stream.isValid()) { return false; }

  xPly Ply;
  xGaussianCloudP TmpCloud;
  auto ReadRes = Ply.readGC(&TmpCloud, &Stream, true);
  if(!ReadRes) { TmpCloud.destroy(); return false; }

  xGaussianCloudCmn::tCameraData CameraPositions = TmpCloud.getCameraData();
  TmpCloud.destroy();
  if(CameraPositions.empty()) { return false; }

  std::vector<tCamParams> Cams;
  Cams.reserve(CameraPositions.size());
  for(const auto& Cam : CameraPositions)
  {
    flt64 qw = Cam.m_OriginWXYZ[0], qx = Cam.m_OriginWXYZ[1], qy = Cam.m_OriginWXYZ[2], qz = Cam.m_OriginWXYZ[3];
    flt64 Norm = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
    if(Norm > 0.0) { qw /= Norm; qx /= Norm; qy /= Norm; qz /= Norm; }

    flt64 R[3][3] = {
      { 1.0 - 2.0*(qy*qy+qz*qz),  2.0*(qx*qy-qw*qz)      ,  2.0*(qx*qz+qw*qy)       },
      {       2.0*(qx*qy+qw*qz),  1.0 - 2.0*(qx*qx+qz*qz),  2.0*(qy*qz-qw*qx)       },
      {       2.0*(qx*qz-qw*qy),  2.0*(qy*qz+qw*qx)      ,  1.0 - 2.0*(qx*qx+qy*qy) }
    };

    if(Cam.m_ImageSizeXY.getX() <= 0 || Cam.m_ImageSizeXY.getY() <= 0) { return false; } //the parameters are rescaled from the image size of the camera

    tCamParams CP;
    CP.setCamName   (Cam.m_Name);
    CP.setResolution(Cam.m_ImageSizeXY);
    CP.setProjType  (eProjType::Perspective);
    CP.setZnear     ((MatrixType)0.1);
    CP.setZfar      ((MatrixType)1000.0);

    const flt64 cx = (Cam.m_PrincipalPointXY[0] >= 0.0) ? Cam.m_PrincipalPointXY[0] : (Cam.m_ImageSizeXY.getX() * 0.5);
    const flt64 cy = (Cam.m_PrincipalPointXY[1] >= 0.0) ? Cam.m_PrincipalPointXY[1] : (Cam.m_ImageSizeXY.getY() * 0.5);

    auto& K = CP.getIntrinsics();
    K.zero();
    K[0][0] = (MatrixType)Cam.m_FocalLengthXY[0]; K[0][2] = (MatrixType)cx;
    K[1][1] = (MatrixType)Cam.m_FocalLengthXY[1]; K[1][2] = (MatrixType)cy;
    K[2][2] = (MatrixType)1.0;
    K[3][3] = (MatrixType)1.0;

    auto& E = CP.getExtrinsics();
    E.zero();
    for(int32 w = 0; w < 3; w++) { E[0][w] = (MatrixType)R[0][w]; }
    for(int32 w = 0; w < 3; w++) { E[1][w] = (MatrixType)R[1][w]; }
    for(int32 w = 0; w < 3; w++) { E[2][w] = (MatrixType)R[2][w]; }
    E[0][3] = (MatrixType)Cam.m_PositionXYZ[0];
    E[1][3] = (MatrixType)Cam.m_PositionXYZ[1];
    E[2][3] = (MatrixType)Cam.m_PositionXYZ[2];
    E[3][3] = (MatrixType)1.0;

    CP.calculateDerrived();
    CP.rescaleToNewResolution(Resolution);
    Cams.push_back(std::move(CP));
  }

  setCamParams(CamGroupIdx, Cams);
  return true;
}
template <class MatrixType> bool xCameraParamsManager<MatrixType>::loadPoseTraceParams(int32 CamGroupIdx, const std::string& PoseTraceFilePath, int32V2 Resolution)
{
  if(m_SelCamIdx[c_RealGroupIdx].empty()) { return false; }
  const tCamParams& RefCam = getCamParams(m_SelCamIdx[c_RealGroupIdx][0]);

  std::ifstream File(PoseTraceFilePath);
  if(!File.is_open()) { return false; }

  auto TryParseValues = [](const std::string& Line, std::vector<flt64>& Values) -> bool
  {
    Values.clear();
    std::stringstream LineStream(Line);
    std::string Token;
    try
    {
      while(std::getline(LineStream, Token, ',')) { Values.push_back(std::stod(Token)); }
    }
    catch(const std::exception&) { return false; }
    return Values.size() == 6;
  };

  auto BuildParams = [&RefCam, Resolution](flt64 X, flt64 Y, flt64 Z, flt64 Yaw, flt64 Pitch, flt64 Roll) -> tCamParams
  {
    const flt64 cy = std::cos(Yaw   * xc_DegToRad<flt64>), sy = std::sin(Yaw   * xc_DegToRad<flt64>);
    const flt64 cp = std::cos(Pitch * xc_DegToRad<flt64>), sp = std::sin(Pitch * xc_DegToRad<flt64>);
    const flt64 cr = std::cos(Roll  * xc_DegToRad<flt64>), sr = std::sin(Roll  * xc_DegToRad<flt64>);

    //R = Rz(Yaw) * Ry(Pitch) * Rx(Roll) -- MIV/TMIV common reference frame convention (Z-up)
    const flt64 R[3][3] = {
      { cy*cp, cy*sp*sr - sy*cr, cy*sp*cr + sy*sr },
      { sy*cp, sy*sp*sr + cy*cr, sy*sp*cr - cy*sr },
      { -sp  , cp*sr           , cp*cr            }
    };

    tCamParams CP;
    CP.setCamName   ("PoseTrace");
    CP.setResolution(Resolution);
    CP.setProjType  (eProjType::Perspective);
    CP.setZnear     (RefCam.getZnear());
    CP.setZfar      (RefCam.getZfar());

    auto& K = CP.getIntrinsics();
    const auto& RefK = RefCam.getIntrinsics();
    for(int32 r = 0; r < 4; r++) { for(int32 c = 0; c < 4; c++) { K[r][c] = RefK[r][c]; } }

    auto& E = CP.getExtrinsics();
    E.zero();
    for(int32 w = 0; w < 3; w++) { E[0][w] = (MatrixType)R[0][w]; }
    for(int32 w = 0; w < 3; w++) { E[1][w] = (MatrixType)R[1][w]; }
    for(int32 w = 0; w < 3; w++) { E[2][w] = (MatrixType)R[2][w]; }
    E[0][3] = (MatrixType)X;
    E[1][3] = (MatrixType)Y;
    E[2][3] = (MatrixType)Z;
    E[3][3] = (MatrixType)1;

    CP.calculateDerrived();
    return CP;
  };

  std::vector<tCamParams> PoseTraceCams;

  std::string Line;
  std::getline(File, Line);
  {
    std::vector<flt64> Values;
    if(TryParseValues(Line, Values)) { PoseTraceCams.push_back(BuildParams(Values[0], Values[1], Values[2], Values[3], Values[4], Values[5])); }
    //otherwise this is a header line - discard it
  }

  while(std::getline(File, Line))
  {
    if(Line.find_first_not_of(" \t\r\n") == std::string::npos) { continue; } //tolerate trailing blank lines

    std::vector<flt64> Values;
    if(!TryParseValues(Line, Values)) { return false; }

    PoseTraceCams.push_back(BuildParams(Values[0], Values[1], Values[2], Values[3], Values[4], Values[5]));
  }
  if(PoseTraceCams.empty()) { return false; }

  setCamParams(CamGroupIdx, PoseTraceCams);
  return true;
}
template <class MatrixType> bool xCameraParamsManager<MatrixType>::loadRealCams(eCamParamsFormat Format, const std::string& CamParamsFile, const std::string& CamNameTemplate, int32V2 Resolution)
{
  switch(Format)
  {
  case eCamParamsFormat::PLY:
    return loadParams(c_RealGroupIdx, CamParamsFile, Resolution);
  case eCamParamsFormat::COLMAP:
    return loadParams(c_RealGroupIdx, CamParamsFile);
  case eCamParamsFormat::INI:
  {
    xCfgINI::xParser IniPreScan;
    if(!IniPreScan.loadFromFile(CamParamsFile)) { return false; }
    std::vector<std::string> CamIdStrs = IniPreScan.getSectionsNames();
    if(CamIdStrs.empty()) { return false; }
    setCamGroup(c_RealGroupIdx, CamIdStrs);
    initAllCams();
    return loadParams(c_RealGroupIdx, CamNameTemplate, CamParamsFile) && getNumRealCams() > 0;
  }
  default:
    return false;
  }
}
template <class MatrixType> void xCameraParamsManager<MatrixType>::setCamParams(int32 CamGroupIdx, const std::vector<tCamParams>& Cams)
{
  m_SelCamIdx[CamGroupIdx].clear();
  m_SelCamIdx[CamGroupIdx].reserve(Cams.size());

  int32 CamIdx = (int32)m_AllCamParams.size(); //continue numbering across groups - m_AllCamParams is shared by all groups
  for(const tCamParams& Cam : Cams)
  {
    tCamParams CP = Cam;
    CP.setIdx(CamIdx);
    m_AllCamParams[CamIdx] = std::move(CP);
    m_SelCamParams[CamGroupIdx][CamIdx] = &m_AllCamParams[CamIdx];
    m_SelCamIdx[CamGroupIdx].push_back(CamIdx);
    CamIdx++;
  }
}

//===============================================================================================================================================================================================================

template class xCameraParamsManager<flt32>;
template class xCameraParamsManager<flt64>;

//===============================================================================================================================================================================================================

} //end of namespace PMBB
