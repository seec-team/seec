add_library(SeeCRecordExternal
  RecordExternal.cpp
)

INSTALL(TARGETS SeeCRecordExternal
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

