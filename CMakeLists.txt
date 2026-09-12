
cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK")
  endif()
endif()

project(MrWrackPKGConverter C)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/resources/head.bin")
  message(FATAL_ERROR "Missing resources/head.bin. Run scripts/fetch_vitashell_resource.sh first.")
endif()
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "MrWrack PKG Converter")
set(VITA_TITLEID "MRWPKG001")
set(VITA_VERSION "02.00")

add_executable(${PROJECT_NAME}
  src/main.c
  src/fs_scan.c
  src/settings.c
  src/smooth_scroll.c
  src/installer.c
  src/launch.c
  src/sha1.c
  src/sfo.c
  src/sha256.c
  src/mrw_pkg.c
  src/install_flow.c
  src/debugScreen.c
  src/vpk_convert.c
)

target_link_libraries(${PROJECT_NAME}
  m
  z
  SceAppMgr_stub
  SceCtrl_stub
  SceDisplay_stub
  SceIofilemgr_stub
  SceKernelThreadMgr_stub
  ScePromoterUtil_stub
  SceSysmodule_stub
  SceSysmem_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

vita_create_vpk(MrWrack-PKG-Converter-v2.0-livearea.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
       resources/head.bin resources/head.bin
  FILE sce_sys/pic0.png sce_sys/pic0.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE sce_sys/livearea/contents/bg0.png sce_sys/livearea/contents/bg0.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
)

