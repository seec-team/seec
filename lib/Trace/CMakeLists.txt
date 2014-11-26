# Disable -Wcast-qual because wxWidgets raises many warnings.
if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${WX_CXX_FLAGS} -Wno-cast-qual")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${WX_CXX_FLAGS} -Wno-cast-qual")
else ()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WX_CXX_FLAGS} -Wno-cast-qual")
endif ()

set(TRACE_HEADERS
  ../../include/seec/Trace/Events.def
  ../../include/seec/Trace/FunctionState.hpp
  ../../include/seec/Trace/GetCurrentRuntimeValue.hpp
  ../../include/seec/Trace/GetRecreatedValue.hpp
  ../../include/seec/Trace/MemoryState.hpp
  ../../include/seec/Trace/ProcessState.hpp
  ../../include/seec/Trace/StateMovement.hpp
  ../../include/seec/Trace/StreamState.hpp
  ../../include/seec/Trace/RuntimeValue.hpp
  ../../include/seec/Trace/ThreadState.hpp
  ../../include/seec/Trace/TraceFormat.hpp
  ../../include/seec/Trace/TracePointer.hpp
  ../../include/seec/Trace/TraceReader.hpp
  ../../include/seec/Trace/TraceSearch.hpp
  ../../include/seec/Trace/TraceStorage.hpp
  )

set(TRACE_SOURCES
  FunctionState.cpp
  GetRecreatedValue.cpp
  MemoryState.cpp
  ProcessState.cpp
  StateMovement.cpp
  StreamState.cpp
  ThreadState.cpp
  TraceFormat.cpp
  TracePointer.cpp
  TraceReader.cpp
  TraceStorage.cpp
)

set(EXECUTION_TRACER_HEADERS
  ../../include/seec/Trace/DetectCalls.hpp
  ../../include/seec/Trace/DetectCallsAll.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCctype.def
  ../../include/seec/Trace/DetectCalls/DetectCallsClocale.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCmath.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCsignal.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCstdio.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCstdlib.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCstring.def
  ../../include/seec/Trace/DetectCalls/DetectCallsCtime.def
  ../../include/seec/Trace/DetectCallsLookup.hpp
  ../../include/seec/Trace/TracedFunction.hpp
  ../../include/seec/Trace/TraceEventWriter.hpp
  ../../include/seec/Trace/TraceMemory.hpp
  ../../include/seec/Trace/TraceProcessListener.hpp
  ../../include/seec/Trace/TraceStreams.hpp
  ../../include/seec/Trace/TraceThreadListener.hpp
  ../../include/seec/Trace/TraceThreadMemCheck.hpp
  ../../include/seec/Trace/FormatSpecifiers.hpp
  ../../include/seec/Trace/PrintFormatSpecifiers.def
  ../../include/seec/Trace/PrintFormatSpecifiers.hpp
  ../../include/seec/Trace/ScanFormatSpecifiers.def
  ../../include/seec/Trace/ScanFormatSpecifiers.hpp
  )

set(EXECUTION_TRACER_SOURCES
  DetectCallsLookup.cpp
  PrintFormatSpecifiers.cpp
  ScanFormatSpecifiers.cpp
  TracedFunction.cpp
  TraceMemory.cpp
  TraceProcessListener.cpp
  TraceStreams.cpp
  TraceThreadListener.cpp
  TraceThreadListenerDetectCalls.cpp
  TraceThreadListenerNotify.cpp
  TraceThreadMemCheck.cpp
  )

add_library(SeeCTrace ${TRACE_HEADERS} ${TRACE_SOURCES})
add_library(SeeCExecutionTracer ${EXECUTION_TRACER_HEADERS} ${EXECUTION_TRACER_SOURCES})

INSTALL(TARGETS SeeCTrace SeeCExecutionTracer
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

