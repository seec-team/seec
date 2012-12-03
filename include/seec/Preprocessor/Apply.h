//===- Preprocessor/Apply.h ------------------------------------------- C -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_PREPROCESSOR_APPLY_H
#define SEEC_PREPROCESSOR_APPLY_H

#include "seec/Preprocessor/Count.h"
#include "seec/Preprocessor/Quote.h"

#define SEEC_PP_APPLY_EMPTY_SEPARATOR 
#define SEEC_PP_APPLY_COMMA_SEPARATOR ,

#define SEEC_PP_APPLY1(FN, S, A1) FN A1

#define SEEC_PP_APPLY2(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY1(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY3(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY2(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY4(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY3(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY5(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY4(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY6(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY5(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY7(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY6(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY8(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY7(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY9(FN, S, A1, ...) FN A1 S \
                              SEEC_PP_APPLY8(FN, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY_IMPL2(NARGS, FUNC, S, ...) \
          SEEC_PP_APPLY ## NARGS (FUNC, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY_IMPL(NARGS, FUNC, S, ...) \
          SEEC_PP_APPLY_IMPL2(NARGS, FUNC, SEEC_PP_QUOTE(S), __VA_ARGS__)

#define SEEC_PP_APPLY(FUNC, ...) \
          SEEC_PP_APPLY_IMPL(SEEC_PP_COUNT_VA(__VA_ARGS__), \
                             FUNC, \
                             SEEC_PP_APPLY_EMPTY_SEPARATOR, \
                             __VA_ARGS__)

#define SEEC_PP_APPLY_WITH_SEP(FUNC, SEP, ...) \
          SEEC_PP_APPLY_IMPL(SEEC_PP_COUNT_VA(__VA_ARGS__), \
                             FUNC, \
                             SEEC_PP_QUOTE(SEP), \
                             __VA_ARGS__)

#endif // SEEC_PREPROCESSOR_APPLY_H
