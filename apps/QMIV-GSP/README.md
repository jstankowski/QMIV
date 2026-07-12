# GS-Player

Real-time viewer of Gaussian-splat clouds on the renderer of QMIV-GS (the same code: what is shown is what the metrics are calculated on).

* one cloud: the render in a window, no metrics;
* two clouds (`-i1`): side by side (the window is two pictures wide) with the quality metrics of the pair calculated on the fly and shown in the window (the code of QMIV-GS: `xAppQM`).

Nothing is written to the disk (except the screenshot, the recorded flight and the test files asked for).

## Build

GS-Player is optional and has two dependencies that no other application uses: SDL3 (window, keyboard, mouse, texture) and Dear ImGui (the overlay). Both are fetched from GitHub while CMake configures
(`cmake/gsplayer.cmake`, as `fmt`) and linked statically.

    cmake -S . -B build -DPMBB_BUILD_GS_PLAYER=ON
    cmake --build build --config Release --target GS-Player

On Linux SDL3 needs the development packages of the video backends it should use (X11 or Wayland, e.g. `libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libwayland-dev`),
without them it is built with the dummy video driver only (the window tests below still run).

The window code (`ui/`, library `gsp-ui`) is built once with the default compiler flags; the application is built per microarchitecture level and dispatched at run time as the other applications.
They meet in `ui/src/GspUi.h` (plain types only).

## Usage

    GS-Player -i0 cloud_{FrameIdx:04d}.ply -cpf cameras -cpm COLMAP
    GS-Player -i0 test_{FrameIdx:04d}.ply -i1 ref_{FrameIdx:04d}.ply -cpf cameras -cpm COLMAP -ml "PSNR,SSIM,IVPSNR"

`-ps` is the size of one picture (default 960x540: one cloud 960x540, two clouds 1920x540); the render follows the window when it is resized (the vertical size scales the camera, pixels stay square).
No arguments (or a wrong option) print all the options. The metrics are the ones of QMIV-GS (`-ml`, default `PSNR, SSIM`; `-mdom YCbCr|RGB|Both`, default the integer BT.709 YCbCr; `-sr -unc -ssm -ssb -sss -ssw`).
The IV-metrics and the others that cost more than a render are calculated as often as they leave room for the renders (every n-th picture); a still picture (paused) is always measured.

### Camera

* **F1 free flight**, along the axes of the camera you look from: A D left / right, W S up / down, E Q forward / back (into the picture / out of it), Z X roll, hold the right mouse button and move the mouse to look, the wheel changes the speed (Shift faster, Ctrl slower),
  Home goes back to the pose of the current input view. The speed starts at 0.3 of the median depth of the scene (seen from the first view) per second.
* **F2 input views**: the cameras of the camera parameters file (the views QMIV-GS renders), scaled to the window; Left / Right.
* **R**: records the free flight to `gsplayer_path.csv` (`-rec`), a posetrace that QMIV-GS, AppGSR and this player (`-pt`) read; R again stops.

### Playback

Space play / pause, `,` `.` step, the slider seeks, `+` `-` change the rate (`-fps`, default 30), O loop. The frames are read ahead by a loader thread (`-nbuf` frames, default 6, within `-mem` of the
physical memory, default 0.5); when the next frame is not there yet the clock waits ("BUFFERING") instead of skipping frames. A file without `{FrameIdx}` is a still scene.

### Other keys

F12 screenshot (`gsplayer_0000.png`, the pictures without the overlay), M the metric of the plot, TAB overlay on / off, F5 help, Esc quit.

### Comparison windows (two clouds only; they open next to the main window, all of them can be open at once, K / L again or Esc in the window close it)

* **K**: the difference of the two renders, `(A - B) + 128` per color channel, saturated at 0 and 255 (no difference = gray). `+` / `-` in that window: the gain x1, x2, x4 ... (x0.25 .. x64).
* **L**: the split view: left of the cursor the pixels of the render A, right of it the render B; the line follows the mouse. **V** in that window switches to above / below the cursor.

## Tests without a user

* `-bench N`: no window; renders N frames of the input views (`-mode 0|1|2`: the free camera at the first view, the input views, the posetrace) with the metrics and prints the times and `HashSynth0`,
  which equals `HashSynth0` of QMIV-GS for the same clouds / cameras / `-ps` / `-psr 1` (checked: musical, 21 views, 960x540, and a posetrace recorded by the free flight, 640x360); the means of the
  metrics equal the averages of QMIV-GS (PSNR, SSIM, IVPSNR, IVSSIM in RGB and YCbCr, 6 digits).
* `-uitest Frames` / `-uitestms Ms` `-uishot file.png`: runs the window for that long and writes the picture of the window with the overlay (`SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software` runs it
  without a display); `-script "ms:cmd[:arg];..."` presses the keys at the given times (toggle, stepf, stepb, seek:N, next, prev, mode:N, reset, record, shot, metric, loop, speed:+1, move:F:R:U:ms,
  look:dx:dy, roll:R:ms, size:W:H).
* `gsp-ui-test`: the window with a fake host.