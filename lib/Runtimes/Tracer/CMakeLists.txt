if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

add_library(SeeCRuntimeTracer SHARED
  Tracer.cpp
)

add_library(SeeCRuntimeTracer-static
  Tracer.cpp
)

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
 ${REQ_LLVM_LIBRARIES}
)

target_link_libraries(SeeCRuntimeTracer-static
 SeeCExecutionTracer
 SeeCTrace
 SeeCUtil
 SeeCRuntimeErrors
 ${REQ_LLVM_LIBRARIES}
)

INSTALL(TARGETS SeeCRuntimeTracer SeeCRuntimeTracer-static
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

