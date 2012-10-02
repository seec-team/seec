set(HEADERS
  ../../../include/seec/Transforms/FunctionsNotInstrumented.def
  ../../../include/seec/Transforms/RecordExternal/RecordExternal.hpp
  ../../../include/seec/Transforms/RecordExternal/RecordInfo.h
  ../../../include/seec/Transforms/RecordExternal/RecordPoints.def
  )

set(SOURCES
  RecordExternal.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCRecordExternal ${HEADERS} ${SOURCES})
else()
  add_library(SeeCRecordExternal ${SOURCES})
endif()

INSTALL(TARGETS SeeCRecordExternal
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

