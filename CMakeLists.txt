cmake_minimum_required (VERSION 2.6.4)
include (CheckFunctionExists)
include (CheckIncludeFile)
include (CheckCXXSourceCompiles)


project(SimGear)

# read 'version' file into a variable (stripping any newlines or spaces)
file(READ version versionFile)
string(STRIP ${versionFile} SIMGEAR_VERSION)

# use simgear version also as the SO version (if building SOs)
SET(SIMGEAR_SOVERSION ${SIMGEAR_VERSION})

# Warning when build is not an out-of-source build.
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" InSourceBuild)
if(InSourceBuild)
    message(WARNING  "Avoid building inside the source tree!")
    message(WARNING  "Create a separate build directory instead (i.e. 'sgbuild') and call CMake from there: ")
    message(WARNING  "  mkdir ../sgbuild && cd ../sgbuild && cmake ${CMAKE_SOURCE_DIR}")
endif(InSourceBuild)

#packaging
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
SET(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simulation support libraries for FlightGear and related projects")
SET(CPACK_PACKAGE_VENDOR "The FlightGear project")
SET(CPACK_GENERATOR "TBZ2")
SET(CPACK_INSTALL_CMAKE_PROJECTS  ${CMAKE_CURRENT_BINARY_DIR};SimGear;ALL;/)


# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${SIMGEAR_VERSION} )
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1}) 
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})

message(STATUS "version is ${CPACK_PACKAGE_VERSION_MAJOR} dot ${CPACK_PACKAGE_VERSION_MINOR} dot ${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_SOURCE_GENERATOR TBZ2)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "simgear-${SIMGEAR_VERSION}" CACHE INTERNAL "tarball basename")
set(CPACK_SOURCE_IGNORE_FILES
  "^${PROJECT_SOURCE_DIR}/.git;\\\\.gitignore;Makefile.am;~$;${CPACK_SOURCE_IGNORE_FILES}")

message(STATUS "ignoring: ${CPACK_SOURCE_IGNORE_FILES}")

include (CPack)

# We have some custom .cmake scripts not in the official distribution.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

# Change the default build type to something fast
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Determine name of library installation directory, i.e. "lib" vs "lib64", which
# differs between all Debian-based vs all other Linux distros.
# See cmake bug #11964, http://cmake.org/gitweb?p=cmake.git;a=commit;h=126c993d
# GNUInstallDirs requires CMake >= 2.8.5, use own file for older cmake
if(${CMAKE_VERSION} VERSION_GREATER 2.8.4)
  include(GNUInstallDirs)
else(${CMAKE_VERSION} VERSION_GREATER 2.8.4)
  include(OldGNUInstallDirs)
endif(${CMAKE_VERSION} VERSION_GREATER 2.8.4)
message(STATUS "Library installation directory: ${CMAKE_INSTALL_LIBDIR}")

#####################################################################################
# Configure library search paths
#####################################################################################

if(NOT "${CMAKE_LIBRARY_ARCHITECTURE}" STREQUAL "")
    # Workaround for Ubuntu/Debian which introduced the "multiarch" library
    # directory structure, which is unsupported by CMake < 2.8.10, so we need to
    # add paths manually
    # see http://www.cmake.org/Bug/view.php?id=12049 and
    # http://www.cmake.org/Bug/view.php?id=12037
    list(APPEND ADDITIONAL_LIBRARY_PATHS
         /usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}
         /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
         /lib/${CMAKE_LIBRARY_ARCHITECTURE})
    message(STATUS "additional library directories: ${ADDITIONAL_LIBRARY_PATHS}")
endif()

if(NOT MSVC)
    # TBD: are these really necessary? Aren't they considered by cmake automatically?
    list(APPEND ADDITIONAL_LIBRARY_PATHS
         /opt/local
         /usr/local
         /usr)
endif()
#####################################################################################

