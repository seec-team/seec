set(TRACE_HEADERS
  ../../include/seec/Trace/Events.def
  ../../include/seec/Trace/FunctionState.hpp
  ../../include/seec/Trace/MemoryState.hpp
  ../../include/seec/Trace/ProcessState.hpp
  ../../include/seec/Trace/StateMovement.hpp
  ../../include/seec/Trace/RuntimeValue.hpp
  ../../include/seec/Trace/ThreadState.hpp
  ../../include/seec/Trace/TraceFormat.hpp
  ../../include/seec/Trace/TraceReader.hpp
  ../../include/seec/Trace/TraceSearch.hpp
  ../../include/seec/Trace/TraceStorage.hpp
  )

set(TRACE_SOURCES
  FunctionState.cpp
  MemoryState.cpp
  ProcessState.cpp
  StateMovement.cpp
  ThreadState.cpp
  TraceFormat.cpp
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
  ../../include/seec/Trace/GetCurrentRuntimeValue.hpp
  ../../include/seec/Trace/TracedFunction.hpp
  ../../include/seec/Trace/TraceEventWriter.hpp
  ../../include/seec/Trace/TraceMemory.hpp
  ../../include/seec/Trace/TraceProcessListener.hpp
  ../../include/seec/Trace/TraceStreams.hpp
  ../../include/seec/Trace/TraceThreadListener.hpp
  ../../include/seec/Trace/TraceThreadMemCheck.hpp
  FormatSpecifiers.hpp
  PrintFormatSpecifiers.def
  PrintFormatSpecifiers.hpp
  ScanFormatSpecifiers.def
  ScanFormatSpecifiers.hpp
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

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCTrace ${TRACE_HEADERS} ${TRACE_SOURCES})
  add_library(SeeCExecutionTracer ${EXECUTION_TRACER_HEADERS} ${EXECUTION_TRACER_SOURCES})
else()
  add_library(SeeCTrace ${TRACE_SOURCES})
  add_library(SeeCExecutionTracer ${EXECUTION_TRACER_SOURCES})
endif()

INSTALL(TARGETS SeeCTrace SeeCExecutionTracer
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

