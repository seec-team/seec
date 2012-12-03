//===- Preprocessor/SelectArg.h --------------------------------------- C -===//
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

#ifndef SEEC_PREPROCESSOR_SELECTARG_H
#define SEEC_PREPROCESSOR_SELECTARG_H

#define SEEC_PP_ARG10(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ...) _9

#define SEEC_PP_ARG20(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ...) \
          SEEC_PP_ARG10(__VA_ARGS__)

#define SEEC_PP_ARG30(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ...) \
          SEEC_PP_ARG20(__VA_ARGS__)

#define SEEC_PP_ARG40(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ...) \
          SEEC_PP_ARG30(__VA_ARGS__)

#endif // SEEC_PREPROCESSOR_SELECTARG_H
