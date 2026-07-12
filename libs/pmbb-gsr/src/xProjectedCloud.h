/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xMemory.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================
// xProjectedCloudP - SoA container for per-Gaussian data after projection. 
//===============================================================================================================================================================================================================
class xProjectedCloudP
{
protected:
  int32  m_MaxNumSplats = 0;
  int32  m_NumValid     = 0; // filled by projector after culling

  //2D screen position 
  flt32* m_ScreenX      = nullptr; // pixel X of Gaussian center
  flt32* m_ScreenY      = nullptr; // pixel Y of Gaussian center
  //depth (camera space Z, positive = in front) 
  flt32* m_Depth        = nullptr; // used as sort key
  //inverse 2D covariance [ic_A ic_B; ic_B ic_C] = Sigma_2D^{-1} stored so rasterizer can compute d^T * Sigma^{-1} * d directly
  flt32* m_InvCov2D_A   = nullptr;
  flt32* m_InvCov2D_B   = nullptr;
  flt32* m_InvCov2D_C   = nullptr;
  //integer AABB (Axis Aligned Bounding Box) of the 3-sigma ellipse on screen (clamped to image bounds) 
  int32* m_AABBMinX     = nullptr;
  int32* m_AABBMinY     = nullptr;
  int32* m_AABBMaxX     = nullptr;
  int32* m_AABBMaxY     = nullptr;
  //color from SH evaluation (linear, not clamped) 
  flt32* m_ColorR       = nullptr;
  flt32* m_ColorG       = nullptr;
  flt32* m_ColorB       = nullptr;
  //opacity: sigmoid(OP) 
  flt32* m_Alpha        = nullptr;
  //sort output: indices of valid splats ordered back-to-front
  int32* m_SortedIdx    = nullptr;

public:
  void upsize (int32 MaxNumSplats);
  void destroy();

  //validity
  int32 getMaxNumSplats() const { return m_MaxNumSplats; }
  int32 getNumValid    () const { return m_NumValid;     }
  void  setNumValid    (uint32 N) { m_NumValid = N;      }

  //per-splat write access (used by projector)
  void setScreenX   (uint32 Idx, flt32  V) { m_ScreenX[Idx] = V; }
  void setScreenY   (uint32 Idx, flt32  V) { m_ScreenY[Idx] = V; }
  void setDepth     (uint32 Idx, flt32  V) { m_Depth  [Idx] = V; }
  void setInvCov2D  (uint32 Idx, flt32  A, flt32 B, flt32 C) { m_InvCov2D_A[Idx]=A; m_InvCov2D_B[Idx]=B; m_InvCov2D_C[Idx]=C; }
  void setAABB      (uint32 Idx, int32 X0, int32 Y0, int32 X1, int32 Y1) { m_AABBMinX[Idx]=X0; m_AABBMinY[Idx]=Y0; m_AABBMaxX[Idx]=X1; m_AABBMaxY[Idx]=Y1; }
  void setColor     (uint32 Idx, flt32  R, flt32 G, flt32 B) { m_ColorR[Idx]=R; m_ColorG[Idx]=G; m_ColorB[Idx]=B; }
  void setAlpha     (uint32 Idx, flt32  V) { m_Alpha  [Idx] = V; }

  //array read access (used by sorter, tile assigner, rasterizer)
  const flt32* getScreenX   () const { return m_ScreenX;    }
  const flt32* getScreenY   () const { return m_ScreenY;    }
  const flt32* getDepth     () const { return m_Depth;      }
  const flt32* getInvCov2D_A() const { return m_InvCov2D_A; }
  const flt32* getInvCov2D_B() const { return m_InvCov2D_B; }
  const flt32* getInvCov2D_C() const { return m_InvCov2D_C; }
  const int32* getAABBMinX  () const { return m_AABBMinX;   }
  const int32* getAABBMinY  () const { return m_AABBMinY;   }
  const int32* getAABBMaxX  () const { return m_AABBMaxX;   }
  const int32* getAABBMaxY  () const { return m_AABBMaxY;   }
  const flt32* getColorR    () const { return m_ColorR;     }
  const flt32* getColorG    () const { return m_ColorG;     }
  const flt32* getColorB    () const { return m_ColorB;     }
  const flt32* getAlpha     () const { return m_Alpha;      }
        int32* getSortedIdx ()       { return m_SortedIdx;  }
  const int32* getSortedIdx () const { return m_SortedIdx;  }
};

//===============================================================================================================================================================================================================

struct PMBB_ALIGN_CACHE xProjectedSplatData
{
  //2D screen position 
  flt32 ScreenX;
  flt32 ScreenY;
  //depth (camera space Z, positive = in front) 
  flt32 Depth;
  //inverse 2D covariance [ic_A ic_B; ic_B ic_C] = Sigma_2D^{-1} stored so rasterizer can compute d^T * Sigma^{-1} * d directly
  flt32 InvCovA;
  flt32 InvCovB;
  flt32 InvCovC;
  //integer AABB (Axis Aligned Bounding Box) of the 3-sigma ellipse on screen (clamped to image bounds) 
  int32 AABBx0;
  int32 AABBy0;
  int32 AABBx1;
  int32 AABBy1;
  //color from SH evaluation (linear, not clamped) 
  flt32 ColorR;
  flt32 ColorG;
  flt32 ColorB;
  //opacity: sigmoid(OP) 
  flt32 Alpha;
};

class xProjectedCloudI
{
protected:
  int32 m_MaxNumSplats = 0;
  int32 m_NumValid     = 0; // filled by projector after culling

  xProjectedSplatData* m_ProjectedSplatData = nullptr;
  //sort output: indices of valid splats ordered back-to-front
  int32*               m_SortedIdx          = nullptr;


public:
  void upsize (int32 NumSplats);
  void destroy(               );

  int32 getMaxNumSplats(        ) const { return m_MaxNumSplats; }
  int32 getNumValid    (        ) const { return m_NumValid; }
  void   setNumValid    (uint32 N)       { m_NumValid = N; }

  const xProjectedSplatData* access() const { return m_ProjectedSplatData; }
  xProjectedSplatData& access(int32 Idx) { return m_ProjectedSplatData[Idx]; }
  const xProjectedSplatData& access(int32 Idx) const { return m_ProjectedSplatData[Idx]; }

  int32* getSortedIdx() { return m_SortedIdx; }
  const int32* getSortedIdx() const { return m_SortedIdx; }

};

//===============================================================================================================================================================================================================

} //end of namespace PMBB::GSR
