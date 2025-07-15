/*
    SPDX-FileCopyrightText: 2025 Patrycja Kaźmierczak <patrycja.kazmierczak@student.put.poznan.pl>
    SPDX-FileCopyrightText: 2025 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xCorrespPixelShiftNEON.h"

#if X_SIMD_CAN_USE_NEON

namespace PMBB_NAMESPACE {

//===============================================================================================================================================================================================================
// xCorrespPixelShiftNEON
//===============================================================================================================================================================================================================
uint64V4 xCorrespPixelShiftNEON::CalcDistAsymmetricRow(const xPicI* Tst, const xPicI* Ref, const int32 y, const int32V4& GlobalColorShift, const int32 SearchRange, const int32V4& CmpWeights)
{
  assert(Tst->isCompatible(Ref));

  const int32     Width     = Tst->getWidth();
  const int32     TstStride = Tst->getStride();
  const int32     TstOffset = y * TstStride;
  const int32x4_t CmpWeightsV       = vld1q_s32(CmpWeights      .getPtr());
  const int32x4_t GlobalColorShiftV = vld1q_s32(GlobalColorShift.getPtr());
 
  const uint16V4* TstPtr = Tst->getAddr() + TstOffset;
  int32x4_t RowDistV = vdupq_n_s32(0);
  for (int32 x = 0; x < Width; x++)
  {
    uint16x4_t TstV     = vld1_u16((TstPtr + x)->getPtr()); 
    int32x4_t  TstClrV  = vaddq_s32(GlobalColorShiftV, vreinterpretq_s32_u32(vmovl_u16(TstV))); 
    int32x4_t  BestDist = xCalcDistWithinBlock(TstClrV, Ref, x, y, SearchRange, CmpWeightsV);
    RowDistV = vaddq_s32(RowDistV, BestDist);
  }//x

  int32V4 RowDist;
  vst1q_s32(RowDist.getPtr(), RowDistV);
  return (uint64V4)RowDist;
}
int32x4_t xCorrespPixelShiftNEON::xCalcDistWithinBlock(const int32x4_t& TstPelV, const xPicI* Ref, const int32 CenterX, const int32 CenterY, const int32 SearchRange, const int32x4_t& CmpWeightsV)
{
  const int32 WindowSize = (SearchRange << 1) + 1;
  const int32 BegY       = CenterY - SearchRange;
  const int32 BegX       = CenterX - SearchRange;  

  const int32     Stride = Ref->getStride(); 
  const uint16V4* RefPtr = Ref->getAddr  () + BegY * Stride + BegX;
  //const uint16V4* RefPtrBeg = Ref->getAddr() + Stride*BegY + BegX; // adres poczatku obrazu + xy = adres poczatku okna

  int32     BestError = std::numeric_limits<int32>::max();
  int32x4_t BestDistV = vdupq_n_s32(0);
  
  for(int32 y = 0; y < WindowSize; y++)
  {
    const uint16V4* RefPtrY = RefPtr + y * Stride;
    for(int32 x = 0; x< WindowSize; x++)
    {
      //uint16x4_t RefV16 = vld1_u16((RefPtrBeg + y*Stride + x)->getElementsPtr()); // adres poczatku okna + yx = liczony pixel w oknie /getElements() wskazuje na pierwszy z 4 elementow pixela, wrzuca do 16x4
      uint16x4_t RefV16 = vld1_u16((RefPtrY + x)->getPtr());      
      int32x4_t  RefV   = vreinterpretq_s32_u32(vmovl_u16(RefV16));
      int32x4_t  Diff   = vsubq_s32(TstPelV, RefV); //tst32x4 - ref32x4
      int32x4_t  Dist   = vmulq_s32(Diff, Diff);    //^2
      int32x4_t  ErrorV = vmulq_s32(Dist, CmpWeightsV); 
      int32      Error  = vaddvq_s32(ErrorV);          // Error = suma[(tst-ref)^2*waga]
      if (Error < BestError) { BestError = Error; BestDistV = Dist; }
    } //x
  } //y
  return BestDistV;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// shift-compensated picture generation
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void xCorrespPixelShiftNEON::GenShftCompRow(xPicI* DstRef, const xPicI* Ref, const xPicI* Tst, const int32 y, const int32V4& GlobalColorShift, const int32 SearchRange, const int32V4& CmpWeights)
{
  const int32 Width     = Tst->getWidth ();
  const int32 TstStride = Tst->getStride();
  const int32 TstOffset = y * TstStride;
  const int32 MaxValue  = DstRef->getMaxPelValue();

  const int32x4_t CmpWeightsV       = vld1q_s32(CmpWeights      .getPtr());
  const int32x4_t GlobalColorShiftV = vld1q_s32(GlobalColorShift.getPtr());
  const int32x4_t MaxValueV         = vdupq_n_s32(MaxValue);

  const uint16V4*    TstPtr = Tst   ->getAddr() + TstOffset;
  uint16V4* restrict DstPtr = DstRef->getAddr() + TstOffset;

  for(int32 x = 0; x < Width; x++)
  {
    uint16x4_t TstV    = vld1_u16((uint16*)(TstPtr + x));
    int32x4_t  TstClrV = vaddq_s32(GlobalColorShiftV, vreinterpretq_s32_u32(vmovl_u16(TstV))); 
    int32x4_t  RefV    = xFindBestPixelWithinBlock(TstClrV, Ref, x, y, SearchRange, CmpWeightsV);
    int32x4_t  DstV    = vminq_s32(vsubq_s32(RefV, GlobalColorShiftV), MaxValueV);
    uint16x4_t DstU16V = vqmovun_s32(DstV);
    vst1_u16((uint16*)(DstPtr + x), DstU16V);
  }//x
}
int32x4_t xCorrespPixelShiftNEON::xFindBestPixelWithinBlock(const int32x4_t& TstPelV, const xPicI* Ref, const int32 CenterX, const int32 CenterY, const int32 SearchRange, const int32x4_t& CmpWeightsV)
{
  const int32 BegY = CenterY - SearchRange;
  const int32 EndY = CenterY + SearchRange;
  const int32 BegX = CenterX - SearchRange;
  const int32 EndX = CenterX + SearchRange;

  const uint16V4* RefPtr = Ref->getAddr  ();
  const int32     Stride = Ref->getStride();

  int32     BestError  = std::numeric_limits<int32>::max();
  int32x4_t BestPixelV = vdupq_n_s32(0);;

  for(int32 y = BegY; y <= EndY; y++)
  {
    const int32 OffsetY =  y * Stride;
    for(int32 x = BegX; x <= EndX; x++)
    {
      const int32 Offset = OffsetY + x;
      uint16x4_t RefV16 = vld1_u16((RefPtr + Offset)->getPtr());      
      int32x4_t  RefV   = vreinterpretq_s32_u32(vmovl_u16(RefV16));
      int32x4_t  Diff   = vsubq_s32(TstPelV, RefV); //tst32x4 - ref32x4
      int32x4_t  Dist   = vmulq_s32(Diff, Diff);    //^2
      int32x4_t  ErrorV = vmulq_s32(Dist, CmpWeightsV);
      int32      Error  = vaddvq_s32(ErrorV);
      if (Error < BestError) { BestError = Error; BestPixelV = RefV; }
    } //x
  } //y

  return BestPixelV;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB

#endif //X_SIMD_CAN_USE_NEON