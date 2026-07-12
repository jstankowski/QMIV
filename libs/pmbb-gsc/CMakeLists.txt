#=========================================================================================================================================
# library PMBB-GSC - Poznan-Multimedia-Building-Blocks - Gaussian Splatting & Cloud
#=========================================================================================================================================
set(LIB_CURRENT_NAME "pmbb-gsc")
set(LIB_DEPENDENCIES "${LIB_PMBB_BASE_NAME}" "${LIB_PMBB_CORE_NAME}")

set(LIB_PMBB_GSC_NAME "${LIB_CURRENT_NAME}")
set(LIB_PMBB_GSC_NAME "${LIB_CURRENT_NAME}" PARENT_SCOPE)

if(NOT PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES)  
  PMBB_setup_lib_common_default()
else() #PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES
  PMBB_setup_lib_common_multiarch()
endif() #PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES