
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SYSROOT}/usr/share/cmake/")
include(XilinxCommon)
include(XilinxVersion)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
set(OpenCV_LIBS opencv_core opencv_video opencv_videoio opencv_imgproc opencv_imgcodecs opencv_highgui)
add_library(${PROJECT_NAME} SHARED
  ../include/vitis/ai/reidtracker.hpp
  ftd/ftd_filter_linear.cpp  ftd/ftd_structure.cpp  ftd/ftd_trajectory.cpp
  ftd/ftd_filter_linear.hpp  ftd/ftd_structure.hpp  ftd/ftd_trajectory.hpp
  ftd/ftd_hungarian.cpp
  ftd/ftd_hungarian.hpp
  common.hpp   ring_queue.hpp  state_map.cpp  state_map.hpp
  tracker.cpp tracker_imp.cpp tracker_imp.hpp
  ${CMAKE_CURRENT_BINARY_DIR}/version.c
  )

target_link_libraries(${PROJECT_NAME}  ${OpenCV_LIBS} vitis_ai_library-reid pthread glog)
set_target_properties(${PROJECT_NAME} PROPERTIES
  LIBRARY_OUTPUT_NAME  ${PROJECT_NAME}
  )
install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-targets
  RUNTIME DESTINATION ${INSTALL_BIN_DIR}
  LIBRARY DESTINATION ${INSTALL_LIB_DIR})

#install(
#  EXPORT ${PROJECT_NAME}-targets
#  NAMESPACE ${PROJECT_NAME}::
#  DESTINATION ${INSTALL_CMAKE_DIR})

#install(FILES ../include/vitis/ai/reidtracker.hpp
#  DESTINATION include/vitis/ai/)
