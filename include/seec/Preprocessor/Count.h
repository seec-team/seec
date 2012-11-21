//===- Preprocessor/Count.h ------------------------------------------- C -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_PREPROCESSOR_COUNT_H
#define SEEC_PREPROCESSOR_COUNT_H

#include "seec/Preprocessor/SelectArg.h"

#define SEEC_PP_COUNT_VA(...) SEEC_PP_ARG40(__VA_ARGS__, 39, 38, 37, 36, 35, \
                                                         34, 33, 32, 31, 30, \
                                                         29, 28, 27, 26, 25, \
                                                         24, 23, 22, 21, 20, \
                                                         19, 18, 17, 16, 15, \
                                                         14, 13, 12, 11, 10, \
                                                          9,  8,  7,  6,  5, \
                                                          4,  3,  2,  1) 

#endif // SEEC_PREPROCESSOR_COUNT_H
