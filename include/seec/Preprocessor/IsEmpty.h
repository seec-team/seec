//===- Preprocessor/IsEmpty.h ----------------------------------------- C -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This follows the implementation given by Jens Gustedt at:
/// http://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_PREPROCESSOR_ISEMPTY_H
#define SEEC_PREPROCESSOR_ISEMPTY_H

#include "seec/Preprocessor/Concat.h"
#include "seec/Preprocessor/SelectArg.h"

#define SEEC_PP_HAS_COMMA(...) SEEC_PP_ARG40(__VA_ARGS__, 1,  1,  1,  1, \
                                                      1,  1,  1,  1,  1, \
                                                      1,  1,  1,  1,  1, \
                                                      1,  1,  1,  1,  1, \
                                                      1,  1,  1,  1,  1, \
                                                      1,  1,  1,  1,  1, \
                                                      1,  1,  1,  1,  1, \
                                                      1,  1,  1,  1,  0)

#define SEEC_PP_TRIGGER_PARENTHESIS(...) ,

// There are four separate tests involved to confirm an empty argument:
//  1. Test if there is just one argument.
//  2. Test if there is an argument of form "(...)".
//  3. Test if the argument can be expanded as a function macro.
//  4. Test if placing the macro between a function-like macro name and
//     parenthesis causes the function-like macro to expand (in this case to a
//     comma).
#define SEEC_PP_ISEMPTY(...)                                                   \
          SEEC_PP_ISEMPTY_IMPL(                                                \
            SEEC_PP_HAS_COMMA(__VA_ARGS__),                                    \
            SEEC_PP_HAS_COMMA(SEEC_PP_TRIGGER_PARENTHESIS __VA_ARGS__),        \
            SEEC_PP_HAS_COMMA(__VA_ARGS__ ()),                                 \
            SEEC_PP_HAS_COMMA(SEEC_PP_TRIGGER_PARENTHESIS __VA_ARGS__ ())      \
            )

#define SEEC_PP_ISEMPTY_IMPL(_0, _1, _2, _3)                                   \
          SEEC_PP_HAS_COMMA(                                                   \
            SEEC_PP_CONCAT5(SEEC_PP_ISEMPTY_CASE_, _0, _1, _2, _3))

// This is the only case for which the argument is empty.
#define SEEC_PP_ISEMPTY_CASE_0001 ,

#endif // SEEC_PREPROCESSOR_ISEMPTY_H
