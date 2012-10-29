//===- Compile.hpp - SeeC Clang usage helpers -----------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_COMPILE_HPP
#define SEEC_CLANG_COMPILE_HPP

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
  uint64_t NextDeclIndex;
  uint64_t NextStmtIndex;

  llvm::DenseMap<clang::Decl const *, uint64_t> DeclMap;
  llvm::DenseMap<clang::Stmt const *, uint64_t> StmtMap;

  virtual void anchor() {};

public:
  SeeCCodeGenAction(llvm::LLVMContext *_VMContext = 0)
  : CodeGenAction(clang::Backend_EmitNothing, _VMContext),
    NextDeclIndex(0),
    NextStmtIndex(0),
    DeclMap(),
    StmtMap()
  {}

  virtual
  clang::ASTConsumer *
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile);

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

class SeeCASTConsumer
: public clang::ASTConsumer,
  public clang::RecursiveASTVisitor<SeeCASTConsumer>
{
  SeeCCodeGenAction &Action;

  clang::CompilerInstance &CI;

  std::unique_ptr<clang::ASTConsumer> Child;

public:
  SeeCASTConsumer(SeeCCodeGenAction &Action,
                  clang::CompilerInstance &CI,
                  clang::ASTConsumer *Child)
  : Action(Action),
    CI(CI),
    Child(Child)
  {}

  virtual ~SeeCASTConsumer() {}

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

  /// \}
};

///
/// \param Filename The source file to be compiled.
/// \param ExecutablePath Used by the Clang driver to find resources.
/// \param Diagnostics The diagnostics engine for this compilation.
/// \return A std::unique_ptr holding a clang::CompileInvocation that will
///         parse the given source file.
std::unique_ptr<clang::CompilerInvocation>
GetCompileForSourceFile(
  char const *Filename,
  llvm::StringRef ExecutablePath,
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics);

///
void GenerateSerializableMappings(SeeCCodeGenAction &Action,
                                  llvm::Module *Mod,
                                  clang::SourceManager &SM,
                                  llvm::StringRef MainFilename);

/// \brief Store all source files in SrcManager into the given llvm::Module.
void StoreCompileInformationInModule(llvm::Module *Mod,
                                     clang::CompilerInstance &Compiler);

} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_COMPILE_HPP
