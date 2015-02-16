set (RUNTIME_TRACER_VERSION_MAJOR "1")
set (RUNTIME_TRACER_VERSION_MINOR "0")
set (RUNTIME_TRACER_VERSION "${RUNTIME_TRACER_VERSION_MAJOR}.${RUNTIME_TRACER_VERSION_MINOR}")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(HEADERS
  ../../../include/seec/Runtimes/MangleFunction.h
  ../../../include/seec/Runtimes/Tracer/InterceptedFunctions.def
  PrintRunError.hpp
  SimpleWrapper.hpp
  Tracer.hpp
  )

set(SOURCES
  PrintRunError.cpp
  Tracer.cpp
  WrapCerrno_h.cpp
  WrapCfenv_h.cpp
  WrapClocale_h.cpp
  WrapCmath_h.cpp
  WrapCstdio_h.cpp
  WrapCstdlib_h.cpp
  WrapCstring_h.cpp
  WrapCtime_h.cpp
  WrapPOSIXdirent_h.cpp
  WrapPOSIXpthread_h.cpp
  WrapPOSIXsys_stat_h.cpp
  WrapPOSIXsys_time_h.cpp
  WrapPOSIXsys_wait_h.cpp
  WrapPOSIXunistd_h.cpp
  )

add_library(seecRuntimeTracer SHARED ${HEADERS} ${SOURCES})

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (LLVM)
#--------------------------------------------------------------------------------
llvm_map_components_to_libnames(REQ_LLVM_LIBRARIES bitreader asmparser target jit mcparser support core option)

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (ICU)
#--------------------------------------------------------------------------------
execute_process(
 COMMAND ${ICU_INSTALL}/bin/icu-config --prefix=${ICU_INSTALL} --ldflags-libsonly
 OUTPUT_VARIABLE REQ_ICU_LIBRARIES
)
string(STRIP ${REQ_ICU_LIBRARIES} REQ_ICU_LIBRARIES)
string(REPLACE "-l" "" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
string(REPLACE " " ";" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (WX)
#--------------------------------------------------------------------------------
execute_process(
 COMMAND ${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --libs base xml
 OUTPUT_VARIABLE REQ_WX_LIBRARIES
)
string(STRIP ${REQ_WX_LIBRARIES} REQ_WX_LIBRARIES)

target_link_libraries(seecRuntimeTracer
 SeeCClang
 SeeCExecutionTracer
 SeeCTrace
 SeeCUtil
 SeeCRuntimeErrors
 SeeCICU
 SeeCwxWidgets

 ${REQ_WX_LIBRARIES}

 # Clang libraries
 ${LLVM_INSTALL}/lib/libclangFrontend.a
 ${LLVM_INSTALL}/lib/libclangDriver.a
 ${LLVM_INSTALL}/lib/libclangSerialization.a
 ${LLVM_INSTALL}/lib/libclangParse.a
 ${LLVM_INSTALL}/lib/libclangSema.a
 ${LLVM_INSTALL}/lib/libclangEdit.a
 ${LLVM_INSTALL}/lib/libclangAnalysis.a
 ${LLVM_INSTALL}/lib/libclangLex.a
 ${LLVM_INSTALL}/lib/libclangAST.a
 ${LLVM_INSTALL}/lib/libclangBasic.a

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