if (NOT MSVC)
option(SIMGEAR_SHARED   "Set to ON to build SimGear as a shared library/framework" OFF)
option(SYSTEM_EXPAT     "Set to ON to build SimGear using the system libExpat" OFF)
else()
# Building SimGear DLLs is currently not supported for MSVC.
set(SIMGEAR_SHARED OFF)
# Using a system expat is currently not supported for MSVC - it would require shared simgear (DLL).
set(SYSTEM_EXPAT OFF)
endif()

option(SIMGEAR_HEADLESS "Set to ON to build SimGear without GUI/graphics support" OFF)
option(JPEG_FACTORY     "Enable JPEG-factory support" OFF)
option(ENABLE_LIBSVN    "Set to ON to build SimGear with libsvnclient support" ON)
option(ENABLE_RTI       "Set to ON to build SimGear with RTI support" OFF)
option(ENABLE_TESTS     "Set to OFF to disable building SimGear's test applications" ON)
option(ENABLE_SOUND     "Set to OFF to disable building SimGear's sound support" ON)

if (MSVC)
  GET_FILENAME_COMPONENT(PARENT_DIR ${PROJECT_SOURCE_DIR} PATH)
  if (CMAKE_CL_64)
    SET(TEST_3RDPARTY_DIR "${PARENT_DIR}/3rdparty.x64")
  else (CMAKE_CL_64)
    SET(TEST_3RDPARTY_DIR "${PARENT_DIR}/3rdparty")
  endif (CMAKE_CL_64)
  if (EXISTS ${TEST_3RDPARTY_DIR})
    set(MSVC_3RDPARTY_ROOT ${PARENT_DIR} CACHE PATH "Location where the third-party dependencies are extracted")
  else (EXISTS ${TEST_3RDPARTY_DIR})
    set(MSVC_3RDPARTY_ROOT NOT_FOUND CACHE PATH "Location where the third-party dependencies are extracted")
  endif (EXISTS ${TEST_3RDPARTY_DIR})
else (MSVC)
  set(MSVC_3RDPARTY_ROOT NOT_FOUND CACHE PATH "Location where the third-party dependencies are extracted")
endif (MSVC)

if (MSVC AND MSVC_3RDPARTY_ROOT)
  message(STATUS "3rdparty files located in ${MSVC_3RDPARTY_ROOT}")
  set( OSG_MSVC "msvc" )
  if (${MSVC_VERSION} EQUAL 1700)
      set( OSG_MSVC ${OSG_MSVC}110 )
  elseif (${MSVC_VERSION} EQUAL 1600)
      set( OSG_MSVC ${OSG_MSVC}100 )
  else (${MSVC_VERSION} EQUAL 1700)
      set( OSG_MSVC ${OSG_MSVC}90 )
  endif (${MSVC_VERSION} EQUAL 1700)
  if (CMAKE_CL_64)
      set( OSG_MSVC ${OSG_MSVC}-64 )
      set( MSVC_3RDPARTY_DIR 3rdParty.x64 )
  else (CMAKE_CL_64)
      set( MSVC_3RDPARTY_DIR 3rdParty )
  endif (CMAKE_CL_64)

  set (CMAKE_LIBRARY_PATH ${MSVC_3RDPARTY_ROOT}/${MSVC_3RDPARTY_DIR}/lib ${MSVC_3RDPARTY_ROOT}/install/${OSG_MSVC}/OpenScenegraph/lib ${MSVC_3RDPARTY_ROOT}/install/${OSG_MSVC}/OpenRTI/lib )
  set (CMAKE_INCLUDE_PATH ${MSVC_3RDPARTY_ROOT}/${MSVC_3RDPARTY_DIR}/include ${MSVC_3RDPARTY_ROOT}/install/${OSG_MSVC}/OpenScenegraph/include ${MSVC_3RDPARTY_ROOT}/install/${OSG_MSVC}/OpenRTI/include)
  find_path(BOOST_ROOT boost/version.hpp
			${MSVC_3RDPARTY_ROOT}/boost
			${MSVC_3RDPARTY_ROOT}/boost_1_52_0
			${MSVC_3RDPARTY_ROOT}/boost_1_51_0
			${MSVC_3RDPARTY_ROOT}/boost_1_50_0
			${MSVC_3RDPARTY_ROOT}/boost_1_49_0
			${MSVC_3RDPARTY_ROOT}/boost_1_48_0
			${MSVC_3RDPARTY_ROOT}/boost_1_47_0
			${MSVC_3RDPARTY_ROOT}/boost_1_46_1
			${MSVC_3RDPARTY_ROOT}/boost_1_46_0
			${MSVC_3RDPARTY_ROOT}/boost_1_45_0
			${MSVC_3RDPARTY_ROOT}/boost_1_44_0
			)
  # set (BOOST_ROOT ${MSVC_3RDPARTY_ROOT}/boost_1_44_0)
  message(STATUS "BOOST_ROOT is ${BOOST_ROOT}")
  set (OPENAL_INCLUDE_DIR ${MSVC_3RDPARTY_ROOT}/${MSVC_3RDPARTY_DIR}/include)
  set (OPENAL_LIBRARY_DIR ${MSVC_3RDPARTY_ROOT}/${MSVC_3RDPARTY_DIR}/lib)
