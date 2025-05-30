# The following lines of boilerplate have to be in your project's
# CMakeLists.txt file:
cmake_minimum_required(VERSION 3.16)

# Project IDF_TARGET should be defined in environment,
# but if not, default to esp32
if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32_ecu_display)
