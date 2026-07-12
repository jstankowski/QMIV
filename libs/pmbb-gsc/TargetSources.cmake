set(SRCLIST_COMMON_H src/xCommonDefGSC.h)

set(SRCLIST_CLOUD_H src/xPointCloud.h   src/xGaussianCloud.h   src/xPly.h   src/xSplatOps.h   src/xSphHarmUtils.h)
set(SRCLIST_CLOUD_C src/xPointCloud.cpp src/xGaussianCloud.cpp src/xPly.cpp src/xSplatOps.cpp                    )

set(SRCLIST_PUBLIC  ${SRCLIST_COMMON_H} ${SRCLIST_CLOUD_H})
set(SRCLIST_PRIVATE                     ${SRCLIST_CLOUD_C})

target_sources(${PROJECT_NAME} PRIVATE ${SRCLIST_PRIVATE} PUBLIC ${SRCLIST_PUBLIC})
source_group(Common FILES ${SRCLIST_COMMON_H})
source_group(Cloud  FILES ${SRCLIST_CLOUD_H} ${SRCLIST_CLOUD_C})