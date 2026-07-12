/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#include "xGspCamera.h"
#include <cmath>
#include <algorithm>
#include <fmt/format.h>

namespace PMBB_NAMESPACE {

    //===============================================================================================================================================================================================================

    static constexpr flt64 c_Pi = 3.14159265358979323846;

    void xGspCamera::xNormalize(flt64* V)
    {
        const flt64 L = std::sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
        if(L > 1e-12) { V[0] /= L; V[1] /= L; V[2] /= L; }
    }
    void xGspCamera::xCross(const flt64* A, const flt64* B, flt64* C)
    {
        C[0] = A[1] * B[2] - A[2] * B[1]; C[1] = A[2] * B[0] - A[0] * B[2]; C[2] = A[0] * B[1] - A[1] * B[0];
    }
    //rotation of the camera (the rows of R, its axes in the world) about an axis given in the world: v' = v cos + (a x v) sin + a (a.v) (1 - cos)
    void xGspCamera::xRotateAxis(flt64 R[3][3], const flt64* Axis, flt64 Ang)
    {
        const flt64 C = std::cos(Ang), S = std::sin(Ang);
        for(int32 r = 0; r < 3; r++)
        {
            const flt64 V[3] = { R[r][0], R[r][1], R[r][2] }; flt64 AxV[3]; xCross(Axis, V, AxV);
            const flt64 Dot = Axis[0] * V[0] + Axis[1] * V[1] + Axis[2] * V[2];
            for(int32 k = 0; k < 3; k++) { R[r][k] = V[k] * C + AxV[k] * S + Axis[k] * Dot * (1.0 - C); }
        }
    }
    void xGspCamera::xOrthonormalize()
    {
        flt64 F[3] = { m_R[2][0], m_R[2][1], m_R[2][2] }, D[3] = { m_R[1][0], m_R[1][1], m_R[1][2] }, Rt[3];
        xNormalize(F);
        xCross(D, F, Rt); xNormalize(Rt);   //right = down x forward (x y z are right-handed: right, down, forward)
        xCross(F, Rt, D); xNormalize(D);    //down = forward x right
        for(int32 k = 0; k < 3; k++) { m_R[0][k] = Rt[k]; m_R[1][k] = D[k]; m_R[2][k] = F[k]; }
    }
    void xGspCamera::xLoadPose(const tCam& Cam)
    {
        const auto& E = Cam.getExtrinsics();
        for(int32 r = 0; r < 3; r++) { for(int32 c = 0; c < 3; c++) { m_R[r][c] = (flt64)E.at(r, c); } m_T[r] = (flt64)E.at(r, 3); }
        m_Up[0] = -m_R[1][0]; m_Up[1] = -m_R[1][1]; m_Up[2] = -m_R[1][2]; //the vertical of the scene: against the down axis of the camera
        m_RefCam = Cam;
        m_Changed = true;
    }

    //===============================================================================================================================================================================================================

