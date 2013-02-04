//===- tools/seec-explain-test/main.cpp -----------------------------------===//
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


#include "seec/Clang/Compile.hpp"
#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/Search.hpp"
#include "seec/ClangEPV/ClangEPV.hpp"
#include "seec/ICU/Indexing.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/ICU/Resources.hpp"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"

#include "unicode/unistr.h"

#include <array>
#include <cstdlib>
#include <iostream>


using namespace seec;

namespace {
  static llvm::cl::opt<std::string>
  InputFile(llvm::cl::desc("<input source>"),
            llvm::cl::Positional,
            llvm::cl::init(""));
}

// From clang's driver.cpp:
llvm::sys::Path GetExecutablePath(const char *ArgV0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return llvm::sys::Path(ArgV0);

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void *) (intptr_t) GetExecutablePath;
  return llvm::sys::Path::GetMainExecutable(ArgV0, P);
}

int main(int argc, char **argv, char * const *envp) {
  atexit(llvm::llvm_shutdown);

  llvm::cl::ParseCommandLineOptions(argc, argv, "seec explanation tester\n");

  auto const ExecutablePath = GetExecutablePath(argv[0], true);

  // Load SeeC's required ICU resources.
  seec::ResourceLoader Resources(ExecutablePath);
  
  std::array<char const *, 2> ResourceList {
    {"SeeCClang", "ClangEPV"}
  };
  
  if (!Resources.loadResources(ResourceList)) {
    llvm::errs() << "failed to load resources\n";
    std::exit(EXIT_FAILURE);
  }

  // Setup clang's diagnostics printing
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts =
    new clang::DiagnosticOptions();
  DiagOpts->ShowColors = true;

  clang::TextDiagnosticPrinter DiagnosticPrinter(llvm::errs(), &*DiagOpts);

  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics =
    new clang::DiagnosticsEngine(
          llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs>
                                  (new clang::DiagnosticIDs()),
          &*DiagOpts,
          &DiagnosticPrinter,
          false);

  Diagnostics->setSuppressSystemWarnings(true);
  
  // Attempt to parse the input file.
  auto Invocation =
    seec::seec_clang::GetCompileForSourceFile(InputFile.c_str(),
                                              ExecutablePath.str(),
                                              Diagnostics);
  
  if (!Invocation) {
    llvm::errs() << "Couldn't get CompilerInvocation.\n";
    std::exit(EXIT_FAILURE);
  }
  
  auto MappedAST = 
    seec_clang::MappedAST::LoadFromCompilerInvocation(std::move(Invocation),
                                                      Diagnostics);
  
  if (!MappedAST) {
    llvm::errs() << "Couldn't get MappedAST.\n";
    std::exit(EXIT_FAILURE);
  }
  
  // Test lookups.
  std::string LookFile;
  unsigned LookLine;
  unsigned LookColumn;
  
  while (true) {
    llvm::outs() << "Lookup file: ";
    std::cin >> LookFile;
    if (std::cin.fail())
      break;
    
    llvm::outs() << "Lookup line: ";
    std::cin >> LookLine;
    if (std::cin.fail())
      break;
    
    llvm::outs() << "Lookup column: ";
    std::cin >> LookColumn;
    if (std::cin.fail())
      break;
    
    auto Result = seec::seec_clang::search(MappedAST->getASTUnit(),
                                           LookFile,
                                           LookLine,
                                           LookColumn);
    
    if (Result.assigned<seec::Error>()) {
      UErrorCode Status = U_ZERO_ERROR;
      auto Message = Result.get<seec::Error>().getMessage(Status, Locale());
      llvm::errs() << Message << "\n";
      continue;
    }
    
    auto &Found = Result.get<seec::seec_clang::SearchResult>();
    
    switch (Found.getFoundLast()) {
      case seec::seec_clang::SearchResult::EFoundKind::None:
        llvm::outs() << "found nothing.\n";
        break;
      
      case seec::seec_clang::SearchResult::EFoundKind::Decl:
        llvm::outs() << "found decl:\n";
        {
          auto D = Found.getFoundDecl();
          D->print(llvm::outs());
          
          auto Exp = seec::clang_epv::explain(D);
          if (Exp.assigned<seec::Error>()) {
            UErrorCode Status = U_ZERO_ERROR;
            auto Message = Exp.get<seec::Error>().getMessage(Status, Locale());
            llvm::outs() << "Couldn't get explanation: " << Message << "\n";
          }
          else {
            llvm::outs() << Exp.get<0>()->getString() << "\n";
          }
        }
        break;
      
      case seec::seec_clang::SearchResult::EFoundKind::Stmt:
        llvm::outs() << "found stmt:\n";
        {
          auto S = Found.getFoundStmt();
          S->dumpPretty(MappedAST->getASTUnit().getASTContext());
          
          auto Exp = seec::clang_epv::explain(S);
          if (Exp.assigned<seec::Error>()) {
            UErrorCode Status = U_ZERO_ERROR;
            auto Message = Exp.get<seec::Error>().getMessage(Status, Locale());
            llvm::outs() << "Couldn't get explanation: " << Message << "\n";
          }
          else {
            llvm::outs() << Exp.get<0>()->getString() << "\n";
          }
        }
        break;
    }
  }
  
  llvm::outs() << "Finished.\n";
  return EXIT_SUCCESS;
}
