set(HEADERS
  ../../../include/seec/Transforms/ReplaceCStdLibIntrinsics/CStdLibIntrinsics.def
  ../../../include/seec/Transforms/ReplaceCStdLibIntrinsics/ReplaceCStdLibIntrinsics.hpp
  )

set(SOURCES
  ReplaceCStdLibIntrinsics.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCReplaceCStdLibIntrinsics ${HEADERS} ${SOURCES})
else()
  add_library(SeeCReplaceCStdLibIntrinsics ${SOURCES})
endif()

INSTALL(TARGETS SeeCReplaceCStdLibIntrinsics
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

