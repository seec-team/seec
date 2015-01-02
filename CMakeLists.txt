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
set(LLVM_INSTALL "/usr/local" CACHE PATH "Root of LLVM install.")
set(ICU_INSTALL "/usr/local" CACHE PATH "Root of ICU install.")
set(WX_INSTALL "/usr/local" CACHE PATH "Root of wxWidgets install.")
set(WX_TOOLCHAIN "" CACHE STRING "Build of wxWidgets to use.")
set(MULTIARCH "" CACHE STRING "Architecture to build for multiarch systems.")
option(SEEC_USER_ACTION_RECORDING "Support user action recording." OFF)

#--------------------------------------------------------------------------------
# find LLVM
#--------------------------------------------------------------------------------

# A bit of a sanity check:
if( NOT EXISTS ${LLVM_INSTALL}/include/llvm )
  message(FATAL_ERROR "LLVM_INSTALL (${LLVM_INSTALL}) is not a valid LLVM installation.")
endif()

# Now set the header paths:
message("Using LLVM install at ${LLVM_INSTALL}")
include_directories( BEFORE ${LLVM_INSTALL}/include )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${LLVM_INSTALL}/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${LLVM_INSTALL}/lib")

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --cflags
 OUTPUT_VARIABLE LLVM_C_FLAGS
)
string(STRIP ${LLVM_C_FLAGS} LLVM_C_FLAGS)

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --cxxflags
 OUTPUT_VARIABLE LLVM_CXX_FLAGS
)
string(STRIP ${LLVM_CXX_FLAGS} LLVM_CXX_FLAGS)

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --system-libs
 COMMAND tr -s " " "\n"
 COMMAND grep "\\-l"
 COMMAND tr -s "\n-" " -"
 OUTPUT_VARIABLE LLVM_LIB_DEPS
)
string(STRIP "${LLVM_LIB_DEPS}" LLVM_LIB_DEPS)
string(REPLACE "-l" "" LLVM_LIB_DEPS "${LLVM_LIB_DEPS}")
string(REPLACE " " ";" LLVM_LIB_DEPS "${LLVM_LIB_DEPS}")

message("LLVM_LIB_DEPS: ${LLVM_LIB_DEPS}")

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

execute_process(
 COMMAND ${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --cflags
 OUTPUT_VARIABLE WX_C_FLAGS
)
string(STRIP ${WX_C_FLAGS} WX_C_FLAGS)

execute_process(
 COMMAND ${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --cxxflags
 OUTPUT_VARIABLE WX_CXX_FLAGS
)
string(STRIP ${WX_CXX_FLAGS} WX_CXX_FLAGS)


#--------------------------------------------------------------------------------
# setup compiler options
#--------------------------------------------------------------------------------

if (SEEC_USER_ACTION_RECORDING)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSEEC_USER_ACTION_RECORDING")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSEEC_USER_ACTION_RECORDING")
endif (SEEC_USER_ACTION_RECORDING)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LLVM_C_FLAGS} -Wall -Wno-strict-aliasing -std=c99 -DWXINTL_NO_GETTEXT_MACRO")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS} -Wall -Wno-strict-aliasing -std=c++11 -DWXINTL_NO_GETTEXT_MACRO")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")

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
                     COMMAND ${ICU_INSTALL}/sbin/icupkg
                     ARGS -d ${${package}_res_dir} new ${package}.dat
                     COMMENT "icupkg ${package}.dat"
                     )

  # build individual resources using genrb
  foreach(resource ${ARGN})
    # destination resource (.res file)
    set(${package}_icu_${resource}_res ${${package}_res_dir}/${resource}.res)

    # genrb command to build the resource
    add_custom_command(OUTPUT ${${package}_icu_${resource}_res}
                       COMMAND ${ICU_INSTALL}/bin/genrb
                       ARGS -e UTF-8 -d ${${package}_res_dir} ${CMAKE_SOURCE_DIR}/resources/${package}/${resource}.txt
                       DEPENDS ${CMAKE_SOURCE_DIR}/resources/${package}/${resource}.txt
                       COMMENT "genrb ${resource}"
                       )

    # resource stamp file (fake output for icupkg for dependencies)
    set(${package}_icu_${resource}_stamp ${${package}_res_dir}/${resource}.stamp)

    # icupkg command to add resource to package
    add_custom_command(OUTPUT ${${package}_icu_${resource}_stamp}
                       COMMAND ${ICU_INSTALL}/sbin/icupkg
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
  message("Build: Debug")
  message("C flags: ${CMAKE_C_FLAGS_DEBUG}")
  message("C++ flags: ${CMAKE_CXX_FLAGS_DEBUG}")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  message("Build: Release")
  message("C flags: ${CMAKE_C_FLAGS_RELEASE}")
  message("C++ flags: ${CMAKE_CXX_FLAGS_RELEASE}")  
else ()
  message("Build: Default")
  message("C flags: ${CMAKE_C_FLAGS}")
  message("C++ flags: ${CMAKE_CXX_FLAGS}")
endif ()

message("LD flags (bin): ${CMAKE_EXE_LINKER_FLAGS}")
message("LD flags (so): ${CMAKE_SHARED_LINKER_FLAGS}")

message("Will build SeeC resources.")
add_subdirectory(resources)

message("Will build SeeC libraries.")
add_subdirectory(lib)

option(SEEC_BUILD_TOOLS "Build the SeeC tools." ON)
if(SEEC_BUILD_TOOLS)
  message("Will build SeeC tools.")
  add_subdirectory(tools)
endif(SEEC_BUILD_TOOLS)

