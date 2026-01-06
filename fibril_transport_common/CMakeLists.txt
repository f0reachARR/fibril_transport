cmake_minimum_required(VERSION 3.10)
project(fibril_transport_common)

set(CMAKE_CXX_STANDARD 17)

# ROS 2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(ros_babel_fish REQUIRED)

# Library
add_library(${PROJECT_NAME} SHARED
  src/dynamic_ros_interface.cpp
  src/type_system.cpp
  src/attribute_system.cpp
  src/ros_attribute.cpp
  src/definition_descriptors.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  ros_babel_fish
)

# Install
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  DESTINATION share/${PROJECT_NAME}/cmake
)

# Testing
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  
  ament_add_gtest(test_dynamic_ros_interface
    test/test_dynamic_ros_interface.cpp
  )
  
  target_link_libraries(test_dynamic_ros_interface
    ${PROJECT_NAME}
  )
  
  # New tests for core components
  ament_add_gtest(test_type_system
    test/test_type_system.cpp
  )
  
  target_link_libraries(test_type_system
    ${PROJECT_NAME}
  )
  
  ament_add_gtest(test_attribute_system
    test/test_attribute_system.cpp
  )
  
  target_link_libraries(test_attribute_system
    ${PROJECT_NAME}
  )
  
  ament_add_gtest(test_definition_descriptors
    test/test_definition_descriptors.cpp
  )
  
  target_link_libraries(test_definition_descriptors
    ${PROJECT_NAME}
  )
endif()

ament_export_targets(${PROJECT_NAME}Targets HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp ros_babel_fish)

ament_package()
