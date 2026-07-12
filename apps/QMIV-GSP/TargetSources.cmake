set(SRCLIST_APP src/main_GSPlayer.cpp src/xAppGSPlayer.h src/xAppGSPlayer.cpp src/xGspCache.h src/xGspCache.cpp src/xGspCamera.h src/xGspCamera.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${SRCLIST_APP})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/spng/src ${LIB_GSP_UI_DIR}/src)
source_group(App FILES ${SRCLIST_APP})