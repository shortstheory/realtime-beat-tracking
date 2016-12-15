cmake_minimum_required(VERSION 3.5)
project(realtime-beat)
set(CMAKE_BUILD_TYPE Release)

set(EXECUTABLE_NAME "realtime-beat")
add_executable(${EXECUTABLE_NAME} realtime-beat.cpp)
target_compile_features(${EXECUTABLE_NAME} PRIVATE cxx_auto_type)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
find_package(SFML 2.0 REQUIRED system window graphics network audio)
FIND_LIBRARY(REQ_LIBRARIES NAMES libfftw3 librtaudio5a)

target_link_libraries(${EXECUTABLE_NAME} ${SFML_LIBRARIES} rtaudio fftw3)