endif (MSVC AND MSVC_3RDPARTY_ROOT)

find_package(Boost REQUIRED)
set (BOOST_CXX_FLAGS "-DBOOST_MULTI_INDEX_DISABLE_SERIALIZATION -DBOOST_BIMAP_DISABLE_SERIALIZATION")

if(SIMGEAR_HEADLESS)
    message(STATUS "SimGear mode: HEADLESS")
    set(ENABLE_SOUND 0)
else()
    message(STATUS "SimGear mode: NORMAL")
    find_package(OpenGL REQUIRED)
    
    if (ENABLE_SOUND)
        find_package(OpenAL REQUIRED)
        message(STATUS "Sound support: ENABLED")
    endif(ENABLE_SOUND)
    
    find_package(OpenSceneGraph 3.0.0 REQUIRED osgText osgSim osgDB osgParticle osgGA osgUtil)
endif(SIMGEAR_HEADLESS)

if(JPEG_FACTORY)
    message(STATUS "JPEG-factory: ENABLED")
    find_package(JPEG REQUIRED)
    include_directories(${JPEG_INCLUDE_DIR})
else()
    message(STATUS "JPEG-factory: DISABLED")
endif(JPEG_FACTORY)

if(ENABLE_LIBSVN)
    find_package(SvnClient)

    if(LIBSVN_FOUND)
        message(STATUS "Subversion client support: ENABLED")
        set(HAVE_SVN_CLIENT_H 1)
        set(HAVE_LIBSVN_CLIENT_1 1)
    else()
        # Oops. ENABLE_LIBSVN is ON, but svn is still missing.
        # Provide clearly visible warning/hint, so builders know what else they should install (or disable).
        message(WARNING "Failed to enable subversion client support. Unable to find required subversion client library. Some features may not be available (scenery download).")
        message(WARNING "Install 'libsvn' library/DLL (libsvn-devel/libsvnclient/...). Otherwise disable subversion support (set 'ENABLE_LIBSVN' to 'OFF').")
    endif(LIBSVN_FOUND)
else()
    message(STATUS "Subversion client support: DISABLED")
endif(ENABLE_LIBSVN)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if (SYSTEM_EXPAT)
    message(STATUS "Requested to use system Expat library, forcing SIMGEAR_SHARED to true")
    set(SIMGEAR_SHARED ON)
    find_package(EXPAT REQUIRED)
    include_directories(${EXPAT_INCLUDE_DIRS})
else()
    message(STATUS "Using built-in expat code")
    add_definitions(-DHAVE_EXPAT_CONFIG_H)
endif(SYSTEM_EXPAT)

check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/timeb.h HAVE_SYS_TIMEB_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(windows.h HAVE_WINDOWS_H)

if(HAVE_INTTYPES_H)
  # ShivaVG needs inttypes.h
  add_definitions(-DHAVE_INTTYPES_H)
endif()

if(ENABLE_RTI)
    # See if we have any rti library variant installed
    message(STATUS "RTI: ENABLED")
    find_package(RTI)
else()
    message(STATUS "RTI: DISABLED")
