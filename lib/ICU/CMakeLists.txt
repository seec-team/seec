set(HEADERS
  ../../include/seec/ICU/Format.hpp
  ../../include/seec/ICU/Indexing.hpp
  ../../include/seec/ICU/LazyMessage.hpp
  ../../include/seec/ICU/Output.hpp
  ../../include/seec/ICU/Resources.hpp
  )

set(SOURCES
  Format.cpp
  Indexing.cpp
  LazyMessage.cpp
  Output.cpp
  Resources.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCICU ${HEADERS} ${SOURCES})
else()
  add_library(SeeCICU ${SOURCES})
endif()

INSTALL(TARGETS SeeCICU
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

