set (RUNTIME_TRACER_VERSION_MAJOR "1")
set (RUNTIME_TRACER_VERSION_MINOR "0")
set (RUNTIME_TRACER_VERSION "${RUNTIME_TRACER_VERSION_MAJOR}.${RUNTIME_TRACER_VERSION_MINOR}")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(HEADERS
  ../../../include/seec/Runtimes/MangleFunction.h
  ../../../include/seec/Runtimes/Tracer/InterceptedFunctions.def
  SimpleWrapper.hpp
  Tracer.hpp
  )

set(SOURCES
  Tracer.cpp
  WrapCfenv_h.cpp
  WrapClocale_h.cpp
  WrapCstdio_h.cpp
  WrapCstdlib_h.cpp
  WrapCstring_h.cpp
  WrapCtime_h.cpp
  WrapPOSIXdirent_h.cpp
  WrapPOSIXsys_stat_h.cpp
  WrapPOSIXsys_time_h.cpp
  WrapPOSIXsys_wait_h.cpp
  WrapPOSIXunistd_h.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
 add_library(seecRuntimeTracer SHARED ${HEADERS} ${SOURCES})
else()
 add_library(seecRuntimeTracer SHARED ${SOURCES})
endif()

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles bitreader asmparser target jit support core
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

execute_process(
 COMMAND ${ICU_INSTALL}/bin/icu-config --prefix=${ICU_INSTALL} --ldflags-libsonly
 OUTPUT_VARIABLE REQ_ICU_LIBRARIES
)
string(STRIP ${REQ_ICU_LIBRARIES} REQ_ICU_LIBRARIES)
string(REPLACE "-l" "" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
string(REPLACE " " ";" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})

target_link_libraries(seecRuntimeTracer
 SeeCExecutionTracer
 SeeCTrace
 SeeCUtil
 SeeCRuntimeErrors
 SeeCICU
 ${REQ_LLVM_LIBRARIES}
 ${REQ_ICU_LIBRARIES}
 ${LLVM_LIB_DEPS}
)

SET_TARGET_PROPERTIES(
  seecRuntimeTracer
  PROPERTIES
    VERSION ${RUNTIME_TRACER_VERSION}
    SOVERSION ${RUNTIME_TRACER_VERSION}
    INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

INSTALL(TARGETS seecRuntimeTracer
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

