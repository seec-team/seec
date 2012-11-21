//===- Preprocessor/AddComma.h ---------------------------------------- C -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_PREPROCESSOR_ADDCOMMA_H
#define SEEC_PREPROCESSOR_ADDCOMMA_H

#include "seec/Preprocessor/Concat.h"
#include "seec/Preprocessor/IsEmpty.h"

#define SEEC_PP_PREPEND_COMMA_IF_NOT_EMPTY(...)                                \
        SEEC_PP_PREPEND_COMMA_IF_NOT_EMPTY_IMPL(SEEC_PP_ISEMPTY(__VA_ARGS__),  \
                                                __VA_ARGS__)

#define SEEC_PP_PREPEND_COMMA_IF_NOT_EMPTY_IMPL(EMPTY, ...)                    \
          SEEC_PP_CONCAT2(SEEC_PP_ADD_COMMA_IF_NOT_EMPTY_, EMPTY) __VA_ARGS__

#define SEEC_PP_ADD_COMMA_IF_NOT_EMPTY_0 ,
#define SEEC_PP_ADD_COMMA_IF_NOT_EMPTY_1 

#endif // SEEC_PREPROCESSOR_ADDCOMMA_H
