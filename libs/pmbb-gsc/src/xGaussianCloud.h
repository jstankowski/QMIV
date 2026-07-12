/*
    SPDX-FileCopyrightText: 2025-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelkoi@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSC.h"
#include <unordered_map>
#include <vector>

namespace PMBB_NAMESPACE::GSC {

//===============================================================================================================================================================================================================

class xGaussianCloudCmn
{
public:
  using tStr = std::string;

  class xCameraPosition
  {
  public:
    int32   m_ImgIdx  = NOT_VALID;
    int32   m_CamIdx  = NOT_VALID;
    flt64V3 m_PosXYZ ;
    flt64V4 m_OriWXYZ; // quaternion w,x,y,z
    flt64V2 m_FocalXY;
    tStr    m_Name   ;

  public:
    xCameraPosition() {}

    xCameraPosition(int32 ImgIdx, int32 CamIdx, flt64V3 PosXYZ, flt64V4 OriWXYZ, flt64V2 FocalXY, tStr Name)
    {
      m_ImgIdx  = ImgIdx ;
      m_CamIdx  = CamIdx ;
      m_PosXYZ  = PosXYZ ;
      m_OriWXYZ = OriWXYZ;
      m_FocalXY = FocalXY;
      m_Name    = Name   ;
    }
  };

  using tCameraData = std::vector<xCameraPosition>;

protected:
  uint32 m_NumSplats     = 0;
  uint32 m_AttCount      = 0;
  uint32 m_SphHarmCount  = 0;
  uint32 m_SphHarmDegree = 0;

  int64  m_POC       = NOT_VALID;
  int64  m_Timestamp = NOT_VALID;


public:
  void setNumSplats     (uint32 NumSplats    ) { m_NumSplats     = NumSplats    ; }
  void setAttCount      (uint32 AttCount     ) { m_AttCount      = AttCount     ; }
  void setSphHarmDegree (uint32 SphHarmDegree) { m_SphHarmDegree = SphHarmDegree; }
  void setSphHarmCount  (uint32 SphHarmCount ) { m_SphHarmCount  = SphHarmCount ; }

  uint32 getNumSplats     () const { return m_NumSplats    ; }
  uint32 getAttCount      () const { return m_AttCount     ; }
  uint32 getSphHarmDegree () const { return m_SphHarmDegree; }
  uint32 getSphHarmCount  () const { return m_SphHarmCount ; }

  //time
  inline void  setPOC      (int64 POC      )       { m_POC = POC; }
  inline int64 getPOC      (               ) const { return m_POC; }
  inline void  setTimestamp(int64 Timestamp)       { m_Timestamp = Timestamp; }
  inline int64 getTimestamp(               ) const { return m_Timestamp; }

};

//===============================================================================================================================================================================================================

class xGaussianCloud : public xGaussianCloudCmn
{
public:
  xGaussianCloud () {};
  ~xGaussianCloud() { destroy(); };

private:
  tCameraData m_CameraData;
  std::array< flt32*, static_cast<size_t>(xAttrFLT::COUNT)> m_AttributesFLT = { nullptr };
  std::array<uint16*, static_cast<size_t>(xAttrINT::COUNT)> m_AttributesINT = { nullptr };

public:
  void create (int32 NumSplats);
  void destroy();
  
  flt32*  getAttrAddr(xAttrFLT Attr) { return m_AttributesFLT[static_cast<size_t>(Attr)]; }
  uint16* getAttrAddr(xAttrINT Attr) { return m_AttributesINT[static_cast<size_t>(Attr)]; }

  const  flt32* getAttrAddr(xAttrFLT Attr) const { return m_AttributesFLT[static_cast<size_t>(Attr)]; }
  const uint16* getAttrAddr(xAttrINT Attr) const { return m_AttributesINT[static_cast<size_t>(Attr)]; }

  void setAttrVal(flt32  Value, xAttrFLT Attr, size_t SplatIdx, size_t SHOff = 0) { m_AttributesFLT[static_cast<size_t>(Attr)][SplatIdx + SHOff] = Value; }
  void setAttrVal(uint16 Value, xAttrINT Attr, size_t SplatIdx, size_t SHOff = 0) { m_AttributesINT[static_cast<size_t>(Attr)][SplatIdx + SHOff] = Value; }

  void setCameraData(const tCameraData& CameraData) { m_CameraData = CameraData; }

  const tCameraData& getCameraData() const { return m_CameraData; }

  //std::vector<flt32>&  getAttrAddr(xAttrFLT Attr) { return m_AttributesFLT[Attr]; }
  //std::vector<uint16>& getAttrAddr(xAttrINT Attr) { return m_AttributesINT[Attr]; }

  //void setAttribute(xAttrFLT Attr, uint32 SplatIdx,  flt32 Value, uint32 SHOff = 0);
  //void setAttribute(xAttrINT Attr, uint32 SplatIdx, uint16 Value, uint32 SHOff = 0);

};

//===============================================================================================================================================================================================================

class xGaussianObject : public xGaussianCloudCmn
{
  flt32V3 P;
  flt32V3 S;
  flt32V3 R;
  flt32   OP;
  flt32V3 H[16];
};

class xGaussianCloudI
{
  std::vector<xGaussianObject> m_CloudStorage;
};


//===============================================================================================================================================================================================================

} //end of namespace PMBB
