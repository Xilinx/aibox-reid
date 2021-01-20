
add_executable(test_accuracy test_accuracy_reidtracker.cpp)
target_link_libraries(test_accuracy ${PROJECT_NAME} pthread vitis_ai_library-refinedet)

add_executable(test_images test_images_reidtracker.cpp)
target_link_libraries(test_images ${PROJECT_NAME} pthread vitis_ai_library-refinedet)

