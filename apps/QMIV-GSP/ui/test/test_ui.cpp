/*
SPDX-FileCopyrightText: 2026 Dziembo
SPDX-License-Identifier: BSD-3-Clause
*/
//UI test: a fake host (a gradient picture, a few metric rows) - runs the window for a few frames, writes the picture of the window with the overlay as a PNG and returns.
//usage: gsp-ui-test [out.png] [frames] [width] [height]    (SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software runs it without a display)
#include "GspUi.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string>

static uint32_t xCrc(const uint8_t* D, size_t N, uint32_t C = 0xFFFFFFFFu)
{
    static uint32_t T[256]; static bool Init = false;
    if(!Init) { for(uint32_t n = 0; n < 256; n++) { uint32_t c = n; for(int k = 0; k < 8; k++) { c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1); } T[n] = c; } Init = true; }
    for(size_t i = 0; i < N; i++) { C = T[(C ^ D[i]) & 0xFF] ^ (C >> 8); }
    return C;
}
static void xChunk(FILE* F, const char* Type, const std::vector<uint8_t>& Data)
{
    uint8_t L[4] = { (uint8_t)(Data.size() >> 24), (uint8_t)(Data.size() >> 16), (uint8_t)(Data.size() >> 8), (uint8_t)Data.size() }; fwrite(L, 1, 4, F);
    std::vector<uint8_t> B(Type, Type + 4); B.insert(B.end(), Data.begin(), Data.end()); fwrite(B.data(), 1, B.size(), F);
    uint32_t C = ~xCrc(B.data(), B.size()); uint8_t Cb[4] = { (uint8_t)(C >> 24), (uint8_t)(C >> 16), (uint8_t)(C >> 8), (uint8_t)C }; fwrite(Cb, 1, 4, F);
}
static bool xWritePng(const char* Path, const uint8_t* Rgb, int W, int H) //uncompressed (stored deflate blocks)
{
    FILE* F = fopen(Path, "wb"); if(!F) { return false; }
    const uint8_t Sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 }; fwrite(Sig, 1, 8, F);
    std::vector<uint8_t> Ihdr = { (uint8_t)(W >> 24), (uint8_t)(W >> 16), (uint8_t)(W >> 8), (uint8_t)W, (uint8_t)(H >> 24), (uint8_t)(H >> 16), (uint8_t)(H >> 8), (uint8_t)H, 8, 2, 0, 0, 0 }; xChunk(F, "IHDR", Ihdr);
    std::vector<uint8_t> Raw; Raw.reserve((size_t)H * (W * 3 + 1)); for(int y = 0; y < H; y++) { Raw.push_back(0); Raw.insert(Raw.end(), Rgb + (size_t)y * W * 3, Rgb + (size_t)(y + 1) * W * 3); }
    std::vector<uint8_t> Z = { 0x78, 0x01 }; size_t Pos = 0; uint32_t A = 1, B = 0;
    for(uint8_t v : Raw) { A = (A + v) % 65521; B = (B + A) % 65521; }
    while(Pos < Raw.size()) { size_t N = std::min<size_t>(65535, Raw.size() - Pos); Z.push_back(Pos + N >= Raw.size() ? 1 : 0); Z.push_back((uint8_t)N); Z.push_back((uint8_t)(N >> 8)); Z.push_back((uint8_t)~N); Z.push_back((uint8_t)(~N >> 8)); Z.insert(Z.end(), Raw.begin() + Pos, Raw.begin() + Pos + N); Pos += N; }
    uint32_t Ad = (B << 16) | A; Z.push_back((uint8_t)(Ad >> 24)); Z.push_back((uint8_t)(Ad >> 16)); Z.push_back((uint8_t)(Ad >> 8)); Z.push_back((uint8_t)Ad);
    xChunk(F, "IDAT", Z); xChunk(F, "IEND", {}); fclose(F); return true;
}

class FakeHost : public gsp::Host
{
public:
    std::vector<uint8_t> Pic; int W = 0, H = 0; bool Fresh = true; int Resizes = 0;
    void make(int w, int h) { W = w; H = h; Pic.resize((size_t)w * h * 3); for(int y = 0; y < h; y++) for(int x = 0; x < w; x++) { uint8_t* P = &Pic[((size_t)y * w + x) * 3]; P[0] = (uint8_t)(255 * x / w); P[1] = (uint8_t)(255 * y / h); P[2] = (x / 32 + y / 32) % 2 ? 200 : 60; } Fresh = true; }
    bool pollFrame(gsp::Frame& F) override { if(!Fresh) { return false; } F.Rgb = Pic.data(); F.W = W; F.H = H; F.Id = 1; Fresh = false; return true; }
    void releaseFrame() override {}
    void fillHud(gsp::Hud& Hd) override
    {
        std::snprintf(Hd.Line1, sizeof(Hd.Line1), "FREE  frame 12/40  play 30 fps  render %dx%d (SBS)", W / 2, H);
        std::snprintf(Hd.Line2, sizeof(Hd.Line2), "pos 1.00 -2.50 0.30   yaw 12.0 pitch -3.0   speed x1.00");
        std::snprintf(Hd.Message, sizeof(Hd.Message), "recording path: 148 poses");
        Hd.NumFrames = 40; Hd.CurFrame = 12; Hd.Playing = true; Hd.RenderFps = 47.0f; Hd.RenderMs = 21.3f; Hd.Mode = gsp::ModeFree;
        Hd.NumRows = 2;
        std::snprintf(Hd.Rows[0].Name, 24, "PSNR (YCbCr)"); std::snprintf(Hd.Rows[0].Value, 40, "34.512 dB"); std::snprintf(Hd.Rows[0].Avg, 40, "34.201 dB"); std::snprintf(Hd.Rows[0].Comp, 72, "Y 33.1  Cb 40.2  Cr 41.7");
        std::snprintf(Hd.Rows[1].Name, 24, "SSIM (YCbCr)"); std::snprintf(Hd.Rows[1].Value, 40, "0.97312"); std::snprintf(Hd.Rows[1].Avg, 40, "0.96980"); std::snprintf(Hd.Rows[1].Comp, 72, "Y 0.9650  Cb 0.9902  Cr 0.9911");
        Hd.PlotN = 100; for(int i = 0; i < 100; i++) { Hd.Plot[i] = 34.0f + 2.0f * (float)std::sin(i * 0.2); } Hd.PlotMin = 30; Hd.PlotMax = 38; std::snprintf(Hd.PlotName, 40, "PSNR");
    }
    void onInput(const gsp::Input&) override {}
    void onCommand(int, int) override {}
    void onResize(int w, int h) override { Resizes++; make(w, h); }
    bool writePng(const char* Path, const uint8_t* Rgb, int w, int h) override { return xWritePng(Path, Rgb, w, h); }
};

int main(int argc, char** argv)
{
    gsp::UiOptions O; O.TestShot = argc > 1 ? argv[1] : "gsp_ui_test.png"; O.TestFrames = argc > 2 ? std::atoi(argv[2]) : 5;
    if(argc > 3) { O.WinW = std::atoi(argv[3]); } if(argc > 4) { O.WinH = std::atoi(argv[4]); } O.VSync = false;
    FakeHost H; H.make(O.WinW, O.WinH);
    const int Rc = gsp::runUi(H, O);
    std::printf("runUi returned %d, resize notifications %d, picture %dx%d\n", Rc, H.Resizes, H.W, H.H);
    return Rc;
}