
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
set(VITA_VERSION "01.41")

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
)

target_link_libraries(${PROJECT_NAME}
  m
  SceAppMgr_stub
  SceCtrl_stub
  SceIofilemgr_stub
  SceKernelThreadMgr_stub
  ScePromoterUtil_stub
  SceSysmodule_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

vita_create_vpk(MrWrack-PKG-Converter-v1.4.1-build-fix.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
       resources/head.bin resources/head.bin
)
