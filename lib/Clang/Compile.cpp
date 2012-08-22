#ifndef SEEC_LLVM_OBJ
#error "Must define SEEC_LLVM_OBJ!"
#endif

#include "seec/Clang/Compile.hpp"
#include "seec/Clang/MDNames.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Version.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Serialization/ASTWriter.h"

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Instruction.h"
#include "llvm/Module.h"
#include "llvm/Type.h"
#include "llvm/Value.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace llvm;

namespace seec {

namespace seec_clang {

//===----------------------------------------------------------------------===//
// class SeeCCodeGenAction
//===----------------------------------------------------------------------===//

ASTConsumer *
SeeCCodeGenAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  auto CodeGenConsumer = CodeGenAction::CreateASTConsumer(CI, InFile);
  return new SeeCASTConsumer(*this, CI, CodeGenConsumer);
}

//===----------------------------------------------------------------------===//
// class SeeCASTConsumer
//===----------------------------------------------------------------------===//

bool SeeCASTConsumer::HandleTopLevelDecl(DeclGroupRef D) {
  if (D.isSingleDecl()) {
    TraverseDecl(D.getSingleDecl());
  }
  else {
    DeclGroup &Group = D.getDeclGroup();
    for (unsigned i = 0; i < Group.size(); ++i) {
      TraverseDecl(Group[i]);
    }
  }

  return Child->HandleTopLevelDecl(D);
}

void SeeCASTConsumer::HandleTranslationUnit(ASTContext &Ctx) {
  Child->HandleTranslationUnit(Ctx);
}

bool SeeCASTConsumer::VisitStmt(Stmt *S) {
  Action.addStmtMap(S);
  return true;
}

bool SeeCASTConsumer::VisitDecl(Decl *D) {
  Action.addDeclMap(D);
  return true;
}

//===----------------------------------------------------------------------===//
// GetCompileForSourceFile
//===----------------------------------------------------------------------===//
std::unique_ptr<CompilerInvocation>
GetCompileForSourceFile(char const *Filename,
                        StringRef ExecutablePath,
                        IntrusiveRefCntPtr<DiagnosticsEngine> Diagnostics) {
  // Create a driver to build the compilation
  driver::Driver Driver(ExecutablePath.str(),
                        llvm::sys::getDefaultTargetTriple(),
                        "a.out",
                        true, // IsProduction
                        *Diagnostics);

  Driver.ResourceDir = SEEC_LLVM_OBJ "/lib/clang/" CLANG_VERSION_STRING;

  char const * CompilationArgs[] {
    "-std=c99",
    "-Wall",
    "-pedantic",
    "-g",
    "-fno-builtin",
    "-fno-stack-protector",
    "-D_FORTIFY_SOURCE=0",
    "-emit-llvm",
    "-S",
    Filename
  };

  std::unique_ptr<driver::Compilation> Compilation
    (Driver.BuildCompilation(CompilationArgs));
  if (!Compilation)
    return nullptr; // TODO: emit error to diagnostics?

  driver::JobList &Jobs = Compilation->getJobs();
  if (Jobs.size() != 1)
    return nullptr;

  driver::Command *Command = dyn_cast<driver::Command>(*Jobs.begin());
  if (!Command)
    return nullptr;

  if (StringRef(Command->getCreator().getName()) != "clang")
    return nullptr;

  driver::ArgStringList const &Args = Command->getArguments();

  std::unique_ptr<CompilerInvocation> Invocation (new CompilerInvocation());

  bool Created = CompilerInvocation::CreateFromArgs(*Invocation,
                                                    Args.data() + 1,
                                                    Args.data() + Args.size(),
                                                    *Diagnostics);
  if (!Created)
    return nullptr;

  return Invocation;
}

//===----------------------------------------------------------------------===//
// GetMetadataPointer
//===----------------------------------------------------------------------===//
template<typename T>
T const *GetMetadataPointer(Instruction const &I, unsigned MDKindID) {
  MDNode *Node = I.getMetadata(MDKindID);
  if (!Node)
    return nullptr;

  Value *Op = Node->getOperand(0);
  if (!Op) {
    errs() << "!Op\n";
    return nullptr;
  }

  ConstantInt *CI = dyn_cast<ConstantInt>(Op);
  if (!CI) {
    errs() << "!CI\n";
    return nullptr;
  }

  return (T const *) (uintptr_t) CI->getZExtValue();
}

//===----------------------------------------------------------------------===//
// GenerateSerializableMappings
//===----------------------------------------------------------------------===//
/// Modify Mod's Metadata to use numbered mappings rather than pointers.
void GenerateSerializableMappings(SeeCCodeGenAction &Action,
                                  llvm::Module *Mod,
                                  SourceManager &SM,
                                  StringRef MainFilename) {
  auto &ModContext = Mod->getContext();

  auto Int64Ty = llvm::Type::getInt64Ty(ModContext);

  auto &DeclMap = Action.getDeclMap();
  auto &StmtMap = Action.getStmtMap();

  // setup the file node for the main file and add it to the files node
  llvm::Value *MainFileNodeOps[] {
    MDString::get(ModContext, MainFilename),
    MDString::get(ModContext, llvm::sys::Path::GetCurrentDirectory().str())
  };

  auto MainFileNode = MDNode::get(ModContext, MainFileNodeOps);

  // Handle Instruction Metadata
  unsigned MDStmtPtrID = Mod->getMDKindID(MDStmtPtrStr);
  unsigned MDStmtIdxID = Mod->getMDKindID(MDStmtIdxStr);
  unsigned MDDeclPtrID = Mod->getMDKindID(MDDeclPtrStr);
  unsigned MDDeclIdxID = Mod->getMDKindID(MDDeclIdxStr);

  for (auto &Function: *Mod) {
    for (auto &BasicBlock: Function) {
      for (auto &Instruction: BasicBlock) {
        if (!Instruction.hasMetadata())
          continue;

        Stmt const *S = GetMetadataPointer<Stmt>(Instruction, MDStmtPtrID);
        if (S) {
          auto It = StmtMap.find(S);
          if (It != StmtMap.end()) {
            Value *Ops[] {
              MainFileNode,
              ConstantInt::get(Int64Ty, It->second) // StmtIdx
            };

            Instruction.setMetadata(MDStmtIdxID, MDNode::get(ModContext, Ops));
            Instruction.setMetadata(MDStmtPtrID, nullptr);
          }
        }

        Decl const *D = GetMetadataPointer<Decl>(Instruction, MDDeclPtrID);
        if (D) {
          auto It = DeclMap.find(D);
          if (It != DeclMap.end()) {
            Value *Ops[] {
              MainFileNode,
              ConstantInt::get(Int64Ty, It->second) // DeclIdx
            };

            Instruction.setMetadata(MDDeclIdxID, MDNode::get(ModContext, Ops));
            Instruction.setMetadata(MDDeclPtrID, nullptr);
          }
        }
      }
    }
  }

  // Handle global Metadata
  llvm::NamedMDNode *GlobalPtrMD = Mod->getNamedMetadata(MDGlobalDeclPtrsStr);
  if (GlobalPtrMD) {
    llvm::NamedMDNode *GlobalIdxMD
      = Mod->getOrInsertNamedMetadata(MDGlobalDeclIdxsStr);

    unsigned NumOperands = GlobalPtrMD->getNumOperands();
    for (unsigned i = 0; i < NumOperands; ++i) {
      llvm::MDNode *PtrNode = GlobalPtrMD->getOperand(i);

      assert(PtrNode->getNumOperands() == 2 && "Unexpected NumOperands!");

      ConstantInt *CIPtr = dyn_cast<ConstantInt>(PtrNode->getOperand(1));
      Decl const *D = (Decl const *) (uintptr_t) CIPtr->getZExtValue();

      auto It = DeclMap.find(D);
      if (It == DeclMap.end()) {
        errs() << "couldn't find Idx for Decl";
        if (NamedDecl const *ND = dyn_cast<NamedDecl>(D))
          errs() << ": " << ND->getName();
        errs() << "\n";
        continue;
      }

      llvm::Value *Ops[] = {
        MainFileNode,
        PtrNode->getOperand(0),
        ConstantInt::get(Int64Ty, It->second)
      };

      GlobalIdxMD->addOperand(llvm::MDNode::get(ModContext, Ops));
    }
  }
}

} // namespace clang (in seec)

} // namespace seec
