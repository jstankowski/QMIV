/*
    SPDX-FileCopyrightText: 2019-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include "xCommonDefFVGS.h"
#include "xCameraParams.h"

namespace PMBB_NAMESPACE::FVGS {

//===============================================================================================================================================================================================================

template <class MatrixType> class xCameraParamsManager
{
public:
  static constexpr int32 c_MaxNumCamGroups = 4;
  static constexpr int32 c_RealGroupIdx    = 0;
  static constexpr int32 c_VirtGroupIdx    = 1;

  using tCamIdStrVec      = std::vector<std::string>;
  using tCamIdxVec        = std::vector<int32>;
  using tCamIdStrToCamIdx = std::map<std::string, int32>;
  using tCamIdxToCamIdStr = std::map<int32, std::string>;
  using tCamParams        = xCameraParams<MatrixType>;
  using tCamParamsMap     = std::map<int32, tCamParams>;
  using tCamParamsPtrMap  = std::map<int32, tCamParams*>;

protected:
  tCamIdStrVec m_SelCamIdStr[c_MaxNumCamGroups];
  tCamIdStrVec m_AllCamIdStr;
  tCamIdxVec   m_SelCamIdx[c_MaxNumCamGroups];

  tCamIdStrToCamIdx m_CamIdStrToCamIdx;
  tCamIdxToCamIdStr m_CamIdxToCamIdStr;
  tCamParamsMap     m_AllCamParams;
  tCamParamsPtrMap  m_SelCamParams[c_MaxNumCamGroups];

public:
  void  setCamGroup(int32 CamGroupIdx, const tCamIdStrVec& CamIdStrs) { m_SelCamIdStr[CamGroupIdx] = CamIdStrs; }
  void  initAllCams( );
  //load from old style DERS-like .txt file
  void  loadParams (int32 CamGroupIdx, std::string CamParamsTemplate, std::string CamParamsFile, MatrixType Znear, MatrixType Zfar, int32V2 Resolution);
  //load from .cfg file in INI format
  bool  loadParams (int32 CamGroupIdx, const std::string& CamNameTemplate, const std::string& CamParamsFilePath);
  //load from COLMAP directory (cameras+images; .txt preferred, if absent find .bin)
  bool  loadParams (int32 CamGroupIdx, const std::string& CamParamsDir);
  //load from camera_position entries embedded in a Gaussian-splat PLY header
  bool  loadParams (int32 CamGroupIdx, const std::string& PlyFilePath, int32V2 Resolution);
  //load from posetrace CSV (X,Y,Z,Yaw,Pitch,Roll per row), reusing intrinsics/Znear/Zfar from an already-loaded real camera
  bool  loadPoseTraceParams(int32 CamGroupIdx, const std::string& PoseTraceFilePath, int32V2 Resolution);
  //load real cameras (c_RealGroupIdx) dispatching on format - PLY/COLMAP/INI - the CamNameTemplate is only used for INI
  bool  loadRealCams(eCamParamsFormat Format, const std::string& CamParamsFile, const std::string& CamNameTemplate, int32V2 Resolution);
  //adopt already-computed params (e.g. parsed from a PLY's camera_position entries by the caller)
  void  setCamParams(int32 CamGroupIdx, const std::vector<tCamParams>& Cams);

  void  overwriteInvalidZs(MatrixType Znear, MatrixType Zfar) { for(auto& [CamIdx, CamParams] : m_AllCamParams) { if(!CamParams.hasValidZs()) { CamParams.setZnear(Znear); CamParams.setZfar(Zfar); } } }
  void  overwriteAllZs    (MatrixType Znear, MatrixType Zfar) { for(auto& [CamIdx, CamParams] : m_AllCamParams) { CamParams.setZnear(Znear); CamParams.setZfar(Zfar); } }
  
  void  rescaleAllToNewResolution(int32V2 Resolution ) { for(auto& [CamIdx, CamParams] : m_AllCamParams) { CamParams.rescaleToNewResolution(Resolution); } }


public:
  int32             getNumCams(int32 CamGroupIdx) const { return (int32)m_SelCamIdx[CamGroupIdx].size(); }
  const tCamIdxVec& getCamIdx (int32 CamGroupIdx) const { return m_SelCamIdx[CamGroupIdx]; }

  int32              CamIdStrToCamIdx(const std::string& CamIdStr) const { return m_CamIdStrToCamIdx.at(CamIdStr); }
  const std::string& CamIdxToCamIdStr(int32              CamIdx  ) const { return m_CamIdxToCamIdStr.at(CamIdx); }

  const tCamIdStrToCamIdx& getCamIdStrToCamIdx() const { return m_CamIdStrToCamIdx; }
  const tCamIdxToCamIdStr& getCamIdxToCamIdStr() const { return m_CamIdxToCamIdStr; }

  const tCamParams&    getCamParams    (int32 CamIdx) const { return m_AllCamParams.at(CamIdx); }
  const tCamParamsMap& getAllCamParams (            ) const { return m_AllCamParams; }

public: 
  int32 getNumRealCams() const { return getNumCams(c_RealGroupIdx); }
  int32 getNumVirtCams() const { return getNumCams(c_VirtGroupIdx); }

  const tCamIdxVec& getRealCamIdx() const { return getCamIdx(c_RealGroupIdx); }
  const tCamIdxVec& getVirtCamIdx() const { return getCamIdx(c_VirtGroupIdx); }

  const tCamParamsPtrMap& getRealCamParams() { return m_SelCamParams[c_RealGroupIdx]; }
  const tCamParamsPtrMap& getVirtCamParams() { return m_SelCamParams[c_VirtGroupIdx]; }
};

//===============================================================================================================================================================================================================

#ifndef PMBB_xCameraParamsManager_IMPLEMENTATION
extern template class xCameraParamsManager<flt32>;
extern template class xCameraParamsManager<flt64>;
#endif // !PMBB_xCameraParamsManager_IMPLEMENTATION

//===============================================================================================================================================================================================================

} //end of namespace PMBB

