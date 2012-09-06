add_library(SeeCUtil 
  Printing.cpp
  SynchronizedExit.cpp
)

INSTALL(TARGETS SeeCUtil
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

