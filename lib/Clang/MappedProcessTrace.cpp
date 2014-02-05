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

Maybe<std::unique_ptr<ProcessTrace>, Error>
ProcessTrace::load(llvm::StringRef ExecutablePath,
                   std::unique_ptr<trace::InputBufferAllocator> Allocator)
{
  // Load the bitcode.
  auto &Context = llvm::getGlobalContext();
  
  auto MaybeMod = Allocator->getModule(Context);
  if (MaybeMod.assigned<Error>())
    return MaybeMod.move<Error>();
  
  auto const Mod = MaybeMod.get<llvm::Module *>();
  
  // Read the process trace using the InputBufferAllocator.
  auto MaybeProcTrace = trace::ProcessTrace::readFrom(std::move(Allocator));
  if (MaybeProcTrace.assigned<Error>())
    return MaybeProcTrace.move<Error>();

  auto ProcTrace =
    MaybeProcTrace.move<std::unique_ptr<seec::trace::ProcessTrace>>();
  assert(ProcTrace);
  
  return std::unique_ptr<ProcessTrace>
                        (new ProcessTrace(ExecutablePath,
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
