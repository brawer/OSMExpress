# SPDX-FileCopyrightText: 2025 Sascha Brawer <sascha@brawer.ch>
# SPDX-License-Identifier: BSD-2-Clause
#
# CMake script for finding LMDB when installed as a system package.
# If found, LMDB_FOUND is set to TRUE and a target LMDB::LMDB is defined.

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LMDB QUIET lmdb)
endif()

find_path(
    LMDB_INCLUDE_DIR
    NAMES lmdb.h
    HINTS ${PC_LMDB_INCLUDE_DIRS}
    PATH_SUFFIXES include)

find_library(
    LMDB_LIBRARY
    NAMES lmdb
    HINTS ${PC_LMDB_LIBRARY_DIRS}
    PATH_SUFFIXES lib)

if (LMDB_INCLUDE_DIR AND LMDB_LIBRARY)
    file(READ "${LMDB_INCLUDE_DIR}/lmdb.h" ver)
    string(REGEX MATCH "MDB_VERSION_MAJOR[\t ]+([0-9]+)[\t ]*" _ ${ver})
    set(ver_major ${CMAKE_MATCH_1})
    string(REGEX MATCH "MDB_VERSION_MINOR[\t ]+([0-9]+)[\t ]*" _ ${ver})
    set(ver_minor ${CMAKE_MATCH_1})
    string(REGEX MATCH "MDB_VERSION_PATCH[\t ]+([0-9]+)[\t ]*" _ ${ver})
    set(ver_patch ${CMAKE_MATCH_1})
    set(LMDB_VERSION "${ver_major}.${ver_minor}.${ver_patch}")
endif()

find_package_handle_standard_args(
    LMDB
    REQUIRED_VARS LMDB_LIBRARY LMDB_INCLUDE_DIR
    VERSION_VAR LMDB_VERSION)

if (LMDB_FOUND AND NOT TARGET LMDB::LMDB)
    message(
        STATUS
        "Found LMDB: ${LMDB_LIBRARY} (found version \"${LMDB_VERSION}\")")
    add_library(LMDB::LMDB INTERFACE IMPORTED)
    set_target_properties(
        LMDB::LMDB
        PROPERTIES
            INTERFACE_LINK_LIBRARIES      "${LMDB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LMDB_INCLUDE_DIR}")
endif()
