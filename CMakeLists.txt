cmake_minimum_required(VERSION 2.8)

IF(DEFINED CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: None Debug Release.")
ELSE()
   SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: None Debug Release.")
ENDIF()

project(seec)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

#--------------------------------------------------------------------------------
# Cached variables.
#--------------------------------------------------------------------------------
set(BUNDLE_PREFIX "/Applications" CACHE PATH "Where to install application bundles (Mac OS X).")
set(LLVM_INSTALL_OVERRIDE "" CACHE PATH "Override LLVM install location.")
set(ICU_INSTALL "/usr/local" CACHE PATH "Root of ICU install.")
set(WX_INSTALL "/usr/local" CACHE PATH "Root of wxWidgets install.")
set(WX_TOOLCHAIN "" CACHE STRING "Build of wxWidgets to use.")
set(MULTIARCH "" CACHE STRING "Architecture to build for multiarch systems.")
option(SEEC_USER_ACTION_RECORDING "Support user action recording." OFF)

# find MULTIARCH automatically if we can.
if(MULTIARCH STREQUAL "")
  if(EXISTS /usr/bin/dpkg-architecture)
    execute_process(COMMAND /usr/bin/dpkg-architecture -qDEB_HOST_MULTIARCH
                    OUTPUT_VARIABLE THE_MULTIARCH)
    string(STRIP ${THE_MULTIARCH} MULTIARCH)
  endif(EXISTS /usr/bin/dpkg-architecture)
endif(MULTIARCH STREQUAL "")

#--------------------------------------------------------------------------------
# find LLVM
#--------------------------------------------------------------------------------

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(LLVM_INSTALL "${LLVM_DIR}/../../../")

if(NOT "${LLVM_INSTALL_OVERRIDE}" STREQUAL "")
  message(STATUS "Using LLVM_INSTALL_OVERRIDE ${LLVM_INSTALL_OVERRIDE}")
  set(LLVM_INSTALL "${LLVM_INSTALL_OVERRIDE}")
endif()

include_directories("${LLVM_INSTALL}/include")
message(STATUS "LLVM include ${LLVM_INSTALL}/include")

add_definitions(${LLVM_DEFINITIONS})

# Copy Clang resources into our resources build directory:
file(COPY ${LLVM_INSTALL}/lib/clang DESTINATION ${CMAKE_BINARY_DIR}/resources/)

INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/resources/clang DESTINATION lib/seec/resources)

#--------------------------------------------------------------------------------
# find ICU
#--------------------------------------------------------------------------------
if( NOT EXISTS ${ICU_INSTALL}/include/${MULTIARCH}/unicode/unistr.h )
  if ( NOT EXISTS ${ICU_INSTALL}/include/unicode/unistr.h )
    message(FATAL_ERROR "ICU_INSTALL (${ICU_INSTALL}) is not a valid ICU installation.")
  else()
    message("Using ICU at ${ICU_INSTALL}")
    include_directories( BEFORE ${ICU_INSTALL}/include )
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${ICU_INSTALL}/lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${ICU_INSTALL}/lib")
  endif()
else()
  message("Using ICU at ${ICU_INSTALL} for architecture ${MULTIARCH}")
  include_directories( BEFORE ${ICU_INSTALL}/include/${MULTIARCH} )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${ICU_INSTALL}/lib/${MULTIARCH}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${ICU_INSTALL}/lib/${MULTIARCH}")
endif()

if(EXISTS ${ICU_INSTALL}/bin64)
  set(ICU_GENRB "${ICU_INSTALL}/bin64/genrb")
  set(ICU_ICUPKG "${ICU_INSTALL}/bin64/icupkg")
else()
  set(ICU_GENRB "${ICU_INSTALL}/bin/genrb")
  set(ICU_ICUPKG "${ICU_INSTALL}/sbin/icupkg")
endif(EXISTS ${ICU_INSTALL}/bin64)

#--------------------------------------------------------------------------------
# find wxWidgets
#--------------------------------------------------------------------------------
if( (NOT "${WX_TOOLCHAIN}" STREQUAL "") AND (EXISTS ${WX_INSTALL}/lib/wx/config/${WX_TOOLCHAIN}) )
  set(WX_CONFIG_BIN "${WX_INSTALL}/lib/wx/config/${WX_TOOLCHAIN}")
else()
  if( EXISTS ${WX_INSTALL}/bin/wx-config )
    set(WX_CONFIG_BIN "${WX_INSTALL}/bin/wx-config")
  endif()
endif()

if( NOT EXISTS ${WX_CONFIG_BIN} )
  message(FATAL_ERROR "WX_INSTALL (${WX_INSTALL}) is not a valid wxWidgets installation.")
endif()

message("using WX_CONFIG_BIN at ${WX_CONFIG_BIN}")

EXEC_PROGRAM(sh
 ARGS "${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --cflags"
 OUTPUT_VARIABLE WX_C_FLAGS
)
string(STRIP ${WX_C_FLAGS} WX_C_FLAGS)

EXEC_PROGRAM(sh
 ARGS "${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --cxxflags"
 OUTPUT_VARIABLE WX_CXX_FLAGS
)
string(STRIP ${WX_CXX_FLAGS} WX_CXX_FLAGS)


#--------------------------------------------------------------------------------
# setup compiler options
#--------------------------------------------------------------------------------

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if (SEEC_USER_ACTION_RECORDING)
  add_definitions(-DSEEC_USER_ACTION_RECORDING)
