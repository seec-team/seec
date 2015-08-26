set(HEADERS
  ../../include/seec/ICU/Augmenter.hpp
  ../../include/seec/ICU/Format.hpp
  ../../include/seec/ICU/Indexing.hpp
  ../../include/seec/ICU/LazyMessage.hpp
  ../../include/seec/ICU/LineWrapper.hpp
  ../../include/seec/ICU/Output.hpp
  ../../include/seec/ICU/Resources.hpp
  )

set(SOURCES
  Augmenter.cpp
  Format.cpp
  Indexing.cpp
  LazyMessage.cpp
  LineWrapper.cpp
  Output.cpp
  Resources.cpp
  )

add_library(SeeCICU ${HEADERS} ${SOURCES})

INSTALL(TARGETS SeeCICU
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

