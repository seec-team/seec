cmake_minimum_required(VERSION 2.8)
project(SeeC)

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
set(GV_INSTALL "/usr" CACHE PATH "Root of graphviz install.") 
set(GV_LIBS "" CACHE STRING "Set the graphviz libraries to link with.")

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

# Copy Clang resources into our resources build directory:
file(COPY ${LLVM_INSTALL}/lib/clang DESTINATION ${CMAKE_BINARY_DIR}/resources/)

INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/resources/clang DESTINATION lib/seec/resources)

#--------------------------------------------------------------------------------
# find ICU
#--------------------------------------------------------------------------------
if( NOT EXISTS ${ICU_INSTALL}/include/unicode/unistr.h )
  message(FATAL_ERROR "ICU_INSTALL (${ICU_INSTALL}) is not a valid ICU installation.")
endif()

message("Using ICU at ${ICU_INSTALL}")
include_directories( BEFORE ${ICU_INSTALL}/include )

#--------------------------------------------------------------------------------
# find wxWidgets
#--------------------------------------------------------------------------------
if( NOT EXISTS ${WX_INSTALL}/bin/wx-config )
  message(FATAL_ERROR "WX_INSTALL (${WX_INSTALL}) is not a valid wxWidgets installation.")
endif()

execute_process(
 COMMAND ${WX_INSTALL}/bin/wx-config --prefix=${WX_INSTALL} --cflags
 OUTPUT_VARIABLE WX_C_FLAGS
)
string(STRIP ${WX_C_FLAGS} WX_C_FLAGS)

execute_process(
 COMMAND ${WX_INSTALL}/bin/wx-config --prefix=${WX_INSTALL} --cxxflags
 OUTPUT_VARIABLE WX_CXX_FLAGS
)
string(STRIP ${WX_CXX_FLAGS} WX_CXX_FLAGS)

message("WX_C_FLAGS ${WX_C_FLAGS}")
message("WX_CXX_FLAGS ${WX_CXX_FLAGS}")

#--------------------------------------------------------------------------------
# find graphviz
#--------------------------------------------------------------------------------
if( NOT EXISTS ${GV_INSTALL}/include/graphviz/gvc.h )
  message(FATAL_ERROR "GV_INSTALL (${GV_INSTALL}) is not a valid Graphviz installation.")
endif()

message("Using Graphviz at ${GV_INSTALL}")
include_directories( BEFORE ${GV_INSTALL}/include/graphviz )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${GV_INSTALL}/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${GV_INSTALL}/lib")

if (GV_LIBS)
  message("Using user-specified GV_LIBS: ${GV_LIBS}")
  
  set(REQ_GVC_LIBRARIES "")
  
  FOREACH (LIB ${GV_LIBS})
     list(APPEND REQ_GVC_LIBRARIES ${LIB})
  ENDFOREACH (LIB)
else()
  message("Getting graphviz libraries from libgvc.pc")
  
  execute_process(
    COMMAND cat ${GV_INSTALL}/lib/pkgconfig/libgvc.pc
    COMMAND grep "Libs:"
    COMMAND tr " \t" "\n"
    COMMAND grep "\\-l"
    COMMAND tr "\n" " "
    OUTPUT_VARIABLE REQ_GVC_LIBRARIES
  )
  
  string(STRIP ${REQ_GVC_LIBRARIES} REQ_GVC_LIBRARIES)
  string(REPLACE "-l" "" REQ_GVC_LIBRARIES ${REQ_GVC_LIBRARIES})
  string(REPLACE " " ";" REQ_GVC_LIBRARIES ${REQ_GVC_LIBRARIES})
endif()

message("Graphviz libraries: ${REQ_GVC_LIBRARIES}")

#--------------------------------------------------------------------------------
# setup compiler options
#--------------------------------------------------------------------------------

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
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --ldflags
 OUTPUT_VARIABLE LLVM_LD_FLAGS
)
string(STRIP ${LLVM_LD_FLAGS} LLVM_LD_FLAGS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LLVM_C_FLAGS} -Wall -std=c99 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXX_FLAGS} -Wall -std=c++11 -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LD_FLAGS} -L ${ICU_INSTALL}/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L ${ICU_INSTALL}/lib")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

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
                       ARGS -d ${${package}_res_dir} ${CMAKE_SOURCE_DIR}/resources/${package}/${resource}.txt
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

message("Using C flags: ${CMAKE_C_FLAGS}")
message("Using C++ flags: ${CMAKE_CXX_FLAGS}")

message("Will build SeeC resources.")
add_subdirectory(resources)

message("Will build SeeC libraries.")
add_subdirectory(lib)

option(SEEC_BUILD_TOOLS "Build the SeeC tools." ON)
if(SEEC_BUILD_TOOLS)
  message("Will build SeeC tools.")
  add_subdirectory(tools)
endif(SEEC_BUILD_TOOLS)

