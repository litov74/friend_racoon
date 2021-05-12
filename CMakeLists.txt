
set(EXTRA_COMPONENTS_DIRS "${CMAKE_SOURCE_DIR}/components")
cmake_minimum_required(VERSION 3.5)
# set(PROJECT_VER "Racoon.D.2020-11.154")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(racoon)

add_custom_command(
    TARGET app          # ESP вызывает это всё после сборки бинарника !!!
    # TARGET bootloader
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -P "${CMAKE_SOURCE_DIR}/build_number_increment.cmake"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
