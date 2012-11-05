set(HEADERS
  ../../include/seec/Clang/Compile.hpp
  ../../include/seec/Clang/MappedAST.hpp
  ../../include/seec/Clang/MappedFile.hpp
  ../../include/seec/Clang/MappedStmt.hpp
  ../../include/seec/Clang/RuntimeValueMapping.hpp
  ../../include/seec/Clang/StateMapping.hpp
  )

set(SOURCES
  Compile.cpp
  MappedAST.cpp
  MappedFile.cpp
  MappedStmt.cpp
  RuntimeValueMapping.cpp
  StateMapping.cpp
)

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCClang ${HEADERS} ${SOURCES})
else()
  add_library(SeeCClang ${SOURCES})
endif()

INSTALL(TARGETS SeeCClang
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