endif(ENABLE_RTI)

check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(ftime HAVE_FTIME)
check_function_exists(timegm HAVE_TIMEGM)
check_function_exists(rint HAVE_RINT)
check_function_exists(mkdtemp HAVE_MKDTEMP)
check_function_exists(bcopy HAVE_BCOPY)
check_function_exists(mmap HAVE_MMAP)

if(HAVE_UNISTD_H)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_INCLUDE_PATH})
    check_cxx_source_compiles(
       "#include <unistd.h>
        #if !defined(_POSIX_TIMERS) || (0 >= _POSIX_TIMERS)
            #error clock_gettime is not supported
        #endif

        int main() { return 0; }
        "
        HAVE_CLOCK_GETTIME)
endif(HAVE_UNISTD_H)

set(RT_LIBRARY "")
if(HAVE_CLOCK_GETTIME)
    check_library_exists(rt clock_gettime "" HAVE_RT)
    if(HAVE_RT)
        set(RT_LIBRARY rt)
    endif(HAVE_RT)
endif(HAVE_CLOCK_GETTIME)

SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually 'd' on windows")
SET(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_MINSIZEREL_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")

# isnan might not be real symbol, so can't check using function_exists
check_cxx_source_compiles(
    "#include <cmath> 
    void f() { isnan(0.0);} "
    HAVE_ISNAN)

check_cxx_source_compiles(
    "#include <cmath> 
    void f() { std::isnan(0.0);} "
    HAVE_STD_ISNAN)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(WARNING_FLAGS_CXX "-Wall")
    set(WARNING_FLAGS_C   "-Wall")

    # certain GCC versions don't provide the atomic builds, and hence
    # require is to provide them in SGAtomic.cxx
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_INCLUDE_PATH})
    check_cxx_source_compiles(
       "int main() { unsigned mValue; return __sync_add_and_fetch(&mValue, 1); }"
        GCC_ATOMIC_BUILTINS_FOUND)
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   set(WARNING_FLAGS_CXX "-Wall -Wno-overloaded-virtual")
   set(WARNING_FLAGS_C   "-Wall")
endif()

if(WIN32)

    if(MINGW)
        add_definitions(-D_WIN32_WINNT=0x501)
    endif()

    if(MSVC)
        # turn off various warnings
        # foreach(warning 4244 4251 4267 4275 4290 4786 4305 4996)
        #     SET(WARNING_FLAGS "${WARNING_FLAGS} /wd${warning}")
        # endforeach(warning)
        
        set(MSVC_FLAGS "-DWIN32 -DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -D__CRT_NONSTDC_NO_WARNINGS /wd4996 /wd4250")
    endif(MSVC)
    
    # assumed on Windows
    set(HAVE_GETLOCALTIME 1)
    
    set( WINSOCK_LIBRARY "ws2_32.lib" )
    set( RT_LIBRARY "winmm" )
endif(WIN32)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS_C} ${MSVC_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS_CXX} ${MSVC_FLAGS} ${BOOST_CXX_FLAGS}")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/simgear/canvas/ShivaVG/include)
include_directories(${PROJECT_BINARY_DIR}/simgear)
include_directories(${PROJECT_BINARY_DIR}/simgear/xml)

include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIR} 
    ${OPENAL_INCLUDE_DIR} )

add_definitions(-DHAVE_CONFIG_H)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/simgear/simgear_config_cmake.h.in"
  "${PROJECT_BINARY_DIR}/simgear/simgear_config.h"
  )

configure_file (
    "${PROJECT_SOURCE_DIR}/simgear/xml/expat_config_cmake.in"
    "${PROJECT_BINARY_DIR}/simgear/xml/expat_config.h"
)

if(ENABLE_TESTS)
    # enable CTest / make test target
    message(STATUS "Tests: ENABLED")

    include (Dart)
        enable_testing()
else()
    message(STATUS "Tests: DISABLED")
endif(ENABLE_TESTS)

install (FILES ${PROJECT_BINARY_DIR}/simgear/simgear_config.h  DESTINATION include/simgear/)
add_subdirectory(simgear)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
