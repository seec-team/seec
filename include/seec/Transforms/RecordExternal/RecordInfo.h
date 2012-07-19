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
