# Disable -Wcast-qual because wxWidgets raises many warnings.
if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${WX_CXX_FLAGS} -Wno-cast-qual")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${WX_CXX_FLAGS} -Wno-cast-qual")
else ()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WX_CXX_FLAGS} -Wno-cast-qual")
endif ()

# wxWidgets for Cocoa uses RTTI, so we need to enable it for this library.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(HEADERS
  ../../include/seec/wxWidgets/AugmentResources.hpp
  ../../include/seec/wxWidgets/CallbackFSHandler.hpp
  ../../include/seec/wxWidgets/CleanPreprocessor.h
  ../../include/seec/wxWidgets/Config.hpp
  ../../include/seec/wxWidgets/ICUBundleFSHandler.hpp
  ../../include/seec/wxWidgets/ImageResources.hpp
  ../../include/seec/wxWidgets/StringConversion.hpp
  ../../include/seec/wxWidgets/XmlNodeIterator.hpp
  )

set(SOURCES
  AugmentResources.cpp
  CallbackFSHandler.cpp
  Config.cpp
  ICUBundleFSHandler.cpp
  ImageResources.cpp
  StringConversion.cpp
  )

add_library(SeeCwxWidgets ${HEADERS} ${SOURCES})

INSTALL(TARGETS SeeCwxWidgets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
