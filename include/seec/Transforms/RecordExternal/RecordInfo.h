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

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
extern char const SeeCInfoModuleIdentifier[];

extern char const SeeCInfoModuleBitcode[];
extern uint64_t   SeeCInfoModuleBitcodeLength;

extern void    *SeeCInfoFunctions[];
extern uint64_t SeeCInfoFunctionsLength;

extern void    *SeeCInfoGlobals[];
extern uint64_t SeeCInfoGlobalsLength;

extern char const __SeeC_ResourcePath__[];
#endif

}

#endif // SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDINFO_H
