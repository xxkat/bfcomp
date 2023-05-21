cmake_minimum_required(VERSION 3.25.0)

project(bfcomp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(
  ${PROJECT_NAME}
  src/main.cc
)

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
  src
)

target_compile_features(
  ${PROJECT_NAME}
  PRIVATE
  cxx_std_23
)

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
  -Wall
  -Wextra
  -Werror
  -Wpedantic
  -flto
  -fno-rtti
  -fno-exceptions
)
