cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

#=========================================================================================================================================
# GS-Player - real-time viewer of Gaussian-splat clouds 
#=========================================================================================================================================
add_subdirectory(ui gsp-ui)  # the window (SDL3 + Dear ImGui), built once; defines LIB_GSP_UI_NAME

set(APP_NAME "QMIV-GS-Player")
set(LIB_GSP_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")

set(APP_DEPENDENCIES "${LIB_PMBB_BASE_NAME}" "${LIB_PMBB_CORE_NAME}" "${LIB_PMBB_SLST_NAME}" "${LIB_PMBB_IVQM_NAME}" "${LIB_PMBB_FVGS_NAME}" "${LIB_PMBB_GSR_NAME}" "${LIB_PMBB_QMAU_NAME}" "${LIB_PMBB_HASH_NAME}")
set(APP_THIRD_PARTY  "${THIRDPARTY_SPNG_NAME}" "${LIB_GSP_UI_NAME}")

if(NOT PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES)
  PMBB_setup_app_common_default()
else() #PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES
  if(PMBB_GENERATE_SINGLE_APP_WITH_WITH_RUNTIME_DISPATCH)
    PMBB_setup_app_common_multiarch()
  endif() #PMBB_GENERATE_SINGLE_APP_WITH_WITH_RUNTIME_DISPATCH
  if(PMBB_GENERATE_DEDICATED_APPS_FOR_EVERY_MFL)
    PMBB_setup_app_common_dedicated()
  endif() #PMBB_GENERATE_DEDICATED_APPS_FOR_EVERY_MFL
endif() #PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES