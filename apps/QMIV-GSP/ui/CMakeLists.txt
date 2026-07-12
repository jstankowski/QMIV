cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

#=========================================================================================================================================
# gsp-ui - window, input and overlay of GS-Player (SDL3 + Dear ImGui). Built once with the default compiler flags (not per microarchitecture
# level) and used by the application through the plain interface of src/GspUi.h
#=========================================================================================================================================
set(LIB_GSP_UI_NAME "gsp-ui")
set(LIB_GSP_UI_NAME "gsp-ui" PARENT_SCOPE)

project(${LIB_GSP_UI_NAME} CXX)
add_library(${PROJECT_NAME} STATIC src/GspUi.h src/GspUi.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features   (${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
target_link_libraries     (${PROJECT_NAME} PUBLIC gsp-imgui SDL3::SDL3-static)

# UI test with a fake host (window with a picture and the overlay, written as PNG)
add_executable(gsp-ui-test test/test_ui.cpp)
target_compile_features(gsp-ui-test PRIVATE cxx_std_17)
target_link_libraries(gsp-ui-test PRIVATE gsp-ui)