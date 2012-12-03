//===- include/seec/Transforms/RecordExternal/RecordInfo.h ----------------===//
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

#ifndef SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDINFO_H
#define SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDINFO_H

extern "C" {

extern char const SeeCInfoModuleIdentifier[];

extern void *SeeCInfoFunctions[];
extern uint64_t SeeCInfoFunctionsLength;

extern void *SeeCInfoGlobals[];
extern uint64_t SeeCInfoGlobalsLength;

}

#endif // SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDINFO_H
