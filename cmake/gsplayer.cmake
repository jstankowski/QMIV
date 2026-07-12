# GS-Player dependencies (used only by the GS-Player application, switched on by PMBB_BUILD_GS_PLAYER): SDL3 (window, input, texture) and Dear ImGui (overlay), both fetched from GitHub and linked statically
message(STATUS "${PROJECT_NAME} --> trying to fetch https://github.com/libsdl-org/SDL.git and https://github.com/ocornut/imgui.git (GS-Player)")
enable_language(C) # SDL3 is written in C
include(FetchContent)

set(SDL_SHARED   OFF CACHE BOOL "" FORCE)
set(SDL_STATIC   ON  CACHE BOOL "" FORCE)
set(SDL_TESTS    OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL  OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(sdl3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.4.20
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.9b
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)
# the project switches on the whole program optimization (CMAKE_INTERPROCEDURAL_OPTIMIZATION): the C sources of SDL3 do not compile with it on MSVC (/GL: memcpy helpers), SDL gets it off
set(GSP_SAVED_IPO ${CMAKE_INTERPROCEDURAL_OPTIMIZATION})
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
FetchContent_MakeAvailable(sdl3 imgui)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${GSP_SAVED_IPO})

# Dear ImGui has no CMake project: the core files and the SDL3 + SDL_Renderer backends
add_library(gsp-imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(gsp-imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(gsp-imgui PUBLIC SDL3::SDL3-static)
target_compile_features(gsp-imgui PRIVATE cxx_std_17)
message(STATUS "${PROJECT_NAME} --> fetch DONE (QMIV-GS-Player)")