//===- RecordPoints.hpp - Trampolines for instrumented calls -------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This provides the call points for modules instrumented with the
// RecordInternal pass. These simply reroute their information to the
// InternalRecordingListener that was specified when the pass was created.
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_RECORD_INTERNAL_POINTS_HPP_
#define _SEEC_RECORD_INTERNAL_POINTS_HPP_

#include "llvm/Support/DataTypes.h"
#include "RecordInternal.hpp"

namespace llvm {

class Function;

}

extern "C" {

#define SEEC_QUOTE(...) __VA_ARGS__
#define SEEC_RECORD_FORWARD(POINT, ARGS, ...) \
void SeeCRecord##POINT(seec::InternalRecordingListener *listener ARGS) { \
  listener->record##POINT(__VA_ARGS__); \
}

SEEC_RECORD_FORWARD(FunctionBegin,
                    SEEC_QUOTE(, llvm::Function *function),
                    function)

SEEC_RECORD_FORWARD(FunctionEnd,
                    SEEC_QUOTE(),
                    )

SEEC_RECORD_FORWARD(Load,
                    SEEC_QUOTE(, int32_t instr, void *addr, uint64_t length),
                    instr, addr, length)

SEEC_RECORD_FORWARD(PreStore,
                    SEEC_QUOTE(, int32_t instr, void *addr, uint64_t length),
                    instr, addr, length)

SEEC_RECORD_FORWARD(PostStore,
                    SEEC_QUOTE(, int32_t instr, void *addr, uint64_t length),
                    instr, addr, length)

SEEC_RECORD_FORWARD(PreCall,
                    SEEC_QUOTE(, uint32_t instr, void *addr),
                    instr, addr)

SEEC_RECORD_FORWARD(PostCall,
                    SEEC_QUOTE(, uint32_t instr, void *addr),
                    instr, addr)

SEEC_RECORD_FORWARD(PreCallIntrinsic,
                    SEEC_QUOTE(, uint32_t instr),
                    instr)

SEEC_RECORD_FORWARD(PostCallIntrinsic,
                    SEEC_QUOTE(, uint32_t instr),
                    instr)

#define SEEC_RECORD_UPDATE_VALUE(TYPENAME, CTYPE) \
SEEC_RECORD_FORWARD(Update##TYPENAME, \
                    SEEC_QUOTE(, uint32_t instr, CTYPE value), \
                    instr, value)

SEEC_RECORD_UPDATE_VALUE(Pointer, void *)
SEEC_RECORD_UPDATE_VALUE(Int8, uint8_t)
SEEC_RECORD_UPDATE_VALUE(Int16, uint16_t)
SEEC_RECORD_UPDATE_VALUE(Int32, uint32_t)
SEEC_RECORD_UPDATE_VALUE(Int64, uint64_t)
SEEC_RECORD_UPDATE_VALUE(Float, float)
SEEC_RECORD_UPDATE_VALUE(Double, double)

#undef SEEC_RECORD_UPDATE_VALUE
#undef SEEC_RECORD_FORWARD

#define SEEC_REDIRECT_CALL(NAME, RET_TY, RET, PARAMS, ARGUMENTS) \
RET_TY SeeCRedirect_##NAME(seec::InternalRecordingListener *listener PARAMS) { \
  RET listener->redirect_##NAME(ARGUMENTS); \
}
#include "seec/Transforms/RecordInternal/RedirectCalls.def"

#undef SEEC_QUOTE

} // extern "C"

#endif //_SEEC_RECORD_INTERNAL_POINTS_HPP_
