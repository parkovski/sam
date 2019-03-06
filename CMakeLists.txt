cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(sam VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(-diagnostics:caret)
endif()

# list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
# find_package(uvw CONFIG REQUIRED)
# find_package(nlohmann_json CONFIG REQUIRED)

set(DAEMON_SRC daemon.cpp tts.cpp)
set(CLIENT_SRC client.cpp)
list(TRANSFORM DAEMON_SRC PREPEND "src/")
list(TRANSFORM CLIENT_SRC PREPEND "src/")

include_directories(${PROJECT_SOURCE_DIR}/include)
link_libraries(
  spdlog::spdlog
  fmt::fmt-header-only
  # nlohmann_json
  # nlohmann_json::nlohmann_json
  # uvw
)

add_executable(samd ${DAEMON_SRC})
target_include_directories(samd PRIVATE "C:/Program Files (x86)/Microsoft Speech SDK/Include")
target_link_libraries(samd PRIVATE ole32.lib user32.lib)
target_compile_options(samd PRIVATE /MT)
add_executable(samc ${CLIENT_SRC})
