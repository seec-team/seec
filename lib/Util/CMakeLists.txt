set(HEADERS
  ../../include/seec/Util/CheckNew.hpp
  ../../include/seec/Util/ConstantIntPointer.hpp
  ../../include/seec/Util/ConstExprCString.hpp
  ../../include/seec/Util/ConstExprMath.hpp
  ../../include/seec/Util/Dispatch.hpp
  ../../include/seec/Util/Endianness.hpp
  ../../include/seec/Util/Error.hpp
  ../../include/seec/Util/FunctionTraits.hpp
  ../../include/seec/Util/LockedObjectAccessor.hpp
  ../../include/seec/Util/MakeUnique.hpp
  ../../include/seec/Util/Maybe.hpp
  ../../include/seec/Util/ModuleIndex.hpp
  ../../include/seec/Util/Printing.hpp
  ../../include/seec/Util/Range.hpp
  ../../include/seec/Util/Reverse.hpp
  ../../include/seec/Util/ScopeExit.hpp
  ../../include/seec/Util/Serialization.hpp
  ../../include/seec/Util/SynchronizedExit.hpp
  ../../include/seec/Util/TemplateSequence.hpp
  ../../include/seec/Util/ValueConversion.hpp
  )

set(SOURCES
  Printing.cpp
  SynchronizedExit.cpp
  )

if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_library(SeeCUtil ${HEADERS} ${SOURCES})
else()
  add_library(SeeCUtil ${SOURCES})
endif()

INSTALL(TARGETS SeeCUtil
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

