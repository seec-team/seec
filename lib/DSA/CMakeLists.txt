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

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCDSA ${HEADERS} ${SOURCES})
else()
  add_library(SeeCDSA ${SOURCES})
endif()

INSTALL(TARGETS SeeCDSA
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
