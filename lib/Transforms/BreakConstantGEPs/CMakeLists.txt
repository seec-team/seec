set(HEADERS
  ../../../include/seec/Transforms/BreakConstantGEPs/BreakConstantGEPs.h
  )

set(SOURCES
  BreakConstantGEPs.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCBreakConstantGEPs ${HEADERS} ${SOURCES})
else()
  add_library(SeeCBreakConstantGEPs ${SOURCES})
endif()

INSTALL(TARGETS SeeCBreakConstantGEPs
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

