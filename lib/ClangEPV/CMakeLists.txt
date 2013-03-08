set(SEEC_CLANGEPV_HEADERS
  ../../include/seec/ClangEPV/ClangEPV.hpp
  DeclLinks.def
  StmtLinks.def
)

set(SEEC_CLANGEPV_SOURCES
  ClangEPV.cpp
)

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCClangEPV ${SEEC_CLANGEPV_HEADERS} ${SEEC_CLANGEPV_SOURCES})
else()
  add_library(SeeCClangEPV ${SEEC_CLANGEPV_SOURCES})
endif()

INSTALL(TARGETS SeeCClangEPV
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
