add_library(SeeCwxWidgets 
  StringConversion.cpp
)

INSTALL(TARGETS SeeCwxWidgets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

