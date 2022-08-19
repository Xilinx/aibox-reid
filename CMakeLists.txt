#
# Copyright 2019 Xilinx Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.5)

project(aibox-reid)

SET(CMAKE_INSTALL_PREFIX $ENV{OECORE_TARGET_SYSROOT})

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing -fpermissive -rdynamic -Wl,--no-undefined" )
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++17")
SET(CMAKE_CXX_FLAGS_DEBUG "-O0")
SET(CMAKE_C_FLAGS_DEBUG "-O0")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
find_package(GStreamer REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS opencv_core opencv_video opencv_videoio opencv_imgproc opencv_imgcodecs opencv_highgui)

SET(INSTALL_PATH "opt/xilinx/kv260-aibox-reid")


add_subdirectory(reidtracker)

add_library(vvas_reid SHARED src/vvas_reid.cpp src/common.hpp)
target_include_directories(vvas_reid PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(vvas_reid PRIVATE reidtracker/include/)
set_target_properties(vvas_reid PROPERTIES INSTALL_RPATH ${INSTALL_PATH}/lib)
target_link_libraries(vvas_reid
  gstapp-1.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 glib-2.0 gstvideo-1.0 gstallocators-1.0 gstrtsp-1.0 gstrtspserver-1.0
  glib-2.0 gobject-2.0 ${OpenCV_LIBS} jansson vitis_ai_library-reid aa2_reidtracker gstvvasinfermeta-2.0 glog pthread
  )
install(TARGETS vvas_reid DESTINATION ${INSTALL_PATH}/lib)
add_dependencies(vvas_reid aa2_reidtracker)

add_library(vvas_crop SHARED src/vvas_crop.cpp)
target_include_directories(vvas_crop PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(vvas_crop
  gstapp-1.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 glib-2.0 gstvideo-1.0 gstallocators-1.0 gstrtsp-1.0 gstrtspserver-1.0
  glib-2.0 gobject-2.0 ${OpenCV_LIBS} jansson vvasutil-2.0 gstvvasinfermeta-2.0 glog)
install(TARGETS vvas_crop DESTINATION ${INSTALL_PATH}/lib)

add_library(vvas_drawreid SHARED src/vvas_drawreid.cpp)
target_include_directories(vvas_drawreid PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(vvas_drawreid
  gstapp-1.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 glib-2.0 gstvideo-1.0 gstallocators-1.0 gstrtsp-1.0 gstrtspserver-1.0
  glib-2.0 gobject-2.0 ${OpenCV_LIBS}  jansson vvasutil-2.0 gstvvasinfermeta-2.0)
install(TARGETS vvas_drawreid DESTINATION ${INSTALL_PATH}/lib)


add_library(vvas_pedpp SHARED src/vvas_pedpp.c)
target_include_directories(vvas_pedpp PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(vvas_pedpp
  jansson vvasutil-2.0 gstvvasinfermeta-2.0)
install(TARGETS vvas_pedpp DESTINATION ${INSTALL_PATH}/lib)

add_executable(${CMAKE_PROJECT_NAME} src/main.cpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME}
  gstapp-1.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 glib-2.0 gstvideo-1.0 gstallocators-1.0 gstrtsp-1.0 gstrtspserver-1.0
  glib-2.0 gobject-2.0 )
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${INSTALL_PATH}/bin)

install(PROGRAMS
    script/aibox-reid-install.py
    DESTINATION ${INSTALL_PATH}/bin)

install(FILES
    README
    DESTINATION ${INSTALL_PATH}/
    RENAME      README_AIBOX-REID
    )

install(FILES
    config/ped_pp.json
    config/refinedet.json
    config/crop.json
    config/reid.json
    config/draw_reid.json
    DESTINATION ${INSTALL_PATH}/share/vvas/)

install(DIRECTORY
    models/kv260/personreid-res18_pt
	models/kv260/refinedet_pruned_0_96
    DESTINATION ${INSTALL_PATH}/share/vitis_ai_library/models)

install(DIRECTORY
    notebook/
    DESTINATION ${INSTALL_PATH}/share/notebooks/)

set(VERSION "1.0.1")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
set(CPACK_RPM_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")
include(CPack)