endif (SEEC_USER_ACTION_RECORDING)

add_definitions(-DWXINTL_NO_GETTEXT_MACRO)

# needed when compiling with mingw-w64 under msys2
set(SEEC_CXX_STD "-std=c++11")
if (WIN32)
  set(SEEC_CXX_STD "-std=gnu++11")
endif (WIN32)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LLVM_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-strict-aliasing -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-strict-aliasing ${SEEC_CXX_STD} -fvisibility-inlines-hidden")

if (NOT ${LLVM_ENABLE_RTTI})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif (NOT ${LLVM_ENABLE_RTTI})

if (NOT ${LLVM_ENABLE_EH})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
endif (NOT ${LLVM_ENABLE_EH})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -O0")

set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")

#--------------------------------------------------------------------------------
# Set build variables
#--------------------------------------------------------------------------------
set_property(GLOBAL PROPERTY SEEC_RESOURCE_FILES "")

#--------------------------------------------------------------------------------
# macro for generating ICU resources
# adapted from: http://www.cmake.org/pipermail/cmake/2009-March/027433.html
# note that, for icupkg to work correctly, resources must be specified BEFORE
# any other resource that they depend upon. e.g. en_AU en root.
#--------------------------------------------------------------------------------


macro(icu_resources package)
  # build directory for this package's resources
  set(${package}_res_dir ${CMAKE_BINARY_DIR}/resources/${package})

  # destination package (.dat file)
  set(${package}_icu_dat ${${package}_res_dir}/${package}.dat)

  # create empty package if necessary
  add_custom_command(OUTPUT ${${package}_icu_dat}
                     COMMAND ${ICU_ICUPKG}
                     ARGS -d ${${package}_res_dir} new ${package}.dat
                     COMMENT "icupkg ${package}.dat"
                     )

  # build individual resources using genrb
  foreach(resource ${ARGN})
    # destination resource (.res file)
    set(${package}_icu_${resource}_res ${${package}_res_dir}/${resource}.res)

    # genrb command to build the resource
    add_custom_command(OUTPUT ${${package}_icu_${resource}_res}
                       COMMAND ${ICU_GENRB}
                       ARGS -e UTF-8 -d ${${package}_res_dir} -s ${CMAKE_SOURCE_DIR}/resources/${package} ${resource}.txt
                       DEPENDS ${CMAKE_SOURCE_DIR}/resources/${package}/${resource}.txt
                       COMMENT "genrb ${resource} : -e UTF-8 -d ${${package}_res_dir} ${CMAKE_SOURCE_DIR}/resources/${package}/${resource}.txt"
                       )

    # resource stamp file (fake output for icupkg for dependencies)
    set(${package}_icu_${resource}_stamp ${${package}_res_dir}/${resource}.stamp)

    # icupkg command to add resource to package
    add_custom_command(OUTPUT ${${package}_icu_${resource}_stamp}
                       COMMAND ${ICU_ICUPKG}
                       ARGS -d ${${package}_res_dir} -a ${resource}.res ${package}.dat ${package}.dat
                       COMMAND "${CMAKE_COMMAND}" -E touch "${${package}_icu_${resource}_stamp}"
                       DEPENDS ${${package}_icu_${resource}_res} ${${package}_icu_dat}
                       COMMENT "icupkg ${resource} into ${package}"
                       )

    # add the resource stamp file to this package's list of stamps
    list(APPEND ${package}_icu_stamps ${${package}_icu_${resource}_stamp})
  endforeach(resource)

  # custom target to require packaging each resource (depends on stamp files)
  add_custom_target(icu_resources_${package} ALL DEPENDS ${${package}_icu_stamps})

  # add the package (.dat file) to the list of resource files.
  # list(APPEND SEEC_RESOURCE_FILES ${${package}_icu_dat})

  set_property(GLOBAL APPEND PROPERTY SEEC_RESOURCE_FILES "${${package}_icu_dat}")

  # install the package (.dat file)
  INSTALL(FILES ${${package}_icu_dat} DESTINATION lib/seec/resources)
endmacro(icu_resources)

#--------------------------------------------------------------------------------
# Setup the build for the various SeeC components
#--------------------------------------------------------------------------------
include_directories( BEFORE ${PROJECT_SOURCE_DIR}/include )

if (CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Build: Debug")
  message(STATUS "C flags: ${CMAKE_C_FLAGS_DEBUG}")
  message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS_DEBUG}")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Build: Release")
  message(STATUS "C flags: ${CMAKE_C_FLAGS_RELEASE}")
  message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS_RELEASE}")
else ()
  message(STATUS "Build: Default")
  message(STATUS "C flags: ${CMAKE_C_FLAGS}")
  message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
endif ()

message(STATUS "LD flags (bin): ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "LD flags (so): ${CMAKE_SHARED_LINKER_FLAGS}")

message(STATUS "Will build SeeC resources.")
add_subdirectory(resources)

message(STATUS "Will build SeeC libraries.")
add_subdirectory(lib)

option(SEEC_BUILD_TOOLS "Build the SeeC tools." ON)
if(SEEC_BUILD_TOOLS)
  message(STATUS "Will build SeeC tools.")
  add_subdirectory(tools)
endif(SEEC_BUILD_TOOLS)

