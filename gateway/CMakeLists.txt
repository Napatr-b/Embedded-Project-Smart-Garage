cmake_minimum_required(VERSION 3.16)
# src is our IDF component; __pio_env supplies PlatformIO generic build settings.
set(COMPONENTS src __pio_env)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smart_garage_gateway)
