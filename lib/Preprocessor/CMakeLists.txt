set(HEADERS
  ../../include/seec/Preprocessor/AddComma.h
  ../../include/seec/Preprocessor/Apply.h
  ../../include/seec/Preprocessor/Concat.h
  ../../include/seec/Preprocessor/Count.h
  ../../include/seec/Preprocessor/IsEmpty.h
  ../../include/seec/Preprocessor/MakeMemberChecker.hpp
  ../../include/seec/Preprocessor/MakeMemberFnChecker.hpp
  ../../include/seec/Preprocessor/Quote.h
  ../../include/seec/Preprocessor/SelectArg.h
  )

add_custom_target(SeeCPreprocessor SOURCES ${HEADERS})
