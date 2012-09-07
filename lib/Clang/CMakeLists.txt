add_library(SeeCClang 
  Compile.cpp
  MappedAST.cpp
  RuntimeValueMapping.cpp
  StateMapping.cpp
)

INSTALL(TARGETS SeeCClang
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

