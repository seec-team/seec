add_library(SeeCRecordInternal
  RecordInternal.cpp
)

INSTALL(TARGETS SeeCRecordInternal
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

