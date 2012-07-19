//===- Preprocessor/Apply.h ------------------------------------------- C -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_PREPROCESSOR_APPLY_H
#define SEEC_PREPROCESSOR_APPLY_H

#include "seec/Preprocessor/Count.h"

#define SEEC_PP_APPLY1(FUNC, A1) FUNC A1
#define SEEC_PP_APPLY2(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY1(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY3(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY2(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY4(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY3(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY5(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY4(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY6(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY5(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY7(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY6(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY8(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY7(FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY9(FUNC, A1, ...) FUNC A1 SEEC_PP_APPLY8(FUNC, __VA_ARGS__)

#define SEEC_PP_APPLY_IMPL2(NARGS, FUNC, ...) \
          SEEC_PP_APPLY ## NARGS (FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY_IMPL(NARGS, FUNC, ...) \
          SEEC_PP_APPLY_IMPL2(NARGS, FUNC, __VA_ARGS__)
#define SEEC_PP_APPLY(FUNC, ...) \
          SEEC_PP_APPLY_IMPL(SEEC_PP_COUNT_VA(__VA_ARGS__), FUNC, __VA_ARGS__)

#endif // SEEC_PREPROCESSOR_APPLY_H
