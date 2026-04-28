cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build host_test target with state_store + cJSON
set(EXTRA_COMPONENT_DIRS "../components")
set(COMPONENTS main esptool_py)

project(claude_rlcd_test)
