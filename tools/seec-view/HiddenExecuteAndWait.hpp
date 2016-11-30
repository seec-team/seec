//===- tools/seec-trace-view/HiddenExecuteAndWait.hpp ---------------------===//
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

#ifndef SEEC_TRACE_VIEW_HIDDENEXECUTEANDWAIT_HPP
#define SEEC_TRACE_VIEW_HIDDENEXECUTEANDWAIT_HPP

#include "llvm/ADT/StringRef.h"
#include <string>

int HiddenExecuteAndWait(llvm::StringRef Program,
                         const char **Args,
                         const char **EnvPtr,
                         std::string *ErrorMsg,
                         bool *ExecFailed);

#endif // SEEC_TRACE_VIEW_HIDDENEXECUTEANDWAIT_HPP
