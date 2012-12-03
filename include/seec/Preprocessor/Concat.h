//===- Preprocessor/Concat.h ------------------------------------------ C -===//
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

#ifndef SEEC_PREPROCESSOR_CONCAT_H
#define SEEC_PREPROCESSOR_CONCAT_H

#define SEEC_PP_CONCAT1(_1) _1
#define SEEC_PP_CONCAT2(_1, _2) _1 ## _2
#define SEEC_PP_CONCAT3(_1, _2, _3) _1 ## _2 ## _3
#define SEEC_PP_CONCAT4(_1, _2, _3, _4) _1 ## _2 ## _3 ## _4
#define SEEC_PP_CONCAT5(_1, _2, _3, _4, _5) _1 ## _2 ## _3 ## _4 ## _5

#endif // SEEC_PREPROCESSOR_CONCAT_H
