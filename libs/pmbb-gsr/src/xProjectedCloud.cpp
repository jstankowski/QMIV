/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xProjectedCloud.h"

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

void xProjectedCloudP::upsize(int32 NumSplats)
{
  if(m_MaxNumSplats >= NumSplats) { return; }

  m_MaxNumSplats = NumSplats;
  m_NumValid     = 0;

  if(m_ScreenX   ) { xMemory::xAlignedFreeNull(m_ScreenX   ); }
  if(m_ScreenY   ) { xMemory::xAlignedFreeNull(m_ScreenY   ); }
  if(m_Depth     ) { xMemory::xAlignedFreeNull(m_Depth     ); }
  if(m_InvCov2D_A) { xMemory::xAlignedFreeNull(m_InvCov2D_A); }
  if(m_InvCov2D_B) { xMemory::xAlignedFreeNull(m_InvCov2D_B); }
  if(m_InvCov2D_C) { xMemory::xAlignedFreeNull(m_InvCov2D_C); }
  if(m_AABBMinX  ) { xMemory::xAlignedFreeNull(m_AABBMinX  ); }
  if(m_AABBMinY  ) { xMemory::xAlignedFreeNull(m_AABBMinY  ); }
  if(m_AABBMaxX  ) { xMemory::xAlignedFreeNull(m_AABBMaxX  ); }
  if(m_AABBMaxY  ) { xMemory::xAlignedFreeNull(m_AABBMaxY  ); }
  if(m_ColorR    ) { xMemory::xAlignedFreeNull(m_ColorR    ); }
  if(m_ColorG    ) { xMemory::xAlignedFreeNull(m_ColorG    ); }
  if(m_ColorB    ) { xMemory::xAlignedFreeNull(m_ColorB    ); }
  if(m_Alpha     ) { xMemory::xAlignedFreeNull(m_Alpha     ); }
  if(m_SortedIdx ) { xMemory::xAlignedFreeNull(m_SortedIdx ); }

  m_ScreenX    = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_ScreenY    = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_Depth      = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_InvCov2D_A = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_InvCov2D_B = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_InvCov2D_C = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_AABBMinX   = (int32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(int32));
  m_AABBMinY   = (int32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(int32));
  m_AABBMaxX   = (int32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(int32));
  m_AABBMaxY   = (int32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(int32));
  m_ColorR     = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_ColorG     = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_ColorB     = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_Alpha      = (flt32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(flt32));
  m_SortedIdx  = (int32*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(int32));
}
void xProjectedCloudP::destroy()
{
  m_MaxNumSplats = 0;
  m_NumValid     = 0;

  if(m_ScreenX   ) { xMemory::xAlignedFreeNull(m_ScreenX   ); }
  if(m_ScreenY   ) { xMemory::xAlignedFreeNull(m_ScreenY   ); }
  if(m_Depth     ) { xMemory::xAlignedFreeNull(m_Depth     ); }
  if(m_InvCov2D_A) { xMemory::xAlignedFreeNull(m_InvCov2D_A); }
  if(m_InvCov2D_B) { xMemory::xAlignedFreeNull(m_InvCov2D_B); }
  if(m_InvCov2D_C) { xMemory::xAlignedFreeNull(m_InvCov2D_C); }
  if(m_AABBMinX  ) { xMemory::xAlignedFreeNull(m_AABBMinX  ); }
  if(m_AABBMinY  ) { xMemory::xAlignedFreeNull(m_AABBMinY  ); }
  if(m_AABBMaxX  ) { xMemory::xAlignedFreeNull(m_AABBMaxX  ); }
  if(m_AABBMaxY  ) { xMemory::xAlignedFreeNull(m_AABBMaxY  ); }
  if(m_ColorR    ) { xMemory::xAlignedFreeNull(m_ColorR    ); }
  if(m_ColorG    ) { xMemory::xAlignedFreeNull(m_ColorG    ); }
  if(m_ColorB    ) { xMemory::xAlignedFreeNull(m_ColorB    ); }
  if(m_Alpha     ) { xMemory::xAlignedFreeNull(m_Alpha     ); }
  if(m_SortedIdx ) { xMemory::xAlignedFreeNull(m_SortedIdx ); }
}

//===============================================================================================================================================================================================================

void xProjectedCloudI::upsize(int32 NumSplats)
{
  if(m_MaxNumSplats >= NumSplats) { return; }

  m_MaxNumSplats = NumSplats;
  m_NumValid     = 0;

  if(m_ProjectedSplatData) { xMemory::xAlignedFreeNull(m_ProjectedSplatData); }
  if(m_SortedIdx         ) { xMemory::xAlignedFreeNull(m_SortedIdx         ); }

  m_ProjectedSplatData = (xProjectedSplatData*)xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(xProjectedSplatData));
  m_SortedIdx          = (int32*              )xMemory::xAlignedMallocPageAuto(NumSplats * sizeof(int32              ));

}
void xProjectedCloudI::destroy()
{
  m_MaxNumSplats = 0;
  m_NumValid     = 0;

  if(m_ProjectedSplatData) { xMemory::xAlignedFreeNull(m_ProjectedSplatData); }
  if(m_SortedIdx         ) { xMemory::xAlignedFreeNull(m_SortedIdx         ); }
}
//===============================================================================================================================================================================================================

} //end of namespace PMBB::GSR
