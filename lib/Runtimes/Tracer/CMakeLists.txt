if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(HEADERS
  ../../../include/seec/Runtimes/MangleFunction.h
  ../../../include/seec/Runtimes/Tracer/InterceptedFunctions.def
  Tracer.hpp
  )

set(SOURCES
  Tracer.cpp
  WrapCstdio_h.cpp
  WrapCstring_h.cpp
  WrapPOSIXsys_wait_h.cpp
  WrapPOSIXunistd_h.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
 add_library(SeeCRuntimeTracer SHARED ${HEADERS} ${SOURCES})
 add_library(SeeCRuntimeTracer-static ${HEADERS} ${SOURCES})
else()
 add_library(SeeCRuntimeTracer SHARED ${SOURCES})
 add_library(SeeCRuntimeTracer-static ${SOURCES})
endif()

SET_TARGET_PROPERTIES(SeeCRuntimeTracer-static PROPERTIES OUTPUT_NAME "SeeCRuntimeTracer")
SET_TARGET_PROPERTIES(SeeCRuntimeTracer-static PROPERTIES PREFIX "lib")

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles bitreader asmparser target jit support core
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

target_link_libraries(SeeCRuntimeTracer
 SeeCExecutionTracer
 SeeCTrace
 SeeCUtil
 SeeCRuntimeErrors
 SeeCICU
 ${REQ_LLVM_LIBRARIES}
)

target_link_libraries(SeeCRuntimeTracer-static
 SeeCExecutionTracer
 SeeCTrace
 SeeCUtil
 SeeCRuntimeErrors
 SeeCICU
 ${REQ_LLVM_LIBRARIES}
)

INSTALL(TARGETS SeeCRuntimeTracer SeeCRuntimeTracer-static
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

