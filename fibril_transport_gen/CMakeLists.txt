cmake_minimum_required(VERSION 3.10)
project(fibril_transport_gen)

set(CMAKE_CXX_STANDARD 17)

# ROS 2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(fibril_transport_common REQUIRED)

# Tree-sitter dependency (システムパッケージ)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TREE_SITTER REQUIRED tree-sitter)

# Tree-sitter文法から生成されたパーサー
add_library(tree_sitter_fibril STATIC
  grammar/src/parser.c
)

target_include_directories(tree_sitter_fibril PUBLIC
  grammar/src
)

add_executable(fibril_transport_gen
  src/main.cpp
  src/parser/parser.cpp
  src/parser/ast.cpp
  src/validator/validator.cpp
  src/validator/ros_validator.cpp
  src/generator/cpp_generator.cpp
)

target_include_directories(fibril_transport_gen PRIVATE
  ${TREE_SITTER_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

ament_target_dependencies(fibril_transport_gen
  rclcpp
  fibril_transport_common
)

target_link_libraries(fibril_transport_gen
  ${TREE_SITTER_LIBRARIES}
  tree_sitter_fibril
  stdc++fs
)

install(TARGETS fibril_transport_gen
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  
  ament_add_gtest(test_parser
    test/test_parser.cpp
    src/parser/parser.cpp
    src/parser/ast.cpp
  )
  
  target_include_directories(test_parser PRIVATE
    ${TREE_SITTER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  
  target_link_libraries(test_parser
    ${TREE_SITTER_LIBRARIES}
    tree_sitter_fibril
    stdc++fs
  )
  
  ament_add_gtest(test_validator
    test/test_validator.cpp
    src/parser/parser.cpp
    src/parser/ast.cpp
    src/validator/validator.cpp
    src/validator/ros_validator.cpp
  )
  
  target_include_directories(test_validator PRIVATE
    ${TREE_SITTER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  
  ament_target_dependencies(test_validator
    rclcpp
    fibril_transport_common
  )
  
  target_link_libraries(test_validator
    ${TREE_SITTER_LIBRARIES}
    tree_sitter_fibril
    stdc++fs
  )
  
  ament_add_gtest(test_ros_validator
    test/test_ros_validator.cpp
    src/parser/parser.cpp
    src/parser/ast.cpp
    src/validator/validator.cpp
    src/validator/ros_validator.cpp
  )
  
  target_include_directories(test_ros_validator PRIVATE
    ${TREE_SITTER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  
  ament_target_dependencies(test_ros_validator
    rclcpp
    fibril_transport_common
  )
  
  target_link_libraries(test_ros_validator
    ${TREE_SITTER_LIBRARIES}
    tree_sitter_fibril
    stdc++fs
  )
endif()

ament_package()
