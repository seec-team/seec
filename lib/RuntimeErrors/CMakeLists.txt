add_library(SeeCRuntimeErrors 
  ArgumentTypes.cpp
  FormatSelects.cpp
  RuntimeErrors.cpp
  UnicodeFormatter.cpp
)

INSTALL(TARGETS SeeCRuntimeErrors
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

