set(HEADERS
  ../../include/seec/DSA/Interval.hpp
  ../../include/seec/DSA/IntervalMapVector.hpp
  ../../include/seec/DSA/MappedMemoryBlock.hpp
  ../../include/seec/DSA/MemoryArea.hpp
  )

add_custom_target(SeeCDSA SOURCES ${HEADERS})
