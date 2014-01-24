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

#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// ProcessTrace
//===----------------------------------------------------------------------===//

seec::Maybe<std::unique_ptr<ProcessTrace>, seec::Error>
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
  
  auto MaybeMod = Allocator->getModule(Context);
  if (MaybeMod.assigned<seec::Error>()) {
    return std::move(MaybeMod.get<seec::Error>());
  }
  
  assert(MaybeMod.assigned<llvm::Module *>());
  auto Mod = MaybeMod.get<llvm::Module *>();
  
  return std::unique_ptr<ProcessTrace>
                        (new ProcessTrace(ExecutablePath,
                                          std::move(Allocator),
                                          std::move(ProcTrace),
                                          std::make_shared<seec::ModuleIndex>
                                                          (*Mod, true)));
}

seec::seec_clang::MappedFunctionDecl const *
ProcessTrace::getMappedFunctionAt(uintptr_t const Address) const
{
  auto const MaybeIndex = UnmappedTrace->getIndexOfFunctionAt(Address);
  if (!MaybeIndex.assigned<uint32_t>())
    return nullptr;
  
  auto const LLVMFn = ModuleIndex->getFunction(MaybeIndex.get<uint32_t>());
  if (!LLVMFn)
    return nullptr;
  
  return Mapping.getMappedFunctionDecl(LLVMFn);
}


} // namespace cm (in seec)

} // namespace seec
