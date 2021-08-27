cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(sam VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 OR NOT WIN32)
  message(FATAL_ERROR "This program must be built for 32-bit Windows")
endif()

if(MSVC)
  add_compile_options(-diagnostics:caret)
endif()

#list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/contrib)
add_subdirectory(contrib/spdlog)
add_subdirectory(contrib/fmt)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

#find_package(spdlog REQUIRED)
#find_package(fmt REQUIRED)

set(SOURCES sam.cpp tts.cpp)
list(TRANSFORM SOURCES PREPEND "src/")
list(APPEND SOURCES "res/res.rc")

include_directories(${PROJECT_SOURCE_DIR}/include)
link_libraries(
  spdlog::spdlog_header_only
  fmt::fmt-header-only
)

add_executable(sam ${SOURCES})
target_include_directories(sam PRIVATE "C:/Program Files (x86)/Microsoft Speech SDK/Include")
target_link_libraries(sam PRIVATE ole32.lib user32.lib)
set_property(TARGET sam PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
