set(HEADERS
  ../../include/seec/DSA/Interval.hpp
  ../../include/seec/DSA/IntervalMapVector.hpp
  ../../include/seec/DSA/MappedMemoryBlock.hpp
  ../../include/seec/DSA/MemoryArea.hpp
  ../../include/seec/DSA/MemoryBlock.hpp
  )

set(SOURCES
  MemoryBlock.cpp
  )

add_library(SeeCDSA ${HEADERS} ${SOURCES})

INSTALL(TARGETS SeeCDSA
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
