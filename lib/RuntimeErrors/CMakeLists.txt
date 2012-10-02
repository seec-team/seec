set(HEADERS
  ../../include/seec/RuntimeErrors/ArgumentTypes.def
  ../../include/seec/RuntimeErrors/ArgumentTypes.hpp
  ../../include/seec/RuntimeErrors/FormatSelects.def
  ../../include/seec/RuntimeErrors/FormatSelects.hpp
  ../../include/seec/RuntimeErrors/RuntimeErrors.def
  ../../include/seec/RuntimeErrors/RuntimeErrors.hpp
  ../../include/seec/RuntimeErrors/UnicodeFormatter.hpp
  )

set(SOURCES
  ArgumentTypes.cpp
  FormatSelects.cpp
  RuntimeErrors.cpp
  UnicodeFormatter.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCRuntimeErrors ${HEADERS} ${SOURCES})
else()
  add_library(SeeCRuntimeErrors ${SOURCES})
endif()

INSTALL(TARGETS SeeCRuntimeErrors
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

