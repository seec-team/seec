set(HEADERS
  ../../../include/seec/Transforms/ReplaceCStdLibIntrinsics/CStdLibIntrinsics.def
  ../../../include/seec/Transforms/ReplaceCStdLibIntrinsics/ReplaceCStdLibIntrinsics.hpp
  )

set(SOURCES
  ReplaceCStdLibIntrinsics.cpp
  )

add_library(SeeCReplaceCStdLibIntrinsics ${HEADERS} ${SOURCES})

INSTALL(TARGETS SeeCReplaceCStdLibIntrinsics
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

