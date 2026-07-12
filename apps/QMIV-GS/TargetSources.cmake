set(SRCLIST_APP src/main_QMGC.cpp src/xAppQMGC.h src/xAppQMGC.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${SRCLIST_APP})
source_group(App FILES ${SRCLIST_APP})

