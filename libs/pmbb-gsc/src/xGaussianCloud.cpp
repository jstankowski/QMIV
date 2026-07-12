/*
    SPDX-FileCopyrightText: 2025-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelkoi@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xGaussianCloud.h"
#include "xMemory.h"

namespace PMBB_NAMESPACE::GSC {

//===============================================================================================================================================================================================================

void xGaussianCloud::create(int32 NumSplats)
{
  m_NumSplats = NumSplats;

  const int32 AttrFLTCount = (int32)xAttrFLT::COUNT;
  for(int32 AttrFLTIdx = 0; AttrFLTIdx < AttrFLTCount; AttrFLTIdx++)
  {
    m_AttributesFLT[AttrFLTIdx] = (flt32*)xMemory::xAlignedMallocPageAuto(m_NumSplats * sizeof(flt32));
    xMemsetX(m_AttributesFLT[AttrFLTIdx], (flt32)0, m_NumSplats);
  }

  const int32 AttrINTCount = (int32)xAttrINT::COUNT;
  for(int32 AttrINTIdx = 0; AttrINTIdx < AttrINTCount; AttrINTIdx++)
  {
    m_AttributesINT[AttrINTIdx] = (uint16*)xMemory::xAlignedMallocPageAuto(m_NumSplats * sizeof(uint16));
    xMemsetX(m_AttributesINT[AttrINTIdx], (uint16)0, m_NumSplats);
  }
}
void xGaussianCloud::destroy()
{
  const int32 AttrFLTCount = (size_t)xAttrFLT::COUNT;
  for(int32 AttrFLTIdx = 0; AttrFLTIdx < AttrFLTCount; AttrFLTIdx++)
  {
    if(m_AttributesFLT[AttrFLTIdx] != nullptr) { xMemory::xAlignedFreeNull(m_AttributesFLT[AttrFLTIdx]); }
  }

  const int32 AttrINTCount = (size_t)xAttrINT::COUNT;
  for(int32 AttrINTIdx = 0; AttrINTIdx < AttrINTCount; AttrINTIdx++)
  {
    if(m_AttributesINT[AttrINTIdx] != nullptr) { xMemory::xAlignedFreeNull(m_AttributesINT[AttrINTIdx]); }
  }
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB
