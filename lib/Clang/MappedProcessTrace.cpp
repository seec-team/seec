//===- lib/Clang/MappedProcessTrace.cpp -----------------------------------===//
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

#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Util/MakeUnique.hpp"

#include "llvm/LLVMContext.h"
#include "llvm/Support/IRReader.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// ProcessTrace
//===----------------------------------------------------------------------===//

seec::util::Maybe<std::unique_ptr<ProcessTrace>,
                  seec::Error>
ProcessTrace::
load(llvm::StringRef ExecutablePath,
     std::unique_ptr<seec::trace::InputBufferAllocator> &&Allocator) {
  // Read the process trace using the InputBufferAllocator.
  auto MaybeProcTrace = seec::trace::ProcessTrace::readFrom(*Allocator);
  if (!MaybeProcTrace.assigned(0)) {
    return std::move(MaybeProcTrace.get<1>());
  }

  auto ProcTrace = std::move(MaybeProcTrace.get<0>());
  assert(ProcTrace);

  // Load the bitcode.
  auto &Context = llvm::getGlobalContext();
  
  auto DirPath = Allocator->getTraceDirectory();
  DirPath.appendComponent(ProcTrace->getModuleIdentifier());

  llvm::SMDiagnostic ParseError;
  llvm::Module *Mod = llvm::ParseIRFile(DirPath.str(), ParseError, Context);
  if (!Mod) {
    return seec::Error(LazyMessageByRef::create("SeeCClang",
                      {"errors", "ParseModuleFail"},
                      std::make_pair("path",
                                     Allocator->getTraceDirectory().c_str()),
                      std::make_pair("error",
                                     ParseError.getMessage().c_str())));
  }
  
  return std::unique_ptr<ProcessTrace>
                        (new ProcessTrace(ExecutablePath,
                                          std::move(Allocator),
                                          std::move(ProcTrace),
                                          std::make_shared<seec::ModuleIndex>
                                                          (*Mod, true)));
}


} // namespace cm (in seec)

} // namespace seec
