/*
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blażej.szydełko@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#include "xGSR_ProjectorSTD.h"
#include "xSphHarmUtils.h"   // xSphHarmEvaluator
#include "xCameraModel.h"    // xRotationMat, xExtrinsicMat, xIntrinsicMat
#include <cmath>
#include <algorithm>

namespace PMBB_NAMESPACE::GSR {

//===============================================================================================================================================================================================================

template<class tProjectedCloud> void xProjectorSTD::ProjectRng(tProjectedCloud* Projected, uint8* ValidMask, const tGaussC* Cloud, const tCamPar& CamParams, int32V2 PicSize, uint32 BegIdx, uint32 EndIdx)
{
  // camera parameters
  const flt32V2 FocalLen = (flt32V2)CamParams.getIntrinsics().getFocalLengths  (); // fx, fy
  const flt32V2 PrinciPt = (flt32V2)CamParams.getIntrinsics().getPrincipalPoint(); // cx, cy
  const flt32   Fx = FocalLen.getX(), Fy = FocalLen.getY();
  const flt32   Cx = PrinciPt.getX(), Cy = PrinciPt.getY();
  const int32   W  = PicSize .getX(), H  = PicSize .getY();

  const auto& ExtLoc = CamParams.getExtrinsicsLocal();
  // Rotation matrix W (3x3) - transforms 3D point from world to camera space (upper-left of ExtrinsicsLocal)
  const flt32 W00 = ExtLoc.at(0,0), W01 = ExtLoc.at(0,1), W02 = ExtLoc.at(0,2);
  const flt32 W10 = ExtLoc.at(1,0), W11 = ExtLoc.at(1,1), W12 = ExtLoc.at(1,2);
  const flt32 W20 = ExtLoc.at(2,0), W21 = ExtLoc.at(2,1), W22 = ExtLoc.at(2,2);
  // Translation vector t (3x1) - ExtrinsicsLocal column 3 = -R*T_world
  const flt32 TX  = ExtLoc.at(0,3), TY  = ExtLoc.at(1,3), TZ  = ExtLoc.at(2,3);

  // SoA pointers from cloud
  const flt32* restrict PtrPX = Cloud->getAttrAddr(GSC::xAttrFLT::PX);
  const flt32* restrict PtrPY = Cloud->getAttrAddr(GSC::xAttrFLT::PY);
  const flt32* restrict PtrPZ = Cloud->getAttrAddr(GSC::xAttrFLT::PZ);
  const flt32* restrict PtrSX = Cloud->getAttrAddr(GSC::xAttrFLT::SX);
  const flt32* restrict PtrSY = Cloud->getAttrAddr(GSC::xAttrFLT::SY);
  const flt32* restrict PtrSZ = Cloud->getAttrAddr(GSC::xAttrFLT::SZ);
  const flt32* restrict PtrRX = Cloud->getAttrAddr(GSC::xAttrFLT::RX); // quaternion x
  const flt32* restrict PtrRY = Cloud->getAttrAddr(GSC::xAttrFLT::RY); // quaternion y
  const flt32* restrict PtrRZ = Cloud->getAttrAddr(GSC::xAttrFLT::RZ); // quaternion z
  const flt32* restrict PtrRW = Cloud->getAttrAddr(GSC::xAttrFLT::RW); // quaternion w
  const flt32* restrict PtrOP = Cloud->getAttrAddr(GSC::xAttrFLT::OP); // pre-sigmoid opacity

  // MGS mode - clamp limit for x/z and y/z (1.3 * half-FOV in normalised coords)
  const flt32 LimitX = 1.3f * (0.5f * (flt32)W) / Fx;
  const flt32 LimitY = 1.3f * (0.5f * (flt32)H) / Fy;

  for(uint32 i = BegIdx; i < EndIdx; i++)
  {
    // 1. Transform world position to camera space
    const flt32 Px = PtrPX[i], Py = PtrPY[i], Pz = PtrPZ[i];
    const flt32 CamX = W00*Px + W01*Py + W02*Pz + TX;
    const flt32 CamY = W10*Px + W11*Py + W12*Pz + TY;
    const flt32 CamZ = W20*Px + W21*Py + W22*Pz + TZ;

    // 2. Frustum cull: behind camera
    if(CamZ <= 0.0f) { ValidMask[i] = 0; continue; }

    // 3. Project center to screen (Screen position)
    const flt32 InvCamZ  = 1.0f / CamZ;
    const flt32 ScreenX  = Fx * CamX * InvCamZ + Cx;
    const flt32 ScreenY  = Fy * CamY * InvCamZ + Cy;

    // 4. Calc splat rotation matrix from quaternion (x,y,z,w) (Quaternion -> rotation matrix)
    const flt32 qx = PtrRX[i], qy = PtrRY[i], qz = PtrRZ[i], qw = PtrRW[i];
    const flt32 qLen = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw); // normalize quaternion
    const flt32 qxi = qx/qLen, qyi = qy/qLen, qzi = qz/qLen, qwi = qw/qLen;
    const flt32 xx=qxi*qxi, yy=qyi*qyi, zz=qzi*qzi;
    const flt32 xy=qxi*qyi, xz=qxi*qzi, yz=qyi*qzi;
    const flt32 wx=qwi*qxi, wy=qwi*qyi, wz=qwi*qzi;
    const flt32 Rs00=1-2*(yy+zz), Rs01=2*(xy-wz)  , Rs02=2*(xz+wy)  ; // R_splat (column-major stored as R[row][col]) ??????
    const flt32 Rs10=2*(xy+wz)  , Rs11=1-2*(xx+zz), Rs12=2*(yz-wx)  ;
    const flt32 Rs20=2*(xz-wy)  , Rs21=2*(yz+wx)  , Rs22=1-2*(xx+yy);

    // 5. Calc 3D covariance Sigma_3D = R_splat * diag(s^2) * R_splat^T // s = exp(log_scale)
    const flt32 Sx = std::exp(PtrSX[i]), Sy = std::exp(PtrSY[i]), Sz = std::exp(PtrSZ[i]);
    const flt32 Sx2 = Sx*Sx, Sy2 = Sy*Sy, Sz2 = Sz*Sz;
    const flt32 S00 = Rs00*Rs00*Sx2 + Rs01*Rs01*Sy2 + Rs02*Rs02*Sz2; // Sigma_3D[r][c] = sum_k Rs[r][k] * sk^2 * Rs[c][k]
    const flt32 S01 = Rs00*Rs10*Sx2 + Rs01*Rs11*Sy2 + Rs02*Rs12*Sz2;
    const flt32 S02 = Rs00*Rs20*Sx2 + Rs01*Rs21*Sy2 + Rs02*Rs22*Sz2;
    const flt32 S11 = Rs10*Rs10*Sx2 + Rs11*Rs11*Sy2 + Rs12*Rs12*Sz2;
    const flt32 S12 = Rs10*Rs20*Sx2 + Rs11*Rs21*Sy2 + Rs12*Rs22*Sz2;
    const flt32 S22 = Rs20*Rs20*Sx2 + Rs21*Rs21*Sy2 + Rs22*Rs22*Sz2;
    // Sigma_3D is symmetric, so S10=S01, S20=S02, S21=S12

    // 6. Project 3D covariance to 2D
      // Sigma_2D = J * W * Sigma_3D * W^T * J^T
      // J (2x3) = perspective Jacobian at (CamX, CamY, CamZ):
      //   J = [Fx/CamZ,       0, -Fx*CamX/CamZ^2]
      //       [      0, Fy/CamZ, -Fy*CamY/CamZ^2]
      // W (3x3) = camera rotation (world->camera)
      // T = J * W  (2x3)      
      // T[row][col] = J[row][0]*W[0][col] + J[row][1]*W[1][col] + J[row][2]*W[2][col]

      // Perspective Jacobian with mpeg-compat clamping:
      //    tX = CamZ * clamp(CamX/CamZ, -LimitX, LimitX)  (= CamX when not clamped)
      //    J = [Fx/CamZ,       0, -Fx*tX/CamZ^2]
      //        [0,       Fy/CamZ, -Fy*tY/CamZ^2]

    const flt32 InvCamZ2 = InvCamZ * InvCamZ;
    flt32 tX = CamX;
    flt32 tY = CamY;
    if constexpr (c_ProjClamping)
    {
      const flt32 XoZ = CamX * InvCamZ;
      const flt32 YoZ = CamY * InvCamZ;
      tX  = CamZ * std::max(-LimitX, std::min(LimitX, XoZ));
      tY  = CamZ * std::max(-LimitY, std::min(LimitY, YoZ));
    }
    const flt32 J00 = Fx * InvCamZ, J02 = -Fx * tX * InvCamZ2;
    const flt32 J11 = Fy * InvCamZ, J12 = -Fy * tY * InvCamZ2;
    // row 0: J[0][0]=J00, J[0][1]=0, J[0][2]=J02
    const flt32 T00 = J00 * W00 + J02 * W20;
    const flt32 T01 = J00 * W01 + J02 * W21;
    const flt32 T02 = J00 * W02 + J02 * W22;
    // row 1: J[1][0]=0, J[1][1]=J11, J[1][2]=J12
    const flt32 T10 = J11 * W10 + J12 * W20;
    const flt32 T11 = J11 * W11 + J12 * W21;
    const flt32 T12 = J11 * W12 + J12 * W22;

    // Sigma_2D = T * Sigma_3D * T^T
    flt32 ST0_0 = S00 * T00 + S01 * T01 + S02 * T02;
    flt32 ST0_1 = S01 * T00 + S11 * T01 + S12 * T02;
    flt32 ST0_2 = S02 * T00 + S12 * T01 + S22 * T02;
    flt32 ST1_0 = S00 * T10 + S01 * T11 + S02 * T12;
    flt32 ST1_1 = S01 * T10 + S11 * T11 + S12 * T12;
    flt32 ST1_2 = S02 * T10 + S12 * T11 + S22 * T12;
    flt32 CovA = T00 * ST0_0 + T01 * ST0_1 + T02 * ST0_2;
    flt32 CovB = T00 * ST1_0 + T01 * ST1_1 + T02 * ST1_2;
    flt32 CovC = T10 * ST1_0 + T11 * ST1_1 + T12 * ST1_2;

    // Regularize diagonal
    CovA += c_CovRegularize;
    CovC += c_CovRegularize;

    // 7. Invert 2D covariance (Invert Sigma_2D)
    const flt32 Det = CovA * CovC - CovB * CovB;
    if(Det < 1.0e-6f) { ValidMask[i] = 0; continue; }
    const flt32 InvDet  = 1.0f / Det;
    const flt32 InvCovA =  CovC * InvDet;
    const flt32 InvCovB = -CovB * InvDet;
    const flt32 InvCovC =  CovA * InvDet;

    // 8. Compute AABB (Axis Aligned Bounding Box) from largest eigenvalue of Sigma_2D 
    const flt32 Trace     = CovA + CovC;
    const flt32 Diff      = CovA - CovC;
    const flt32 LambdaMax = 0.5f * (Trace + std::sqrt(Diff*Diff + 4.0f*CovB*CovB));
    const int32 Radius    = (int32)std::ceil(std::sqrt(LambdaMax) * c_CullSigmas);
    const int32 AABBx0 = std::max(0,   (int32)std::floor(ScreenX) - Radius);
    const int32 AABBy0 = std::max(0,   (int32)std::floor(ScreenY) - Radius);
    const int32 AABBx1 = std::min(W-1, (int32)std::ceil (ScreenX) + Radius);
    const int32 AABBy1 = std::min(H-1, (int32)std::ceil (ScreenY) + Radius);
    // Cull if AABB is entirely outside image
    if(AABBx0 > AABBx1 || AABBy0 > AABBy1) { ValidMask[i] = 0; continue; }

    // 9. Evaluate SH color
    // 3DGS SH coefficients are trained with world-space direction from camera to Gaussian.
    // CamXYZ = R_w2c * (P - pos), so R_w2c^T * CamXYZ = P - pos = world-space cam-to-gauss vector.
    const flt32 DirLen = std::sqrt(CamX*CamX + CamY*CamY + CamZ*CamZ);
    const flt32 WX = W00*CamX + W10*CamY + W20*CamZ;
    const flt32 WY = W01*CamX + W11*CamY + W21*CamZ;
    const flt32 WZ = W02*CamX + W12*CamY + W22*CamZ;
    const flt32V3 ViewDir = { WX/DirLen, WY/DirLen, WZ/DirLen };
    const flt32V3 Color   = xEvalSphericalHarmonics(Cloud, i, ViewDir);

    // 10. Opacity: sigmoid(OP)
    const flt32 Alpha = 1.0f / (1.0f + std::exp(-PtrOP[i]));

    // Store to projected cloud
    if constexpr(std::is_same_v<tProjectedCloud, xProjectedCloudP>)
    {
      Projected->setScreenX (i, ScreenX);
      Projected->setScreenY (i, ScreenY);
      Projected->setDepth   (i, CamZ);
      Projected->setInvCov2D(i, InvCovA, InvCovB, InvCovC);
      Projected->setAABB    (i, AABBx0, AABBy0, AABBx1, AABBy1);
      Projected->setColor   (i, Color.getX(), Color.getY(), Color.getZ());
      Projected->setAlpha   (i, Alpha);      
    }
    if constexpr(std::is_same_v<tProjectedCloud, xProjectedCloudI>)
    {
      Projected->access(i) = { ScreenX, ScreenY, CamZ,
                               InvCovA, InvCovB, InvCovC,
                               AABBx0, AABBy0, AABBx1, AABBy1,
                               Color.getX(), Color.getY(), Color.getZ(), Alpha};
    }
    ValidMask[i] = 1;
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

flt32V3 xProjectorSTD::xEvalSphericalHarmonics(const tGaussC* Cloud, uint32 SplatIdx, flt32V3 ViewDir)
{
  using namespace GSC;

  const uint32 Degree = Cloud->getSphHarmDegree();
  const uint32 Count  = Cloud->getSphHarmCount(); // number of AC coefficients (0 for degree 0)

  // DC component
  const flt32 DC_R = Cloud->getAttrAddr(xAttrFLT::HR0)[SplatIdx];
  const flt32 DC_G = Cloud->getAttrAddr(xAttrFLT::HG0)[SplatIdx];
  const flt32 DC_B = Cloud->getAttrAddr(xAttrFLT::HB0)[SplatIdx];

  // Degree 0 only: color = 0.5 + SH_Y00 * DC
  if(Degree == 0 || Count == 0)
  {
    constexpr flt32 Y00 = 0.28209479177387814f; // 1 / (2*sqrt(pi))
    return { 0.5f + Y00*DC_R, 0.5f + Y00*DC_G, 0.5f + Y00*DC_B };
  }

  // Higher degrees: build coefficient arrays and use xSphHarmEvaluator
  // xSphHarmEvaluator expects 16 coefficients per channel (degree 0..3 = up to 16 SH basis functions)
  std::array<flt32, 16> ShR{}, ShG{}, ShB{};

  // DC (index 0)
  ShR[0] = DC_R;
  ShG[0] = DC_G;
  ShB[0] = DC_B;

  // AC coefficients (indices 1..15), stored in xAttrFLT as HR1..HR15 etc.
  // xAttrFLT enum layout: HR0,HG0,HB0, HR1,HG1,HB1, ... HR15,HG15,HB15
  // Count = number of AC coefficients per channel (e.g. degree 1 -> 3, degree 2 -> 8, degree 3 -> 15)
  const uint32 MaxAC = std::min(Count, 15u);
  for(uint32 k = 0; k < MaxAC; k++)
  {
    // xAttrFLT::HR1 = xAttrFLT::HR0 + 3*(k+1) -- step by 3 (R,G,B interleaved in enum)
    const auto AttrR = static_cast<xAttrFLT>(static_cast<int32>(xAttrFLT::HR0) + 3*(int32)(k+1));
    const auto AttrG = static_cast<xAttrFLT>(static_cast<int32>(xAttrFLT::HG0) + 3*(int32)(k+1));
    const auto AttrB = static_cast<xAttrFLT>(static_cast<int32>(xAttrFLT::HB0) + 3*(int32)(k+1));
    ShR[k+1] = Cloud->getAttrAddr(AttrR)[SplatIdx];
    ShG[k+1] = Cloud->getAttrAddr(AttrG)[SplatIdx];
    ShB[k+1] = Cloud->getAttrAddr(AttrB)[SplatIdx];
  }

  xSphHarmEvaluator Evaluator(ShR.data(), ShG.data(), ShB.data());
  flt32V3 Color = Evaluator.xEvaluateAtDirection(ViewDir.getX(), ViewDir.getY(), ViewDir.getZ());
  // Add 0.5 DC offset (same convention as 3DGS training and mpeg-gsc-metrics computeRadiance)
  return { std::max(0.0f, Color.getX() + 0.5f), std::max(0.0f, Color.getY() + 0.5f), std::max(0.0f, Color.getZ() + 0.5f) };
}

//=============================================================================================================================================================================

template void xProjectorSTD::ProjectRng<xProjectedCloudP>(xProjectedCloudP*, uint8*, const tGaussC*, const tCamPar&, int32V2, uint32, uint32);
template void xProjectorSTD::ProjectRng<xProjectedCloudI>(xProjectedCloudI*, uint8*, const tGaussC*, const tCamPar&, int32V2, uint32, uint32);

//=============================================================================================================================================================================

} //end of namespace PMBB::GSR
