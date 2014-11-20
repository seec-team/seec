//===- lib/Runtimes/Tracer/PrintRunError.cpp ------------------------------===//
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

#include "PrintRunError.hpp"

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

void PrintRunError(seec::runtime_errors::RunError const &Error,
                   llvm::Instruction const *Instruction,
                   seec::ModuleIndex const &ModIndex,
                   seec::AugmentationCollection const &Augmentations)
{
  using namespace seec::runtime_errors;

  auto MaybeDesc = Description::create(Error, Augmentations.getCallbackFn());
  if (!MaybeDesc.assigned<std::unique_ptr<Description>>())
    return;

  auto const Desc = DescriptionPrinterUnicode{
                      MaybeDesc.move<std::unique_ptr<Description>>(),
                      "\n",
                      " "};

  llvm::errs() << "\n" << Desc.getString() << "\n";

  // Now attempt to print the original source location, if this Module has
  // SeeC-Clang mapping.

  // Setup diagnostics printing for Clang diagnostics.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts
    = new clang::DiagnosticOptions();
  DiagOpts->ShowColors = true;

  clang::IgnoringDiagConsumer DiagnosticPrinter;

  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics
    = new clang::DiagnosticsEngine(
      llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs>
                              (new clang::DiagnosticIDs()),
      &*DiagOpts,
      &DiagnosticPrinter,
      false);

  Diagnostics->setSuppressSystemWarnings(true);
  Diagnostics->setIgnoreAllWarnings(true);

  // Setup the map to find Decls and Stmts from Instructions
  seec::seec_clang::MappedModule MapMod(ModIndex, Diagnostics);

  clang::LangOptions LangOpt;

  clang::PrintingPolicy PrintPolicy(LangOpt);
  PrintPolicy.ConstantArraySizeAsWritten = true;

  auto const StmtAndAST = MapMod.getStmtAndMappedAST(Instruction);
  if (!StmtAndAST.first || !StmtAndAST.second)
    return;

  auto const &AST = StmtAndAST.second->getASTUnit();
  auto const &SrcManager = AST.getSourceManager();

  auto const LocStart = StmtAndAST.first->getLocStart();
  auto const Filename = SrcManager.getFilename(LocStart);
  auto const Line = SrcManager.getSpellingLineNumber(LocStart);
  auto const Column = SrcManager.getSpellingColumnNumber(LocStart);

  llvm::errs() << Filename << "\n"
               << "Line " << Line
               << " Column " << Column << ":\n";

  StmtAndAST.first->printPretty(llvm::errs(), nullptr, PrintPolicy);

  llvm::errs() << "\n";
}

} // namespace trace (in seec)

} // namespace seec
