# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.6)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.${APP_TARGET}")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.${APP_TARGET}")
else()
    MESSAGE(FATAL_ERROR "-- not find sdkconfig.${APP_TARGET}")
endif()

if(${FACTORY_PARTITION})
    set(SDKCONFIG_DEFAULTS "${SDKCONFIG_DEFAULTS};sdkconfig.partition.factory")
else()
    set(SDKCONFIG_DEFAULTS "${SDKCONFIG_DEFAULTS};sdkconfig.partition.without-factory")
endif()

if(${APP_RELEASE})
    set(SDKCONFIG_DEFAULTS "${SDKCONFIG_DEFAULTS};sdkconfig.release")
else()
    set(SDKCONFIG_DEFAULTS "${SDKCONFIG_DEFAULTS};sdkconfig.debug")
endif()

MESSAGE(STATUS "-- target[${APP_TARGET}] Using defaults: ${SDKCONFIG_DEFAULTS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(${APP_TARGET})