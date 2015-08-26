set(HEADERS
  ../../../include/seec/Transforms/BreakConstantGEPs/BreakConstantGEPs.h
  )

set(SOURCES
  BreakConstantGEPs.cpp
  )

add_library(SeeCBreakConstantGEPs ${HEADERS} ${SOURCES})

INSTALL(TARGETS SeeCBreakConstantGEPs
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