    void xGspCamera::setViews(const std::vector<tCam>& Views)
    {
        m_Views = Views; m_ViewIdx = 0;
        if(m_Views.empty()) { return; }
        //the speed of the flight: a quarter of the spread of the cameras per second (the units of the scene are unknown)
        flt64 C[3] = { 0, 0, 0 };
        for(const tCam& V : m_Views) { const auto& E = V.getExtrinsics(); for(int32 r = 0; r < 3; r++) { C[r] += (flt64)E.at(r, 3); } }
        for(int32 r = 0; r < 3; r++) { C[r] /= (flt64)m_Views.size(); }
        flt64 S2 = 0;
        for(const tCam& V : m_Views) { const auto& E = V.getExtrinsics(); for(int32 r = 0; r < 3; r++) { const flt64 D = (flt64)E.at(r, 3) - C[r]; S2 += D * D; } }
        const flt64 Spread = std::sqrt(S2 / (flt64)m_Views.size());
        m_BaseSpeed = Spread > 1e-6 ? 0.25 * Spread : 1.0;
        xLoadPose(m_Views[0]);
    }
    void xGspCamera::setMode(int32 Mode)
    {
        if(Mode == gsp::ModeViews && m_Views.empty()) { return; }
        m_Mode = Mode; m_Changed = true; //the free pose stays as it was; resetFree() goes back to the current view
    }
    void xGspCamera::setViewIdx(int32 ViewIdx)
    {
        if(m_Views.empty()) { return; }
        const int32 N = (int32)m_Views.size();
        m_ViewIdx = ((ViewIdx % N) + N) % N; m_Changed = true;
    }
    void xGspCamera::nextView(int32 Delta) { setViewIdx(m_ViewIdx + Delta); }
    void xGspCamera::resetFree()
    {
        if(m_Views.empty()) { return; }
        xLoadPose(m_Views[(size_t)m_ViewIdx]);
    }
    void xGspCamera::estimateSpeed(const FVGS::xGaussianObject* Objs, int32 N)
    {
        if(m_Views.empty() || N <= 0) { return; }
        const auto& E = m_Views[0].getExtrinsics();
        const flt64 F[3] = { (flt64)E.at(2, 0), (flt64)E.at(2, 1), (flt64)E.at(2, 2) }, T[3] = { (flt64)E.at(0, 3), (flt64)E.at(1, 3), (flt64)E.at(2, 3) }; //forward = the row 2 of R
        std::vector<flt64> Depths; const int32 Stride = std::max(1, N / 20000);
        for(int32 i = 0; i < N; i += Stride)
        {
            const FVGS::xGaussianObject& O = Objs[i];
            if(!(O.Opacity > 0.0f)) { continue; } //opacity above 0.5 (before the sigmoid, the splats that are seen)
            const flt64 D = ((flt64)O.PositionXYX[0] - T[0]) * F[0] + ((flt64)O.PositionXYX[1] - T[1]) * F[1] + ((flt64)O.PositionXYX[2] - T[2]) * F[2];
            if(D > 1e-6) { Depths.push_back(D); }
        }
        if(Depths.size() < 100) { return; }
        std::nth_element(Depths.begin(), Depths.begin() + (std::ptrdiff_t)(Depths.size() / 2), Depths.end());
        m_BaseSpeed = 0.3 * Depths[Depths.size() / 2];
        m_Changed = true;
    }
    bool xGspCamera::update(const gsp::Input& In)
    {
        if(m_Mode != gsp::ModeFree) { return false; }
        bool Moved = false;
        //look: the yaw about the vertical of the scene, the pitch about the left axis of the camera (not beyond the vertical)
        const flt64 Sens = 0.12 * c_Pi / 180.0;
        if(In.Look[0] != 0.0f || In.Look[1] != 0.0f)
        {
            flt64 Up[3] = { m_Up[0], m_Up[1], m_Up[2] };
            xRotateAxis(m_R, Up, -(flt64)In.Look[0] * Sens);                                  //the mouse to the right: turn to the right (a positive angle about the up axis turns to the left)
            const flt64 Fwd[3] = { m_R[2][0], m_R[2][1], m_R[2][2] };
            const flt64 CurElev = std::asin(std::max(-1.0, std::min(1.0, Fwd[0] * m_Up[0] + Fwd[1] * m_Up[1] + Fwd[2] * m_Up[2]))); //the elevation of the forward axis above the horizon
            const flt64 Lim = 89.0 * c_Pi / 180.0;
            const flt64 NewElev = std::max(-Lim, std::min(Lim, CurElev - (flt64)In.Look[1] * Sens)); //the mouse down: the view goes down
            flt64 Right[3] = { m_R[0][0], m_R[0][1], m_R[0][2] };
            xRotateAxis(m_R, Right, NewElev - CurElev);                                       //a positive angle about the right axis raises the forward axis
            xOrthonormalize(); Moved = true;
        }
        if(In.Roll != 0.0f)
        {
            flt64 Fwd[3] = { m_R[2][0], m_R[2][1], m_R[2][2] };
            xRotateAxis(m_R, Fwd, (flt64)In.Roll * (60.0 * c_Pi / 180.0) * (flt64)In.Dt);                   //X: the horizon turns clockwise
            xOrthonormalize(); Moved = true;
        }
        //move along the axes of the camera: forward, right, up (the axes are the rows of R: right, down, forward)
        if(In.Move[0] != 0.0f || In.Move[1] != 0.0f || In.Move[2] != 0.0f)
        {
            const flt64 Step = m_BaseSpeed * (flt64)In.Speed * (flt64)In.Dt;
            for(int32 k = 0; k < 3; k++) { m_T[k] += Step * ((flt64)In.Move[0] * m_R[2][k] + (flt64)In.Move[1] * m_R[0][k] - (flt64)In.Move[2] * m_R[1][k]); }
            Moved = true;
        }
        if(Moved) { m_Changed = true; }
        return Moved;
    }

    //===============================================================================================================================================================================================================

    xGspCamera::tCam xGspCamera::xRescale(const tCam& Native, int32 W, int32 H)
    {
        tCam C = Native;
        const int32V2 Res = Native.getResolution();
        const flt64 S = (flt64)H / (flt64)Res.getY();
        auto& K = C.getIntrinsics();
        const flt64 Cx = (flt64)K[0][2], Cy = (flt64)K[1][2];
        K[0][0] = (flt64)K[0][0] * S; K[1][1] = (flt64)K[1][1] * S;
        K[0][2] = (Cx - (flt64)Res.getX() * 0.5) * S + (flt64)W * 0.5;
        K[1][2] = Cy * S;
        C.setResolution(int32V2(W, H));
        C.calculateDerrived();
        return C;
    }
    xGspCamera::tCam xGspCamera::build(int32 W, int32 H, int32 PoseIdx) const
    {
        if(m_Mode == gsp::ModeViews && !m_Views.empty()) { return xRescale(m_Views[(size_t)m_ViewIdx], W, H); }
        //free: the intrinsics of the reference view, the pose of the flight
        tCam C = m_RefCam;
        auto& E = C.getExtrinsics();
        for(int32 r = 0; r < 3; r++) { for(int32 c = 0; c < 3; c++) { E[r][c] = m_R[r][c]; } E[r][3] = m_T[r]; }
        C.calculateDerrived();
        return xRescale(C, W, H);
    }
    std::array<flt64, 6> xGspCamera::getPoseYPR() const
    {
        const flt64 D = 180.0 / c_Pi;
        const flt64 Yaw = std::atan2(m_R[1][0], m_R[0][0]), Pitch = std::asin(std::max(-1.0, std::min(1.0, -m_R[2][0]))), Roll = std::atan2(m_R[2][1], m_R[2][2]);
        return { m_T[0], m_T[1], m_T[2], Yaw * D, Pitch * D, Roll * D };
    }
    std::string xGspCamera::formatInfo(int32 PoseIdx) const
    {
        if(m_Mode == gsp::ModeViews) { return fmt::format("input view {}/{}   (Left / Right)", m_ViewIdx + 1, (int32)m_Views.size()); }
        const auto P = getPoseYPR();
        return fmt::format("pos {:.3f} {:.3f} {:.3f}   yaw {:.1f} pitch {:.1f} roll {:.1f}", P[0], P[1], P[2], P[3], P[4], P[5]);
    }

    //===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE