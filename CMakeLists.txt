cmake_minimum_required (VERSION 3.5)
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
project(OSMExpress)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g")
set(CMAKE_CXX_FLAGS "-Wno-deprecated")
set(CMAKE_CXX_FLAGS "-Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS "-pthread")
set(OSMX_VERSION "0.2.0")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(ROARING_BUILD_STATIC ON CACHE INTERNAL "")
set(ROARING_LINK_STATIC ON CACHE INTERNAL "")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FetchContent)

# TODO: Switch to a released version after next CapnProto release (post 1.2.0).
# Reason: https://github.com/capnproto/capnproto/issues/2353
# Change for v1: https://github.com/capnproto/capnproto/pull/2355
# Change for v2: https://github.com/capnproto/capnproto/pull/2354
FetchContent_Declare(
    CapnProto
    GIT_REPOSITORY https://github.com/capnproto/capnproto.git
    GIT_TAG master
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS)

FetchContent_Declare(
    Osmium
    GIT_REPOSITORY https://github.com/osmcode/libosmium.git
    GIT_TAG v2.22.0
    SOURCE_SUBDIR test/catch
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS)

FetchContent_Declare(
    Protozero
    GIT_REPOSITORY https://github.com/mapbox/protozero.git
    GIT_TAG v1.8.0
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS)

FetchContent_MakeAvailable(CapnProto Osmium Protozero)

if(NOT CapnProto_FOUND)
    add_subdirectory(${capnproto_SOURCE_DIR} EXCLUDE_FROM_ALL)
endif()

if(NOT OSMIUM_FOUND)
    add_library(Osmium INTERFACE)
    include_directories(SYSTEM ${osmium_SOURCE_DIR}/include)
endif()

if(NOT Protozero_FOUND)
    add_library(Protozero INTERFACE)
    include_directories(SYSTEM ${protozero_SOURCE_DIR}/include)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/s2geometry EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/CRoaring EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/Catch2 EXCLUDE_FROM_ALL)

add_custom_target(build_lmdb COMMAND make liblmdb.a CC=clang XCFLAGS=-D_WANT_SEMUN
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lmdb/libraries/liblmdb
)

include_directories(vendor/s2geometry/src)
include_directories(vendor/CRoaring/cpp)
include_directories(vendor/CRoaring/include)
include_directories(vendor/cxxopts/include)
include_directories(vendor/json/single_include)
include_directories(vendor/lmdb/libraries/liblmdb)
include_directories(include)

# needed for Expat install dir
if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
include_directories(/usr/local/include)
link_directories(osmx /usr/local/lib)
endif()

set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/capnpc_generated)
file(MAKE_DIRECTORY ${CAPNPC_OUTPUT_DIR})
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS include/osmx/messages.capnp)

add_executable(
    osmx
    src/cmd.cpp
    src/storage.cpp
    src/expand.cpp
    src/extract.cpp
    src/update.cpp
    src/region.cpp
    ${CAPNP_SRCS})

add_dependencies(osmx build_lmdb s2)

target_include_directories(
    osmx
    PUBLIC include ${CAPNPC_OUTPUT_DIR}/include)

target_link_libraries(osmx z expat bz2 s2 roaring CapnProto::capnp)
target_link_libraries(osmx ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lmdb/libraries/liblmdb/liblmdb.a)

set_property(TARGET osmx PROPERTY CXX_STANDARD 14)

add_executable(osmxTest test/test_region.cpp src/region.cpp)
set_property(TARGET osmxTest PROPERTY CXX_STANDARD 14)
target_include_directories(
    osmxTest
    PUBLIC include ${CAPNPC_OUTPUT_DIR}/include)
target_link_libraries(osmxTest s2 Catch2::Catch2WithMain)

enable_testing()
add_test(osmxTest osmxTest)

install(TARGETS osmx DESTINATION bin)
add_custom_target(archive COMMAND dist/archive.sh ${OSMX_VERSION} ${CMAKE_SYSTEM_NAME})
add_dependencies(archive osmx)

add_library(
    osmx-static
    STATIC
    src/storage.cpp
    src/expand.cpp
    src/extract.cpp
    src/update.cpp
    src/region.cpp)

set_property(TARGET osmx-static PROPERTY CXX_STANDARD 14)

target_include_directories(
    osmx-static
    PUBLIC include ${CAPNPC_OUTPUT_DIR}/include)

target_link_libraries(osmx-static z expat bz2 s2 roaring CapnProto::capnp)

add_dependencies(osmx-static build_lmdb)
