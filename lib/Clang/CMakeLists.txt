set(SEEC_CLANG_HEADERS
  ../../include/seec/Clang/Compile.hpp
  ../../include/seec/Clang/MappedAST.hpp
  ../../include/seec/Clang/MappedFile.hpp
  ../../include/seec/Clang/MappedModule.hpp
  ../../include/seec/Clang/MappedStmt.hpp
  ../../include/seec/Clang/MDNames.hpp
  ../../include/seec/Clang/RuntimeValueMapping.hpp
  ../../include/seec/Clang/Search.hpp
  ../../include/seec/Clang/SourceMapping.hpp
  ../../include/seec/Clang/StateMapping.hpp
)

set(SEEC_CLANG_SOURCES
  Compile.cpp
  MappedAST.cpp
  MappedFile.cpp
  MappedModule.cpp
  MappedStmt.cpp
  RuntimeValueMapping.cpp
  Search.cpp
  StateMapping.cpp
)

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCClang ${SEEC_CLANG_HEADERS} ${SEEC_CLANG_SOURCES})
else()
  add_library(SeeCClang ${SEEC_CLANG_SOURCES})
endif()

INSTALL(TARGETS SeeCClang
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)


set(SEEC_CLANG_MAPPED_TRACE_HEADERS
  ../../include/seec/Clang/MappedGlobalVariable.hpp
  ../../include/seec/Clang/MappedProcessState.hpp
  ../../include/seec/Clang/MappedProcessTrace.hpp
  ../../include/seec/Clang/MappedStateMovement.hpp
  ../../include/seec/Clang/MappedThreadState.hpp
  ../../include/seec/Clang/MappedValue.hpp
)

set(SEEC_CLANG_MAPPED_TRACE_SOURCES
  MappedGlobalVariable.cpp
  MappedProcessState.cpp
  MappedProcessTrace.cpp
  MappedStateMovement.cpp
  MappedThreadState.cpp
  MappedValue.cpp
)

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCClangMappedTrace ${SEEC_CLANG_MAPPED_TRACE_HEADERS} ${SEEC_CLANG_MAPPED_TRACE_SOURCES})
else()
  add_library(SeeCClangMappedTrace ${SEEC_CLANG_MAPPED_TRACE_SOURCES})
endif()

INSTALL(TARGETS SeeCClangMappedTrace
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
