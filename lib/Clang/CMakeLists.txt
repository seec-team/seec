add_library(SeeCClang 
  Compile.cpp
  MappedAST.cpp
  StateMapping.cpp
)

INSTALL(TARGETS SeeCClang
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

