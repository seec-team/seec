//===- include/seec/Clang/Compile.hpp -------------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file SeeC Clang usage helpers.
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_COMPILE_HPP
#define SEEC_CLANG_COMPILE_HPP

#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclGroup.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/CodeGen/BackendUtil.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInvocation.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace clang {

class ASTDeserializationListener;
class ASTMutationListener;
class CompilerInstance;
class CXXRecordDecl;
class Decl;
class FunctionDecl;
class Stmt;
class TagDecl;

}

namespace llvm {

class LLVMContext;

}

namespace seec {

namespace seec_clang {

class SeeCCodeGenAction : public clang::CodeGenAction {
  char const * const *ArgBegin;
  char const * const *ArgEnd;
  
  clang::CompilerInstance *Compiler;
  std::string File;
  
  uint64_t NextDeclIndex;
  uint64_t NextStmtIndex;

  llvm::DenseMap<clang::Decl const *, uint64_t> DeclMap;
  llvm::DenseMap<clang::Stmt const *, uint64_t> StmtMap;

public:
  SeeCCodeGenAction(const char * const *WithArgBegin,
                    const char * const *WithArgEnd,
                    unsigned _Action,
                    llvm::LLVMContext *_VMContext = 0)
  : CodeGenAction(_Action, _VMContext),
    ArgBegin(WithArgBegin),
    ArgEnd(WithArgEnd),
    Compiler(nullptr),
    NextDeclIndex(0),
    NextStmtIndex(0),
    DeclMap(),
    StmtMap()
  {}

  virtual
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile);
  
  virtual void ModuleComplete(llvm::Module *Mod);

  void addDeclMap(clang::Decl const *D) {
    if (!DeclMap.count(D))
      DeclMap.insert(std::make_pair(D, NextDeclIndex++));
  }

  void addStmtMap(clang::Stmt const *S) {
    if (!StmtMap.count(S))
      StmtMap.insert(std::make_pair(S, NextStmtIndex++));
  }

  decltype(DeclMap) const &getDeclMap() { return DeclMap; }

  decltype(StmtMap) const &getStmtMap() { return StmtMap; }
};

class SeeCEmitAssemblyAction : public SeeCCodeGenAction {
  virtual void anchor();
public:
  SeeCEmitAssemblyAction(const char * const *ArgBegin,
                         const char * const *ArgEnd,
                         llvm::LLVMContext *_VMContext = 0);
};

class SeeCEmitBCAction : public SeeCCodeGenAction {
  virtual void anchor();
public:
  SeeCEmitBCAction(const char * const *ArgBegin,
                   const char * const *ArgEnd,
                   llvm::LLVMContext *_VMContext = 0);
};

class SeeCEmitLLVMAction : public SeeCCodeGenAction {
  virtual void anchor();
public:
  SeeCEmitLLVMAction(const char * const *ArgBegin,
                     const char * const *ArgEnd,
                     llvm::LLVMContext *_VMContext = 0);
};

class SeeCEmitLLVMOnlyAction : public SeeCCodeGenAction {
  virtual void anchor();
public:
  SeeCEmitLLVMOnlyAction(const char * const *ArgBegin,
                         const char * const *ArgEnd,
                         llvm::LLVMContext *_VMContext = 0);
};

class SeeCEmitCodeGenOnlyAction : public SeeCCodeGenAction {
  virtual void anchor();
public:
  SeeCEmitCodeGenOnlyAction(const char * const *ArgBegin,
                            const char * const *ArgEnd,
                            llvm::LLVMContext *_VMContext = 0);
};

class SeeCEmitObjAction : public SeeCCodeGenAction {
  virtual void anchor();
public:
  SeeCEmitObjAction(const char * const *ArgBegin,
                    const char * const *ArgEnd,
                    llvm::LLVMContext *_VMContext = 0);
};

class SeeCASTConsumer
: public clang::ASTConsumer,
  public clang::RecursiveASTVisitor<SeeCASTConsumer>
{
  SeeCCodeGenAction &Action;

  std::unique_ptr<clang::ASTConsumer> Child;

  std::vector<clang::VariableArrayType *> VATypes;

public:
  SeeCASTConsumer(SeeCCodeGenAction &Action,
                  std::unique_ptr<clang::ASTConsumer> Child)
  : Action(Action),
    Child(std::move(Child)),
    VATypes()
  {}

  virtual ~SeeCASTConsumer();

  /// \name ASTConsumer Methods
  /// \{
  virtual void Initialize(clang::ASTContext &Context) {
    Child->Initialize(Context);
  }

  virtual bool HandleTopLevelDecl(clang::DeclGroupRef D);

  virtual void HandleInterestingDecl(clang::DeclGroupRef D) {
    HandleTopLevelDecl(D);
  }

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx);

  virtual void HandleTagDeclDefinition(clang::TagDecl *D) {
    Child->HandleTagDeclDefinition(D);
  }

  virtual void HandleCXXImplicitFunctionInstantiation(clang::FunctionDecl *D){
    Child->HandleCXXImplicitFunctionInstantiation(D);
  }

  virtual void HandleTopLevelDeclInObjCContainer(clang::DeclGroupRef D) {
    Child->HandleTopLevelDeclInObjCContainer(D);
  }

  virtual void CompleteTentativeDefinition(clang::VarDecl *D) {
    Child->CompleteTentativeDefinition(D);
  }

  virtual void HandleVTable(clang::CXXRecordDecl *D, bool DefinitionRequired){
    Child->HandleVTable(D, DefinitionRequired);
  }

  virtual clang::ASTMutationListener *GetASTMutationListener() {
    return Child->GetASTMutationListener();
  }

  virtual clang::ASTDeserializationListener *GetASTDeserializationListener() {
    return Child->GetASTDeserializationListener();
  }

  virtual void PrintStats() {
    Child->PrintStats();
  }

  /// \}

  /// RecursiveASTVisitor Methods
  /// \{

  bool VisitStmt(clang::Stmt *S);

  bool VisitDecl(clang::Decl *D);

  bool VisitVariableArrayType(::clang::VariableArrayType *T);

  /// \}
};

/// \brief Find the resources directory.
///
std::string getResourcesDirectory(llvm::StringRef ExecutablePath);

/// \brief Find the runtime library directory.
///
std::string getRuntimeLibraryDirectory(llvm::StringRef ExecutablePath);

/// \brief Make all SeeC-Clang mapping information in Mod serializable.
///
void GenerateSerializableMappings(SeeCCodeGenAction &Action,
                                  llvm::Module *Mod,
                                  clang::SourceManager &SM,
                                  llvm::StringRef MainFilename);

/// \brief Store all source files in SrcManager into the given llvm::Module.
///
void StoreCompileInformationInModule(llvm::Module *Mod,
                                     clang::CompilerInstance &Compiler,
                                     const char * const *ArgBegin,
                                     const char * const *ArgEnd);

} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_COMPILE_HPP
