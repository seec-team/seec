add_library(SeeCReplaceCStdLibIntrinsics
  ReplaceCStdLibIntrinsics.cpp
)

INSTALL(TARGETS SeeCReplaceCStdLibIntrinsics
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

