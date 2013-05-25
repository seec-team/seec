# Disable -Wcast-qual because wxWidgets raises many warnings.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-qual")

# wxWidgets for Cocoa uses RTTI, so we need to enable it for this library.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(HEADERS
  ../../include/seec/wxWidgets/CleanPreprocessor.h
  ../../include/seec/wxWidgets/ImageResources.hpp
  ../../include/seec/wxWidgets/StringConversion.hpp
  )

set(SOURCES
  ImageResources.cpp
  StringConversion.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCwxWidgets ${HEADERS} ${SOURCES})
else()
  add_library(SeeCwxWidgets ${SOURCES})
endif()

INSTALL(TARGETS SeeCwxWidgets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
