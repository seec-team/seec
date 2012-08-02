# wxWidgets for Cocoa uses RTTI, so we need to enable it for this library.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

add_library(SeeCwxWidgets 
  StringConversion.cpp
)

INSTALL(TARGETS SeeCwxWidgets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
