/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
#include "GspUi.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // SDL_SetMainReady (SDL_MAIN_HANDLED: the application has its own main)
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace gsp {

    //===============================================================================================================================================================================================================

    namespace {

        struct Ui
        {
            SDL_Window*   Window   = nullptr;
            SDL_Renderer* Renderer = nullptr;
            SDL_Texture*  Texture  = nullptr;
            int           TexW = 0, TexH = 0;
            std::vector<uint8_t> Bgra;          // the picture converted for the texture
            std::vector<uint8_t> LastRgb;       // the last picture as given by the host (for the screenshot)
            int           LastW = 0, LastH = 0;
            bool          ShowHud = true, ShowHelp = false;
            float         Speed = 1.0f;         // the wheel
            float         Scale = 1.0f;
            bool          MouseLook = false;
            float         LookAcc[2] = { 0, 0 };
            Hud           H;
            char          Msg[160] = {};        // a message of the window itself (needs two clouds ...)
            Uint32        MsgUntil = 0;
        };

        void xUploadFrame(Ui& U, const Frame& F)
        {
            if(F.W != U.TexW || F.H != U.TexH || U.Texture == nullptr)
            {
                if(U.Texture) { SDL_DestroyTexture(U.Texture); U.Texture = nullptr; }
                U.Texture = SDL_CreateTexture(U.Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, F.W, F.H);
                U.TexW = F.W; U.TexH = F.H;
                if(U.Texture == nullptr) { std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError()); return; }
                SDL_SetTextureScaleMode(U.Texture, SDL_SCALEMODE_LINEAR);
            }
            const size_t NumPels = (size_t)F.W * F.H;
            U.Bgra.resize(NumPels * 4);
            const uint8_t* S = F.Rgb; uint8_t* D = U.Bgra.data();
            for(size_t i = 0; i < NumPels; i++) { D[0] = S[2]; D[1] = S[1]; D[2] = S[0]; D[3] = 255; S += 3; D += 4; } //ARGB8888 is B, G, R, A in memory (little endian)
            SDL_UpdateTexture(U.Texture, nullptr, U.Bgra.data(), F.W * 4);
            U.LastRgb.assign(F.Rgb, F.Rgb + NumPels * 3); U.LastW = F.W; U.LastH = F.H;
        }

        const char* xModeName(int Mode) { return Mode == ModeFree ? "FREE" : "VIEWS"; }

        void xDrawHud(Ui& U, Host& Hst, int WinW, int WinH)
        {
            Hud& H = U.H;
            const ImGuiWindowFlags Ov = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;
            const float Pad = 6.0f * U.Scale;

            //status in the top-left corner
            ImGui::SetNextWindowPos(ImVec2(Pad, Pad)); ImGui::SetNextWindowBgAlpha(0.45f);
            if(ImGui::Begin("##status", nullptr, Ov | ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted(H.Line1);
                ImGui::TextUnformatted(H.Line2);
                if(H.Message[0]) { ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", H.Message); }
                if(U.ShowHelp)
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("A D  left / right     W S  up / down     E Q  forward / back (along the camera)     Z X  roll     right mouse  look");
                    ImGui::TextUnformatted("wheel  speed      Shift / Ctrl  faster / slower     Home  reset camera");
                    ImGui::TextUnformatted("F1 free   F2 input views (Left / Right)");
                    ImGui::TextUnformatted("Space  play / pause     , .  step     + -  playback fps     O  loop     H  SH degree 3/2/1/0     G  splats / spheres / points");
                    ImGui::TextUnformatted("R  record the path     F12  screenshot     M  metric of the plot     K  difference window (+ - gain)     L  split view window (V: left-right / up-down)");
                    ImGui::TextUnformatted("Tab  overlay on / off     F5  help     Esc  quit");
                }
            }
            ImGui::End();

            //metrics at the right edge
            float PanelTop = (float)WinH;
            if(H.NumRows > 0)
            {
                ImGui::SetNextWindowBgAlpha(0.55f);
                ImGui::SetNextWindowPos(ImVec2((float)WinW - Pad, Pad), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
                if(ImGui::Begin("##metrics", nullptr, Ov | ImGuiWindowFlags_AlwaysAutoResize))
                {
                    if(ImGui::BeginTable("##mt", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
                    {
                        ImGui::TableSetupColumn("metric"); ImGui::TableSetupColumn("now"); ImGui::TableSetupColumn("average"); ImGui::TableSetupColumn("components");
                        ImGui::TableHeadersRow();
                        for(int r = 0; r < H.NumRows; r++)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(H.Rows[r].Name);
                            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(H.Rows[r].Value);
                            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(H.Rows[r].Avg);
                            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(H.Rows[r].Comp);
                        }
                        ImGui::EndTable();
                    }
                    if(H.PlotN > 1) { ImGui::PlotLines(H.PlotName, H.Plot, H.PlotN, 0, nullptr, H.PlotMin, H.PlotMax, ImVec2(300.0f * U.Scale, 48.0f * U.Scale)); }
                }
                ImGui::End();
            }

            //playback bar at the bottom
            const float BarH = 34.0f * U.Scale;
            PanelTop = (float)WinH - BarH;
            ImGui::SetNextWindowPos(ImVec2(0.0f, PanelTop)); ImGui::SetNextWindowSize(ImVec2((float)WinW, BarH)); ImGui::SetNextWindowBgAlpha(0.55f);
            if(ImGui::Begin("##bar", nullptr, Ov))
            {
                if(ImGui::Button(H.Playing ? "Pause" : "Play ")) { Hst.onCommand(CmdTogglePlay, 0); }
                ImGui::SameLine();
                if(ImGui::Button(H.Mode == ModeFree ? "free" : "views")) { Hst.onCommand(CmdMode, (H.Mode + 1) % 2); }
                ImGui::SameLine();
                if(H.NumFrames > 1)
                {
                    int Cur = H.CurFrame;
                    ImGui::SetNextItemWidth((float)WinW - 330.0f * U.Scale);
                    if(ImGui::SliderInt("##seek", &Cur, 0, H.NumFrames - 1, "frame %d")) { Hst.onCommand(CmdSeek, Cur); }
                    ImGui::SameLine(); ImGui::Text("/ %d", H.NumFrames);
                }
                else { ImGui::TextUnformatted("single cloud"); }
                ImGui::SameLine(); if(H.Playing) { ImGui::Text("%.1f ms  %.0f/s", H.RenderMs, H.RenderFps); } else { ImGui::Text("%.1f ms", H.RenderMs); }
            }
            ImGui::End();
        }

        //the windows of the comparison of the two pictures of the side by side (the picture of the window is A | B): the difference and the split view; own window, own renderer and textures
        struct SubWin
        {
            SDL_Window*   Win = nullptr;
            SDL_Renderer* Ren = nullptr;
            SDL_Texture*  Tex[2] = { nullptr, nullptr };
            SDL_WindowID  Id = 0;
            int           TexW = 0, TexH = 0;
            bool          Dirty = true;
            bool          isOpen() const { return Win != nullptr; }
        };

        struct Sub
        {
            SubWin Diff, Cmp;
            float  Gain = 1.0f;          // the amplification of the difference
            bool   Vertical = false;     // the split of the compare window: false = left | right of the cursor, true = above | below
            float  Split[2] = { 0.5f, 0.5f }; // the position of the cursor in the compare window, 0..1 (x, y)
            std::vector<uint8_t> Buf;    // BGRA of the picture sent to a texture
        };

        void xOpenSub(SubWin& S, const char* Title, int W, int H)
        {
            S.Win = SDL_CreateWindow(Title, W, H, SDL_WINDOW_RESIZABLE);
            if(S.Win == nullptr) { std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return; }
            S.Ren = SDL_CreateRenderer(S.Win, nullptr);
            if(S.Ren == nullptr) { S.Ren = SDL_CreateRenderer(S.Win, "software"); }
            if(S.Ren == nullptr) { std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError()); SDL_DestroyWindow(S.Win); S.Win = nullptr; return; }
            SDL_SetRenderVSync(S.Ren, 0); //the main window waits for the display, the others are drawn when something changes
            S.Id = SDL_GetWindowID(S.Win); S.TexW = S.TexH = 0; S.Dirty = true;
        }
        void xCloseSub(SubWin& S)
        {
            for(SDL_Texture*& T : S.Tex) { if(T) { SDL_DestroyTexture(T); T = nullptr; } }
            if(S.Ren) { SDL_DestroyRenderer(S.Ren); S.Ren = nullptr; }
            if(S.Win) { SDL_DestroyWindow(S.Win); S.Win = nullptr; }
            S.Id = 0; S.TexW = S.TexH = 0;
        }
        void xEnsureTex(SubWin& S, int Idx, int W, int H)
        {
            if(S.Tex[Idx] != nullptr && (S.TexW != W || S.TexH != H)) { SDL_DestroyTexture(S.Tex[Idx]); S.Tex[Idx] = nullptr; }
            if(S.Tex[Idx] == nullptr)
            {
                S.Tex[Idx] = SDL_CreateTexture(S.Ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
                if(S.Tex[Idx]) { SDL_SetTextureScaleMode(S.Tex[Idx], SDL_SCALEMODE_LINEAR); }
            }
        }
        //the textures of the sub windows from the last picture of the side by side (RGB, the left half A, the right half B)
        void xUpdateSubs(Ui& U, Sub& Sb)
        {
            if(U.LastW < 2 || U.LastH < 1) { return; }
            const int PW = U.LastW / 2, PH = U.LastH; const size_t Stride = (size_t)U.LastW * 3;
            Sb.Buf.resize((size_t)PW * PH * 4);
            if(Sb.Diff.isOpen())
            {
                //the difference of the pictures with the middle gray for no difference, amplified and saturated (not wrapped)
                xEnsureTex(Sb.Diff, 0, PW, PH); Sb.Diff.TexW = PW; Sb.Diff.TexH = PH;
                for(int y = 0; y < PH; y++)
                {
                    const uint8_t* A = U.LastRgb.data() + (size_t)y * Stride; const uint8_t* B = A + (size_t)PW * 3; uint8_t* D = Sb.Buf.data() + (size_t)y * PW * 4;
                    for(int x = 0; x < PW; x++)
                    {
                        for(int c = 0; c < 3; c++)
                        {
                            const float V = 128.0f + Sb.Gain * (float)((int)A[3 * x + c] - (int)B[3 * x + c]);
                            D[4 * x + (2 - c)] = (uint8_t)(V < 0.0f ? 0.0f : V > 255.0f ? 255.0f : V + 0.5f); //RGB -> B, G, R of ARGB8888
                        }
                        D[4 * x + 3] = 255;
                    }
                }
                if(Sb.Diff.Tex[0]) { SDL_UpdateTexture(Sb.Diff.Tex[0], nullptr, Sb.Buf.data(), PW * 4); }
                Sb.Diff.Dirty = true;
            }
            if(Sb.Cmp.isOpen())
            {
                for(int k = 0; k < 2; k++)
                {
                    xEnsureTex(Sb.Cmp, k, PW, PH);
                    for(int y = 0; y < PH; y++)
                    {
                        const uint8_t* S = U.LastRgb.data() + (size_t)y * Stride + (size_t)k * PW * 3; uint8_t* D = Sb.Buf.data() + (size_t)y * PW * 4;
                        for(int x = 0; x < PW; x++) { D[4 * x] = S[3 * x + 2]; D[4 * x + 1] = S[3 * x + 1]; D[4 * x + 2] = S[3 * x]; D[4 * x + 3] = 255; }
                    }
                    if(Sb.Cmp.Tex[k]) { SDL_UpdateTexture(Sb.Cmp.Tex[k], nullptr, Sb.Buf.data(), PW * 4); }
                }
                Sb.Cmp.TexW = PW; Sb.Cmp.TexH = PH; Sb.Cmp.Dirty = true;
            }
        }
        void xDrawLabel(SDL_Renderer* R, float X, float Y, const char* Text)
        {
            SDL_SetRenderScale(R, 2.0f, 2.0f);
            SDL_SetRenderDrawColor(R, 0, 0, 0, 255); SDL_RenderDebugText(R, X / 2.0f + 1.0f, Y / 2.0f + 1.0f, Text);
            SDL_SetRenderDrawColor(R, 255, 255, 255, 255); SDL_RenderDebugText(R, X / 2.0f, Y / 2.0f, Text);
            SDL_SetRenderScale(R, 1.0f, 1.0f);
        }
        void xDrawSubs(Sub& Sb)
        {
            if(Sb.Diff.isOpen() && Sb.Diff.Dirty)
            {
                SDL_Renderer* R = Sb.Diff.Ren; SDL_SetRenderDrawColor(R, 128, 128, 128, 255); SDL_RenderClear(R);
                if(Sb.Diff.Tex[0]) { SDL_RenderTexture(R, Sb.Diff.Tex[0], nullptr, nullptr); }
                char Text[64]; std::snprintf(Text, sizeof(Text), "A - B + 128  x%g   (+ - gain)", (double)Sb.Gain); xDrawLabel(R, 6.0f, 6.0f, Text);
                SDL_RenderPresent(R); Sb.Diff.Dirty = false;
            }
            if(Sb.Cmp.isOpen() && Sb.Cmp.Dirty)
            {
                SDL_Renderer* R = Sb.Cmp.Ren; SDL_SetRenderDrawColor(R, 0, 0, 0, 255); SDL_RenderClear(R);
                int WW = 1, WH = 1; SDL_GetCurrentRenderOutputSize(R, &WW, &WH);
                const float PW = (float)std::max(1, Sb.Cmp.TexW), PH = (float)std::max(1, Sb.Cmp.TexH);
                if(Sb.Cmp.Tex[0] && Sb.Cmp.Tex[1])
                {
                    SDL_RenderTexture(R, Sb.Cmp.Tex[0], nullptr, nullptr); //A everywhere, B over it from the cursor on
                    if(!Sb.Vertical)
                    {
                        const float Sx = Sb.Split[0] * PW, Dx = Sb.Split[0] * (float)WW; const SDL_FRect Src = { Sx, 0.0f, PW - Sx, PH }, Dst = { Dx, 0.0f, (float)WW - Dx, (float)WH };
                        SDL_RenderTexture(R, Sb.Cmp.Tex[1], &Src, &Dst);
                        SDL_SetRenderDrawColor(R, 255, 255, 255, 255); SDL_RenderLine(R, Dx, 0.0f, Dx, (float)WH);
                        xDrawLabel(R, 6.0f, 6.0f, "A"); xDrawLabel(R, (float)WW - 22.0f, 6.0f, "B");
                    }
                    else
                    {
                        const float Sy = Sb.Split[1] * PH, Dy = Sb.Split[1] * (float)WH; const SDL_FRect Src = { 0.0f, Sy, PW, PH - Sy }, Dst = { 0.0f, Dy, (float)WW, (float)WH - Dy };
                        SDL_RenderTexture(R, Sb.Cmp.Tex[1], &Src, &Dst);
                        SDL_SetRenderDrawColor(R, 255, 255, 255, 255); SDL_RenderLine(R, 0.0f, Dy, (float)WW, Dy);
                        xDrawLabel(R, 6.0f, 6.0f, "A"); xDrawLabel(R, 6.0f, (float)WH - 22.0f, "B");
                    }
                }
                xDrawLabel(R, (float)WW * 0.5f - 60.0f, (float)WH - 22.0f - (Sb.Vertical ? 30.0f : 0.0f), "V: left-right / up-down");
                SDL_RenderPresent(R); Sb.Cmp.Dirty = false;
            }
        }

    } //end of anonymous namespace

      //===============================================================================================================================================================================================================

    int runUi(Host& Hst, const UiOptions& O)
    {
        SDL_SetMainReady();
        if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError()); return 1; }

        Ui U; U.Scale = O.Scale; Sub Sb;
        U.Window = SDL_CreateWindow(O.Title, O.WinW, O.WinH, SDL_WINDOW_RESIZABLE);
        if(U.Window == nullptr) { std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
        U.Renderer = SDL_CreateRenderer(U.Window, nullptr);
        if(U.Renderer == nullptr) { U.Renderer = SDL_CreateRenderer(U.Window, "software"); }
        if(U.Renderer != nullptr) { SDL_SetRenderVSync(U.Renderer, O.VSync ? 1 : 0); }
        if(U.Renderer == nullptr) { std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError()); SDL_DestroyWindow(U.Window); SDL_Quit(); return 1; }
        const SDL_WindowID MainId = SDL_GetWindowID(U.Window);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& IO = ImGui::GetIO();
        IO.IniFilename = nullptr;
        IO.FontGlobalScale = O.Scale;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL3_InitForSDLRenderer(U.Window, U.Renderer);
        ImGui_ImplSDLRenderer3_Init(U.Renderer);

        int WinW = O.WinW, WinH = O.WinH; SDL_GetWindowSize(U.Window, &WinW, &WinH);
        Hst.onResize(WinW, WinH);

        auto Say = [&](const char* Text) { std::snprintf(U.Msg, sizeof(U.Msg), "%s", Text); U.MsgUntil = SDL_GetTicks() + 2500; };
        auto ToggleSub = [&](SubWin& S, const char* Title)
            {
                if(S.isOpen()) { xCloseSub(S); return; }
                if(!U.H.Sbs || U.LastW < 2) { Say("the difference and the split view need two clouds (-i1)"); return; }
                xOpenSub(S, Title, U.LastW / 2, U.LastH); xUpdateSubs(U, Sb);
            };

        bool Quit = false;
        auto OnKey = [&](SDL_Keycode Key, bool InDiff, bool InCmp)
            {
                if(Key == SDLK_ESCAPE) { if(InDiff) { xCloseSub(Sb.Diff); } else if(InCmp) { xCloseSub(Sb.Cmp); } else { Quit = true; } return; }
                if(InCmp && Key == SDLK_V) { Sb.Vertical = !Sb.Vertical; Sb.Cmp.Dirty = true; return; }
                if(InDiff && (Key == SDLK_PLUS || Key == SDLK_EQUALS || Key == SDLK_KP_PLUS)) { Sb.Gain = std::min(64.0f, Sb.Gain * 2.0f); xUpdateSubs(U, Sb); return; }
                if(InDiff && (Key == SDLK_MINUS || Key == SDLK_KP_MINUS)) { Sb.Gain = std::max(0.25f, Sb.Gain * 0.5f); xUpdateSubs(U, Sb); return; }
                switch(Key)
                {
                case SDLK_ESCAPE: Quit = true; break;
                case SDLK_SPACE:  Hst.onCommand(CmdTogglePlay, 0); break;
                case SDLK_PERIOD: Hst.onCommand(CmdStepFwd, 0); break;
                case SDLK_COMMA:  Hst.onCommand(CmdStepBack, 0); break;
                case SDLK_RIGHT:  Hst.onCommand(CmdNextView, 0); break;
                case SDLK_LEFT:   Hst.onCommand(CmdPrevView, 0); break;
                case SDLK_F1:     Hst.onCommand(CmdMode, ModeFree); break;
                case SDLK_F2:     Hst.onCommand(CmdMode, ModeViews); break;
                case SDLK_HOME:   Hst.onCommand(CmdResetCamera, 0); break;
                case SDLK_R:      Hst.onCommand(CmdToggleRecord, 0); break;
                case SDLK_F12:    Hst.onCommand(CmdScreenshot, 0); break;
                case SDLK_M:      Hst.onCommand(CmdCycleMetric, 0); break;
                case SDLK_O:      Hst.onCommand(CmdLoop, 0); break;
                case SDLK_K:      ToggleSub(Sb.Diff, "GS-Player: difference A - B"); break;
                case SDLK_L:      ToggleSub(Sb.Cmp, "GS-Player: A | B"); break;
                case SDLK_PLUS: case SDLK_EQUALS: case SDLK_KP_PLUS: Hst.onCommand(CmdPlaySpeed, +1); break;
                case SDLK_MINUS: case SDLK_KP_MINUS:                 Hst.onCommand(CmdPlaySpeed, -1); break;
                case SDLK_TAB:      U.ShowHud = !U.ShowHud; break;
                case SDLK_F5:     U.ShowHelp = !U.ShowHelp; break;
                default: break;
                }
            };
        int Frames = 0; Uint64 TPrev = SDL_GetPerformanceCounter(); const Uint64 T00 = TPrev; const double Freq = (double)SDL_GetPerformanceFrequency();
        struct TestKey { double Ms; SDL_Keycode Key; bool InDiff, InCmp; float Sx, Sy; bool IsSplit; };
        std::vector<TestKey> TestKeys; size_t TestPos = 0;
        if(O.TestKeys)
        {
            std::string Str = O.TestKeys; size_t B = 0;
            while(B < Str.size())
            {
                size_t E2 = Str.find(';', B); if(E2 == std::string::npos) { E2 = Str.size(); }
                std::string Item = Str.substr(B, E2 - B); B = E2 + 1;
                std::vector<std::string> P; size_t b = 0; while(true) { const size_t e = Item.find(':', b); P.push_back(Item.substr(b, e == std::string::npos ? e : e - b)); if(e == std::string::npos) { break; } b = e + 1; }
                if(P.size() < 2) { continue; }
                TestKey T = { std::atof(P[0].c_str()), 0, false, false, 0.5f, 0.5f, false };
                if(P[1] == "split" && P.size() >= 4) { T.IsSplit = true; T.Sx = (float)std::atof(P[2].c_str()); T.Sy = (float)std::atof(P[3].c_str()); }
                else
                {
                    size_t k = 1; if(P[1] == "d" || P[1] == "c") { T.InDiff = P[1] == "d"; T.InCmp = P[1] == "c"; k = 2; }
                    if(k < P.size() && !P[k].empty()) { T.Key = P[k] == "tab" ? SDLK_TAB : P[k] == "esc" ? SDLK_ESCAPE : (SDL_Keycode)(unsigned char)std::tolower((unsigned char)P[k][0]); }
                }
                TestKeys.push_back(T);
            }
        }

        while(!Quit)
        {
            {
                const double Ms = (double)(SDL_GetPerformanceCounter() - T00) / Freq * 1000.0;
                while(TestPos < TestKeys.size() && TestKeys[TestPos].Ms <= Ms)
                {
                    const TestKey& T = TestKeys[TestPos++];
                    if(T.IsSplit) { Sb.Split[0] = T.Sx; Sb.Split[1] = T.Sy; Sb.Cmp.Dirty = true; } else { OnKey(T.Key, T.InDiff && Sb.Diff.isOpen(), T.InCmp && Sb.Cmp.isOpen()); }
                }
            }
            SDL_Event E;
            while(SDL_PollEvent(&E))
            {
                //the events of the other windows are not for the overlay
                SDL_WindowID EvWin = 0;
                switch(E.type)
                {
                case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: EvWin = E.key.windowID; break;
                case SDL_EVENT_MOUSE_MOTION: EvWin = E.motion.windowID; break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: EvWin = E.button.windowID; break;
                case SDL_EVENT_MOUSE_WHEEL: EvWin = E.wheel.windowID; break;
                default: EvWin = (E.type >= SDL_EVENT_WINDOW_FIRST && E.type <= SDL_EVENT_WINDOW_LAST) ? E.window.windowID : 0; break;
                }
                const bool InMain = EvWin == 0 || EvWin == MainId;
                if(InMain) { ImGui_ImplSDL3_ProcessEvent(&E); }
                const bool KbdFree = !InMain || !ImGui::GetIO().WantCaptureKeyboard;
                const bool MouseFree = !ImGui::GetIO().WantCaptureMouse;
                const bool InDiff = Sb.Diff.isOpen() && EvWin == Sb.Diff.Id, InCmp = Sb.Cmp.isOpen() && EvWin == Sb.Cmp.Id;
                switch(E.type)
                {
                case SDL_EVENT_QUIT: Quit = true; break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: if(EvWin == MainId) { Quit = true; } else if(InDiff) { xCloseSub(Sb.Diff); } else if(InCmp) { xCloseSub(Sb.Cmp); } break;
                case SDL_EVENT_WINDOW_RESIZED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: case SDL_EVENT_WINDOW_EXPOSED:
                    if(EvWin == MainId) { SDL_GetWindowSize(U.Window, &WinW, &WinH); Hst.onResize(WinW, WinH); }
                    else if(InDiff) { Sb.Diff.Dirty = true; } else if(InCmp) { Sb.Cmp.Dirty = true; }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: if(InMain && E.button.button == SDL_BUTTON_RIGHT && MouseFree) { U.MouseLook = true; SDL_SetWindowRelativeMouseMode(U.Window, true); } break;
                case SDL_EVENT_MOUSE_BUTTON_UP:   if(E.button.button == SDL_BUTTON_RIGHT) { U.MouseLook = false; SDL_SetWindowRelativeMouseMode(U.Window, false); } break;
                case SDL_EVENT_MOUSE_MOTION:
                    if(InMain && U.MouseLook) { U.LookAcc[0] += E.motion.xrel; U.LookAcc[1] += E.motion.yrel; }
                    if(InCmp) { int WW = 1, WH = 1; SDL_GetWindowSize(Sb.Cmp.Win, &WW, &WH); Sb.Split[0] = std::min(1.0f, std::max(0.0f, E.motion.x / (float)std::max(1, WW))); Sb.Split[1] = std::min(1.0f, std::max(0.0f, E.motion.y / (float)std::max(1, WH))); Sb.Cmp.Dirty = true; }
                    break;
                case SDL_EVENT_MOUSE_WHEEL: if(InMain && MouseFree) { U.Speed = std::min(100.0f, std::max(0.01f, U.Speed * (E.wheel.y > 0 ? 1.25f : E.wheel.y < 0 ? 0.8f : 1.0f))); } break;
                case SDL_EVENT_KEY_DOWN:
                    if(!E.key.repeat && KbdFree)
                    {
                        OnKey(E.key.key, InDiff, InCmp);
                    }
                    break;
                default: break;
                }
            }

            //continuous input
            const Uint64 TNow = SDL_GetPerformanceCounter(); const float Dt = (float)((double)(TNow - TPrev) / Freq); TPrev = TNow;
            Input In; In.Dt = Dt;
            if(!ImGui::GetIO().WantCaptureKeyboard)
            {
                const bool* K = SDL_GetKeyboardState(nullptr);
                //along the axes of the camera: A D sideways, W S up / down, E Q forward / back (into the picture / out of it)
                In.Move[0] = (K[SDL_SCANCODE_E] ? 1.0f : 0.0f) - (K[SDL_SCANCODE_Q] ? 1.0f : 0.0f);
                In.Move[1] = (K[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (K[SDL_SCANCODE_A] ? 1.0f : 0.0f);
                In.Move[2] = (K[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (K[SDL_SCANCODE_S] ? 1.0f : 0.0f);
                In.Roll    = (K[SDL_SCANCODE_X] ? 1.0f : 0.0f) - (K[SDL_SCANCODE_Z] ? 1.0f : 0.0f);
                In.Speed   = U.Speed * ((K[SDL_SCANCODE_LSHIFT] || K[SDL_SCANCODE_RSHIFT]) ? 4.0f : 1.0f) * ((K[SDL_SCANCODE_LCTRL] || K[SDL_SCANCODE_RCTRL]) ? 0.25f : 1.0f);
            }
            else { In.Speed = U.Speed; }
            In.Look[0] = U.LookAcc[0]; In.Look[1] = U.LookAcc[1]; U.LookAcc[0] = U.LookAcc[1] = 0.0f;
            Hst.onInput(In);

            //picture
            Frame F;
            if(Hst.pollFrame(F)) { xUploadFrame(U, F); Hst.releaseFrame(); if(Sb.Diff.isOpen() || Sb.Cmp.isOpen()) { xUpdateSubs(U, Sb); } }
            Hst.fillHud(U.H);
            if(!U.H.Sbs) { if(Sb.Diff.isOpen()) { xCloseSub(Sb.Diff); } if(Sb.Cmp.isOpen()) { xCloseSub(Sb.Cmp); } }
            if(SDL_GetTicks() < U.MsgUntil && U.H.Message[0] == 0) { std::snprintf(U.H.Message, sizeof(U.H.Message), "%s", U.Msg); }

            //draw
            SDL_SetRenderDrawColor(U.Renderer, 0, 0, 0, 255);
            SDL_RenderClear(U.Renderer);
            if(U.Texture) { SDL_RenderTexture(U.Renderer, U.Texture, nullptr, nullptr); }
            ImGui_ImplSDLRenderer3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
            if(U.ShowHud) { xDrawHud(U, Hst, WinW, WinH); }
            ImGui::Render();
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), U.Renderer);

            Frames++;
            const double ElapsedMs = (double)(SDL_GetPerformanceCounter() - T00) / Freq * 1000.0;
            if((O.TestFrames > 0 && Frames >= O.TestFrames) || (O.TestMs > 0 && ElapsedMs >= (double)O.TestMs)) //the picture of the window with the overlay
            {
                SDL_Surface* Sf = O.TestShot ? SDL_RenderReadPixels(U.Renderer, nullptr) : nullptr;
                SDL_Surface* Cv = Sf ? SDL_ConvertSurface(Sf, SDL_PIXELFORMAT_ARGB8888) : nullptr;
                if(Cv)
                {
                    const int OutW = Cv->w, OutH = Cv->h; std::vector<uint8_t> Rgb((size_t)OutW * OutH * 3);
                    for(int y = 0; y < OutH; y++)
                    {
                        const uint8_t* B = (const uint8_t*)Cv->pixels + (size_t)y * Cv->pitch; uint8_t* D = &Rgb[(size_t)y * OutW * 3];
                        for(int x = 0; x < OutW; x++) { D[3 * x] = B[4 * x + 2]; D[3 * x + 1] = B[4 * x + 1]; D[3 * x + 2] = B[4 * x]; }
                    }
                    Hst.writePng(O.TestShot, Rgb.data(), OutW, OutH);
                }
                if(Cv) { SDL_DestroySurface(Cv); } if(Sf) { SDL_DestroySurface(Sf); }
                //the sub windows of the test: their pictures next to the shot of the window (TestShot with the suffix _diff / _cmp)
                for(int k = 0; k < 2 && O.TestShot; k++)
                {
                    SubWin& S = k == 0 ? Sb.Diff : Sb.Cmp; if(!S.isOpen()) { continue; }
                    xDrawSubs(Sb); S.Dirty = true; xDrawSubs(Sb);
                    SDL_Surface* Ss = SDL_RenderReadPixels(S.Ren, nullptr); SDL_Surface* Cs = Ss ? SDL_ConvertSurface(Ss, SDL_PIXELFORMAT_ARGB8888) : nullptr;
                    if(Cs)
                    {
                        const int OutW = Cs->w, OutH = Cs->h; std::vector<uint8_t> Rgb((size_t)OutW * OutH * 3);
                        for(int y = 0; y < OutH; y++) { const uint8_t* B = (const uint8_t*)Cs->pixels + (size_t)y * Cs->pitch; uint8_t* D = &Rgb[(size_t)y * OutW * 3]; for(int x = 0; x < OutW; x++) { D[3 * x] = B[4 * x + 2]; D[3 * x + 1] = B[4 * x + 1]; D[3 * x + 2] = B[4 * x]; } }
                        std::string P = O.TestShot; const size_t Dot = P.rfind('.'); P.insert(Dot == std::string::npos ? P.size() : Dot, k == 0 ? "_diff" : "_cmp");
                        Hst.writePng(P.c_str(), Rgb.data(), OutW, OutH);
                    }
                    if(Cs) { SDL_DestroySurface(Cs); } if(Ss) { SDL_DestroySurface(Ss); }
                }
                Quit = true;
            }
            SDL_RenderPresent(U.Renderer);
            xDrawSubs(Sb);
            if(!O.VSync) { SDL_Delay(2); }
        }

        xCloseSub(Sb.Diff); xCloseSub(Sb.Cmp);
        ImGui_ImplSDLRenderer3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();
        if(U.Texture) { SDL_DestroyTexture(U.Texture); }
        SDL_DestroyRenderer(U.Renderer); SDL_DestroyWindow(U.Window); SDL_Quit();
        return 0;
    }

    //===============================================================================================================================================================================================================

} //end of namespace gsp