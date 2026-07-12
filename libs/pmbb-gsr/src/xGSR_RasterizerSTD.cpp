/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xGSR_RasterizerSTD.h"
#include <cmath>
#include <algorithm>

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

void xRasterizerSTD::RasterizeTile(xPicP* DstPic, const xProjectedCloudP& Projected, const tSplIdxV& SplatIndicesTile, int32V2 TileIdxXY, bool CapEffectiveAlpha)
{
  const int32 NumSplats = SplatIndicesTile.size();
  if(NumSplats == 0) { return; }

  const int32 PicWidth  = DstPic->getWidth ();
  const int32 PicHeight = DstPic->getHeight();

  // Pixel bounds of this tile (clamped to image)
  const int32 PixX0 = TileIdxXY.getX() * c_TileSize;
  const int32 PixY0 = TileIdxXY.getY() * c_TileSize;
  const int32 PixX1 = std::min(PixX0 + c_TileSize - 1, PicWidth  - 1);
  const int32 PixY1 = std::min(PixY0 + c_TileSize - 1, PicHeight - 1);

  //local tile cache
  flt32 TileBuff[4][c_TileSize][c_TileSize] = {}; //[RGBA][y][x]

  // Cached SoA pointers
  const flt32* PtrScreenX   = Projected.getScreenX   ();
  const flt32* PtrScreenY   = Projected.getScreenY   ();
  const flt32* PtrInvCovA   = Projected.getInvCov2D_A();
  const flt32* PtrInvCovB   = Projected.getInvCov2D_B();
  const flt32* PtrInvCovC   = Projected.getInvCov2D_C();
  const int32* PtrAABBMinX  = Projected.getAABBMinX  ();
  const int32* PtrAABBMinY  = Projected.getAABBMinY  ();
  const int32* PtrAABBMaxX  = Projected.getAABBMaxX  ();
  const int32* PtrAABBMaxY  = Projected.getAABBMaxY  ();
  const flt32* PtrColorR    = Projected.getColorR    ();
  const flt32* PtrColorG    = Projected.getColorG    ();
  const flt32* PtrColorB    = Projected.getColorB    ();
  const flt32* PtrAlpha     = Projected.getAlpha     ();

  // Iterate Gaussians in front-to-back order (guaranteed by xTileAssigner)
  for(int32 i = 0; i < NumSplats; i++)
  {
    const int32 Idx = SplatIndicesTile[i];

    // Intersect Gaussian AABB with tile pixel bounds
    const int32 X0 = std::max(PtrAABBMinX[Idx], PixX0);
    const int32 Y0 = std::max(PtrAABBMinY[Idx], PixY0);
    const int32 X1 = std::min(PtrAABBMaxX[Idx], PixX1);
    const int32 Y1 = std::min(PtrAABBMaxY[Idx], PixY1);
    if(X0 > X1 || Y0 > Y1) { continue; }

    const flt32 ScreenX = PtrScreenX[Idx];
    const flt32 ScreenY = PtrScreenY[Idx];
    const flt32 InvCovA = PtrInvCovA[Idx];
    const flt32 InvCovB = PtrInvCovB[Idx];
    const flt32 InvCovC = PtrInvCovC[Idx];
    const flt32 R       = PtrColorR [Idx];
    const flt32 G       = PtrColorG [Idx];
    const flt32 B       = PtrColorB [Idx];
    const flt32 A       = PtrAlpha  [Idx];

    // Rasterize: for each pixel in the intersection
    for(int32 Py = Y0; Py <= Y1; Py++)
    {
      const int32  TLy     = Py - PixY0; // local tile row
      const flt32  Dy      = c_PelCenter0o5 ? ((flt32)Py + 0.5f) - ScreenY : (flt32)Py - ScreenY; // MGM: pixel centre at +0.5
      const flt32  icC_Dy  = InvCovC * Dy;
      const flt32  icB_Dy  = InvCovB * Dy;

      for(int32 Px = X0; Px <= X1; Px++)
      {
        const int32 TLx = Px - PixX0; // local tile col

        // Early exit if tile pixel already saturated (accumulated alpha >= c_AccAlphaSat)
        if(TileBuff[(int32)eCmp::A][TLy][TLx] >= c_AccAlphaSat) { continue; }

        // EWA quadratic form: d^T * Sigma^{-1} * d  where d = (Dx, Dy) = icA*Dx^2 + 2*icB*Dx*Dy + icC*Dy^2
        // EWA weight: same quadratic form as MGM (mathematically equivalent to MGM's sigma)
        const flt32 Dx     = c_PelCenter0o5 ? ((flt32)Px + 0.5f) - ScreenX : (flt32)Px - ScreenX; // MGM: pixel centre at +0.5
        const flt32 Power  = -0.5f * (InvCovA*Dx*Dx + 2.0f*icB_Dy*Dx + icC_Dy*Dy);

        // Guard against positive/degenerate values (same as MGM's sigma < 0 check)
        if(Power > 0.0f || Power < c_PowerThreshold) { continue; }

        const flt32 Weight   = std::exp(Power);
        const flt32 AlphaEff = CapEffectiveAlpha ? std::min(A * Weight, c_EffectiveAlphaCap) : A * Weight; // MGM: cap effective alpha at 0.999

        if(AlphaEff < c_AlphaThreshold) { continue; }

        // Alpha compositing: front-to-back blending
        const flt32 Transmit = (1.0f - TileBuff[(int32)eCmp::A][TLy][TLx]);
        const flt32 Contrib  = AlphaEff * Transmit;

        TileBuff[(int32)eCmp::R][TLy][TLx] += Contrib * R;
        TileBuff[(int32)eCmp::G][TLy][TLx] += Contrib * G;
        TileBuff[(int32)eCmp::B][TLy][TLx] += Contrib * B;
        TileBuff[(int32)eCmp::A][TLy][TLx] += Contrib;
      } //end of loop over AABB x
    } //end of loop over AABB y

    if constexpr(c_TileTerminate)
    {
      if(i && ((i & 0x3F) == 0)) //every 63
      {
        bool Continue = false;
        const flt32* PtrTileA = &(TileBuff[(int32)eCmp::A][0][0]);
        for(int32 p = 0; p < c_TileArea; p++)
        {
          if(PtrTileA[p] < c_AccAlphaSat) { Continue = true; break; }
        }
        if(!Continue) { break; }
      }
    }
  } //end of loop over input splats

  //copy from tile to pic
  const int32 DstStride = DstPic->getStride();
  const int32 MaxVal    = xBitDepth2MaxValue(DstPic->getBitDepth());

  for(int32 CmpIdx = 0; CmpIdx < 3; CmpIdx++)
  {
    uint16* restrict DstPtr = DstPic->getAddr((eCmp)CmpIdx);
    for(int32 y = 0; y <= PixY1 - PixY0; y++)
    {
      const int32 PicY = PixY0 + y;
      uint16* restrict DstRowPtr = DstPtr + PicY * DstStride;
      for(int32 x = 0; x <= PixX1 - PixX0; x++)
      {
        // TODO - std::round is slow, use PMBB fast rounding routines

        const int32 PicX = PixX0 + x;
        flt32  SrcVal = TileBuff[CmpIdx][y][x];
        uint16 DstVal = (uint16)xClip((int32)std::round(SrcVal * (flt32)MaxVal), 0, MaxVal);
        DstRowPtr[PicX] = DstVal;
      }
    }
  }
}
void xRasterizerSTD::RasterizePic(tRstBuf* DstPic, const xProjectedCloudI& Projected, bool CapEffectiveAlpha)
{
  flt32* restrict DstPtrR   = DstPic->getAddr  (eCmp::R);
  flt32* restrict DstPtrG   = DstPic->getAddr  (eCmp::G);
  flt32* restrict DstPtrB   = DstPic->getAddr  (eCmp::B);
  flt32* restrict DstPtrA   = DstPic->getAddr  (eCmp::A);
  const int32     DstStride = DstPic->getStride();

  for(int32 i=0; i< Projected.getNumValid(); i++)
  {
    const int32 Idx = Projected.getSortedIdx()[i];

    const xProjectedSplatData& S = Projected.access(Idx);

    //if(S.AABBy0 < 0 || S.AABBx0 < 0 || S.AABBy1 >= DstPic->getHeight() || S.AABBx1 >= DstPic->getWidth())
    //{
    //  int32 rrr = 0;
    //}

    for(int32 Py = S.AABBy0; Py <= S.AABBy1; Py++)
    {
      const flt32  Dy     = ((flt32)Py + 0.5f) - S.ScreenY;  // mpeg: pixel centre at +0.5
      const flt32  icC_Dy = S.InvCovC * Dy;
      const flt32  icB_Dy = S.InvCovB * Dy;

      flt32* RowR = DstPtrR + Py * DstStride;
      flt32* RowG = DstPtrG + Py * DstStride;
      flt32* RowB = DstPtrB + Py * DstStride;
      flt32* RowA = DstPtrA + Py * DstStride;

      for(int32 Px = S.AABBx0; Px <= S.AABBx1; Px++)
      {
        // Early exit if pixel already saturated (accumulated alpha >= 0.9999)
        if(RowA[Px] >= c_AccAlphaSat) { continue; }

        // EWA weight: same quadratic form as PMBB (mathematically equivalent to mpeg's sigma)
        const flt32 Dx    = ((flt32)Px + 0.5f) - S.ScreenX;  // mpeg: pixel centre at +0.5
        const flt32 Power = -0.5f * (S.InvCovA*Dx*Dx + 2.0f*icB_Dy*Dx + icC_Dy*Dy);

        // Guard against positive/degenerate values (same as mpeg's sigma < 0 check)
        if(Power > 0.0f || Power < c_PowerThreshold) { continue; }

        const flt32 Weight   = std::exp(Power);
        const flt32 AlphaEff = CapEffectiveAlpha ? std::min(S.Alpha * Weight, c_EffectiveAlphaCap) : S.Alpha * Weight; // MGM: cap effective alpha at 0.999

        if(AlphaEff < c_AlphaThreshold) { continue; }

        // Front-to-back alpha compositing
        const flt32 Transmit = 1.0f - RowA[Px];
        const flt32 Contrib  = AlphaEff * Transmit;

        RowR[Px] += Contrib * S.ColorR;
        RowG[Px] += Contrib * S.ColorG;
        RowB[Px] += Contrib * S.ColorB;
        RowA[Px] += Contrib;
      }
    }
  }
}
void xRasterizerSTD::RasterizePic(tRstBuf* DstPic, const xProjectedCloudP& Projected,  bool CapEffectiveAlpha)
{
  flt32* restrict DstPtrR   = DstPic->getAddr  (eCmp::R);
  flt32* restrict DstPtrG   = DstPic->getAddr  (eCmp::G);
  flt32* restrict DstPtrB   = DstPic->getAddr  (eCmp::B);
  flt32* restrict DstPtrA   = DstPic->getAddr  (eCmp::A);
  const int32     DstStride = DstPic->getStride();

}

//===============================================================================================================================================================================================================

} //end of namespace PMBB::GSR
