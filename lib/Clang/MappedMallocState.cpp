//===- lib/Clang/MappedMallocState.cpp ------------------------------------===//
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

#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/Printing.hpp"

#include "llvm/Support/raw_ostream.h"

#include <unicode/locid.h>


namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


//===----------------------------------------------------------------------===//
// MallocState
//===----------------------------------------------------------------------===//

MallocState::MallocState(ProcessState const &WithParent,
                         seec::trace::MallocState const &ForState)
: Parent(WithParent),
  UnmappedState(ForState)
{}

uintptr_t MallocState::getAddress() const
{
  return UnmappedState.getAddress();
}

std::size_t MallocState::getSize() const
{
  return UnmappedState.getSize();
}

llvm::Instruction const *MallocState::getAllocatorInst() const
{
  return UnmappedState.getAllocator();
}

seec::seec_clang::MappedInstruction
MallocState::getAllocatorInstMapping() const
{
  auto const &MappedModule = Parent.getProcessTrace().getMapping();
  return MappedModule.getMapping(this->getAllocatorInst());
}

clang::Stmt const *MallocState::getAllocatorStmt() const
{
  auto const &MappedModule = Parent.getProcessTrace().getMapping();
  auto const &MappedInst = MappedModule.getMapping(this->getAllocatorInst());
  return MappedInst.getStmt();
}

void MallocState::print(llvm::raw_ostream &Out,
                        seec::util::IndentationGuide &Indentation) const
{
  std::string AddressString;
  
  {
    llvm::raw_string_ostream AddressStringStream(AddressString);
    seec::util::write_hex_padded(AddressStringStream, this->getAddress());
  }
  
  auto const LazyDescription =
    seec::LazyMessageByRef::create("SeeCClang",
                                   {"states", "MallocState"},
                                   std::make_pair("size",
                                                  int64_t(this->getSize())),
                                   std::make_pair("address",
                                                  AddressString.c_str()));
  
  UErrorCode ICUStatus = U_ZERO_ERROR;
  auto const Description = LazyDescription->get(ICUStatus, Locale());
  
  if (U_FAILURE(ICUStatus))
    return;
  
  Out << Indentation.getString() << Description << "\n";
  
  auto const Mapping = this->getAllocatorInstMapping();
  if (auto const Stmt = Mapping.getStmt()) {
    Indentation.indent();
    
    auto const &AST = Mapping.getAST()->getASTUnit();
    auto const &SrcManager = AST.getSourceManager();
    
    auto const LocStart = Stmt->getLocStart();
    auto const Filename = SrcManager.getFilename(LocStart);
    auto const Line = SrcManager.getSpellingLineNumber(LocStart);
    auto const Column = SrcManager.getSpellingColumnNumber(LocStart);
    
    auto const LazyLocation =
      seec::LazyMessageByRef::create("SeeCClang",
                                     {"states", "MallocStateAllocatedAt"},
                                     std::make_pair("filename",
                                                    Filename.str().c_str()),
                                     std::make_pair("line", int64_t(Line)),
                                     std::make_pair("column", int64_t(Column)));
    
    auto const Location = LazyLocation->get(ICUStatus, Locale());
    
    if (U_SUCCESS(ICUStatus))
      Out << Indentation.getString() << Location << "\n";
    
    Indentation.unindent();
  }
}


//===----------------------------------------------------------------------===//
// MallocState: Printing
//===----------------------------------------------------------------------===//

// Documented in MappedMallocState.hpp
//
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MallocState const &State)
{
  seec::util::IndentationGuide Indent(" ", 2);
  State.print(Out, Indent);
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
