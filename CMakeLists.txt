cmake_minimum_required(VERSION 3.16)
project(framecolor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The OpenVR library that ships with SteamVR (its path on the Frame's SteamOS)
set(OPENVR_LIB_DIR "/opt/steamvr/bin/linuxarm64" CACHE PATH "Directory containing libopenvr_api.so")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(VULKAN REQUIRED IMPORTED_TARGET vulkan)

add_executable(framecolor
  src/framecolor_main.cpp
  src/draw.cpp
  src/i18n.cpp
  src/vk_texture.cpp
  src/vr_overlay.cpp
)
target_include_directories(framecolor PRIVATE third_party/openvr)
target_compile_options(framecolor PRIVATE -Wall -Wextra)
target_link_libraries(framecolor PRIVATE
  PkgConfig::CAIRO
  PkgConfig::FREETYPE
  PkgConfig::VULKAN
  Threads::Threads
  "${OPENVR_LIB_DIR}/libopenvr_api.so"
)
# Use SteamVR's own library at run time too
set_target_properties(framecolor PROPERTIES
  INSTALL_RPATH "${OPENVR_LIB_DIR}"
  BUILD_WITH_INSTALL_RPATH TRUE
)

install(TARGETS framecolor RUNTIME DESTINATION bin)
