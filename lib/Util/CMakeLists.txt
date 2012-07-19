add_library(SeeCUtil 
  GenericValueHandling.cpp
  Printing.cpp
  SynchronizedExit.cpp
)

INSTALL(TARGETS SeeCUtil
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

