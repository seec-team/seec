add_library(SeeCTrace
  FunctionState.cpp
  MemoryState.cpp
  ProcessState.cpp
  ThreadState.cpp
  TraceFormat.cpp
  TraceReader.cpp
)

add_library(SeeCExecutionTracer
  DetectCallsLookup.cpp
  TracedFunction.cpp
  TraceMemory.cpp
  TraceProcessListener.cpp
  TraceThreadListener.cpp
  TraceThreadListenerDetectCalls.cpp
  TraceThreadListenerNotify.cpp
  TraceThreadMemCheck.cpp
)

INSTALL(TARGETS SeeCTrace SeeCExecutionTracer
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

