/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefGSR.h"
#include "xCameraParams.h"
#include "xGaussianCloud.h"
#include "GspUi.h"
#include <vector>
#include <array>
#include <string>

namespace PMBB_NAMESPACE {

    //===============================================================================================================================================================================================================

    //the camera of GS-Player: the input views (the cameras of the camera parameters file), the poses of a posetrace and the free camera (flying with the keyboard and the mouse)
    //all cameras are kept in their own resolution; build() makes the camera for the picture size of the window (the vertical size scales the intrinsics: square pixels, the picture of a window of
    //another proportions shows more or less of the scene at the sides)
    class xGspCamera
    {
    public:
        using tCam = GSR::tCamPar;

    protected:
        std::vector<tCam> m_Views;
        int32  m_Mode     = gsp::ModeFree;
        int32  m_ViewIdx  = 0;
        //free camera: the pose in the frame of the cameras: [R|T] with T the position and R the world -> camera rotation (calculateLocalExtrinsics = [R | -R T]), so the rows of R are the axes of the camera in the world:
        //row 0 = right (the x of the picture), row 1 = down (y), row 2 = forward (z, the depth)
        flt64  m_R[3][3]  = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        flt64  m_T[3]     = { 0, 0, 0 };
        flt64  m_Up[3]    = { 0, 0, 1 };  //the vertical of the scene for the mouse look (against the down axis of the view the flight started from)
        flt64  m_BaseSpeed= 1.0;          //units per second at the speed 1
        tCam   m_RefCam;                  //intrinsics of the free camera (the view the flight started from)
        bool   m_Changed  = true;

        static void xNormalize(flt64* V);
        static void xCross(const flt64* A, const flt64* B, flt64* C);
        static void xRotateAxis(flt64 R[3][3], const flt64* Axis, flt64 AngleRad);           //R = Rot(Axis, Angle) * R (the axis in the frame of the world)
        void        xOrthonormalize();
        void        xLoadPose(const tCam& Cam);

    public:
        void  setViews(const std::vector<tCam>& Views);
        int32 getNumViews() const { return (int32)m_Views.size(); }
        int32 getMode    () const { return m_Mode; }
        int32 getViewIdx () const { return m_ViewIdx; }
        void  setMode    (int32 Mode);
        void  setViewIdx (int32 ViewIdx);
        void  nextView   (int32 Delta);
        void  resetFree  ();                                 //back to the pose of the current view
        void  estimateSpeed(const FVGS::xGaussianObject* Objs, int32 N); //the speed of the flight from the depth of the scene: 0.3 of the median depth of the splats seen from the first view per second
        bool  update     (const gsp::Input& In);             //the free flight, true if the pose changed
        bool  takeChanged() { const bool C = m_Changed; m_Changed = false; return C; }
        void  setChanged () { m_Changed = true; }

        //the camera to render in the current mode (PoseIdx: the pose of the trace), for the picture W x H
        tCam         build   (int32 W, int32 H, int32 PoseIdx) const;
        static tCam  xRescale(const tCam& Native, int32 W, int32 H);

        //pose as X, Y, Z, Yaw, Pitch, Roll in degrees (the convention of the posetrace files: R = Rz(Yaw) * Ry(Pitch) * Rx(Roll))
        std::array<flt64, 6> getPoseYPR() const;
        std::string          formatInfo (int32 PoseIdx) const;
    };

    //===============================================================================================================================================================================================================

} //end of namespace PMBB_NAMESPACE