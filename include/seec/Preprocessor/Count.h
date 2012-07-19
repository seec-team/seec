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

#define SEEC_PP_COUNT_VA_IMPL(_1, _2, _3, _4, _5, \
                              _6, _7, _8, _9, _10, N, ...) N
#define SEEC_PP_COUNT_VA(...) SEEC_PP_COUNT_VA_IMPL(__VA_ARGS__, \
                                                    10, 9, 8, 7, 6, \
                                                    5, 4, 3, 2, 1)

#endif // SEEC_PREPROCESSOR_COUNT_H
