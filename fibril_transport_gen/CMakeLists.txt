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

add_executable(fibril_transport_gen
  src/main.cpp
  src/parser/parser.cpp
  src/parser/ast.cpp
  src/validator/validator.cpp
  src/generator/generator.cpp
  src/generator/cpp_generator.cpp
  src/generator/bin_generator.cpp
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
)

install(TARGETS fibril_transport_gen
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  # テスト追加
endif()

ament_package()
