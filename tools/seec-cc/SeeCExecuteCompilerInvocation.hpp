//===- tools/seec-cc/SeeCExecuteCompilerInvocation.hpp --------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
///
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_CC_SEECEXECUTECOMPILERINVOCATION_HPP
#define SEEC_CC_SEECEXECUTECOMPILERINVOCATION_HPP

namespace clang {
  class CompilerInstance;
}

bool DoCompilerInvocation(clang::CompilerInstance *Clang,
                          const char * const *ArgBegin,
                          const char * const *ArgEnd);

#endif // SEEC_CC_SEECEXECUTECOMPILERINVOCATION_HPP
