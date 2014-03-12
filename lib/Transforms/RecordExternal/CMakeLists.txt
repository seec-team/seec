set(HEADERS
  ../../../include/seec/Transforms/FunctionsHandled.def
  ../../../include/seec/Transforms/FunctionsNotInstrumented.def
  ../../../include/seec/Transforms/RecordExternal/RecordExternal.hpp
  ../../../include/seec/Transforms/RecordExternal/RecordInfo.h
  ../../../include/seec/Transforms/RecordExternal/RecordPoints.def
  )

set(SOURCES
  RecordExternal.cpp
  )

add_library(SeeCRecordExternal ${HEADERS} ${SOURCES})

INSTALL(TARGETS SeeCRecordExternal
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

