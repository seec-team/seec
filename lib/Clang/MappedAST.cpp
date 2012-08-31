//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "seec/Clang/Compile.hpp"
#include "seec/Clang/MappedAST.hpp"

#include "clang/AST/RecursiveASTVisitor.h"

#include "llvm/Constants.h"
#include "llvm/Instruction.h"

using namespace clang;
using namespace llvm;

namespace seec {

namespace seec_clang {

//===----------------------------------------------------------------------===//
// class Mapper
//===----------------------------------------------------------------------===//

class MappingASTVisitor : public RecursiveASTVisitor<MappingASTVisitor> {
  std::vector<Decl const *> &Decls;
  std::vector<Stmt const *> &Stmts;

public:
  MappingASTVisitor(std::vector<Decl const *> &Decls,
                    std::vector<Stmt const *> &Stmts)
  : Decls(Decls),
    Stmts(Stmts)
  {}

  /// RecursiveASTVisitor Methods
  /// \{

  bool VisitDecl(Decl *D) {
    Decls.push_back(D);
    return true;
  }

  bool VisitStmt(Stmt *S) {
    Stmts.push_back(S);
    return true;
  }

  /// \}
};

//===----------------------------------------------------------------------===//
// class MappedAST
//===----------------------------------------------------------------------===//

MappedAST::~MappedAST() {
  delete AST;
}

std::unique_ptr<MappedAST>
MappedAST::FromASTUnit(clang::ASTUnit *AST) {
  if (!AST)
    return nullptr;

  std::vector<Decl const *> Decls;
  std::vector<Stmt const *> Stmts;

  MappingASTVisitor Mapper(Decls, Stmts);

  for (auto It = AST->top_level_begin(), End = AST->top_level_end();
       It != End; ++It) {
    Mapper.TraverseDecl(*It);
  }

  auto Mapped = std::unique_ptr<MappedAST>(new MappedAST(AST,
                                                         std::move(Decls),
                                                         std::move(Stmts)));
  if (!Mapped)
    delete AST;

  return Mapped;
}

std::unique_ptr<MappedAST>
MappedAST::LoadFromASTFile(llvm::StringRef Filename,
                           IntrusiveRefCntPtr<DiagnosticsEngine> Diags,
                           FileSystemOptions const &FileSystemOpts) {
  return MappedAST::FromASTUnit(
          ASTUnit::LoadFromASTFile(Filename.str(), Diags, FileSystemOpts));
}

std::unique_ptr<MappedAST>
MappedAST::LoadFromCompilerInvocation(
  std::unique_ptr<CompilerInvocation> Invocation,
  IntrusiveRefCntPtr<DiagnosticsEngine> Diags)
{
  return MappedAST::FromASTUnit(
          ASTUnit::LoadFromCompilerInvocation(Invocation.release(), Diags));
}

//===----------------------------------------------------------------------===//
// class MappedModule
//===----------------------------------------------------------------------===//

llvm::sys::Path getPathFromFileNode(llvm::MDNode const *FileNode) {
  auto FilenameStr = dyn_cast<MDString>(FileNode->getOperand(0u));
  if (!FilenameStr)
    return llvm::sys::Path();

  auto PathStr = dyn_cast<MDString>(FileNode->getOperand(1u));
  if (!PathStr)
    return llvm::sys::Path();

  auto FilePath = llvm::sys::Path(PathStr->getString());
  FilePath.appendComponent(FilenameStr->getString());

  return std::move(FilePath);
}

MappedAST const *
MappedModule::getASTForFile(llvm::MDNode const *FileNode) const {
  // check lookup to see if we've already loaded the AST
  auto It = ASTLookup.find(FileNode);
  if (It != ASTLookup.end())
    return It->second;

  // if not, try to load the AST from the source file
  auto FilePath = getPathFromFileNode(FileNode);
  if (FilePath.empty())
    return nullptr;

  auto CI = GetCompileForSourceFile(FilePath.c_str(),
                                    ExecutablePath,
                                    Diags);
  if (!CI) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }

  auto AST = MappedAST::LoadFromCompilerInvocation(std::move(CI), Diags);
  if (!AST) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }

  // get the raw pointer, because we have to push the unique_ptr onto the list
  auto ASTRaw = AST.get();

  ASTLookup[FileNode] = ASTRaw;
  ASTList.push_back(std::move(AST));

  return ASTRaw;
}

MappedModule::MappedModule(
                llvm::Module const &Module,
                llvm::StringRef ExecutablePath,
                llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags)
: // Module(Module),
  ExecutablePath(ExecutablePath),
  Diags(Diags),
  ASTLookup(),
  ASTList(),
  MDStmtIdxKind(Module.getMDKindID(MDStmtIdxStr)),
  MDDeclIdxKind(Module.getMDKindID(MDDeclIdxStr)),
  GlobalLookup()
{
  // Create the GlobalLookup.
  auto GlobalIdxMD = Module.getNamedMetadata(MDGlobalDeclIdxsStr);
  if (!GlobalIdxMD)
    return;

  for (std::size_t i = 0u; i < GlobalIdxMD->getNumOperands(); ++i) {
    auto Node = GlobalIdxMD->getOperand(i);
    assert(Node && Node->getNumOperands() == 3);

    auto FileNode = dyn_cast<MDNode>(Node->getOperand(0u));
    assert(FileNode);

    auto AST = getASTForFile(FileNode);
    assert(AST);

    auto FilePath = getPathFromFileNode(FileNode);
    assert(!FilePath.empty());

    // Sometimes the compilation process creates mappings to Functions that do
    // not exist in the Module, so we must carefully ignore them.
    auto Func = dyn_cast_or_null<Function>(Node->getOperand(1u));
    if (!Func)
      continue;

    auto DeclIdx = dyn_cast<ConstantInt>(Node->getOperand(2u));
    assert(DeclIdx);

    auto Decl = AST->getDeclFromIdx(DeclIdx->getZExtValue());

    GlobalLookup.insert(std::make_pair(Func,
                                       MappedGlobalDecl(std::move(FilePath),
                                                        *AST,
                                                        Decl,
                                                        Func)));
  }
}

MappedGlobalDecl const *
MappedModule::getMappedGlobalDecl(llvm::Function const *F) const {
  auto It = GlobalLookup.find(F);
  if (It == GlobalLookup.end())
    return nullptr;

  return &(It->second);
}

clang::Decl const *MappedModule::getDecl(llvm::Function const *F) const {
  auto It = GlobalLookup.find(F);
  if (It == GlobalLookup.end())
    return nullptr;

  return It->second.getDecl();
}

Decl const *MappedModule::getDecl(Instruction const *I) const {
  auto DeclIdxNode = I->getMetadata(MDDeclIdxKind);
  if (!DeclIdxNode)
    return nullptr;

  auto FileNode = dyn_cast<MDNode>(DeclIdxNode->getOperand(0));
  if (!FileNode)
    return nullptr;

  auto AST = getASTForFile(FileNode);
  if (!AST)
    return nullptr;

  ConstantInt const *CI = dyn_cast<ConstantInt>(DeclIdxNode->getOperand(1));
  if (!CI)
    return nullptr;

  return AST->getDeclFromIdx(CI->getZExtValue());
}

std::pair<clang::Decl const *, MappedAST const *>
MappedModule::getDeclAndMappedAST(llvm::Instruction const *I) const {
  auto DeclIdxNode = I->getMetadata(MDDeclIdxKind);
  if (!DeclIdxNode)
    return std::make_pair(nullptr, nullptr);

  auto FileNode = dyn_cast<MDNode>(DeclIdxNode->getOperand(0));
  if (!FileNode)
    return std::make_pair(nullptr, nullptr);

  auto AST = getASTForFile(FileNode);
  if (!AST)
    return std::make_pair(nullptr, nullptr);

  ConstantInt const *CI = dyn_cast<ConstantInt>(DeclIdxNode->getOperand(1));
  if (!CI)
    return std::make_pair(nullptr, nullptr);

  return std::make_pair(AST->getDeclFromIdx(CI->getZExtValue()), AST);
}

Stmt const *MappedModule::getStmt(Instruction const *I) const {
  auto StmtIdxNode = I->getMetadata(MDStmtIdxKind);
  if (!StmtIdxNode)
    return nullptr;

  auto FileNode = dyn_cast<MDNode>(StmtIdxNode->getOperand(0));
  if (!FileNode)
    return nullptr;

  auto AST = getASTForFile(FileNode);
  if (!AST)
    return nullptr;

  ConstantInt const *CI = dyn_cast<ConstantInt>(StmtIdxNode->getOperand(1));
  if (!CI)
    return nullptr;

  return AST->getStmtFromIdx(CI->getZExtValue());
}

std::pair<clang::Stmt const *, MappedAST const *>
MappedModule::getStmtAndMappedAST(llvm::Instruction const *I) const {
  auto StmtIdxNode = I->getMetadata(MDStmtIdxKind);
  if (!StmtIdxNode)
    return std::make_pair(nullptr, nullptr);

  auto FileNode = dyn_cast<MDNode>(StmtIdxNode->getOperand(0));
  if (!FileNode)
    return std::make_pair(nullptr, nullptr);

  auto AST = getASTForFile(FileNode);
  if (!AST)
    return std::make_pair(nullptr, nullptr);

  ConstantInt const *CI = dyn_cast<ConstantInt>(StmtIdxNode->getOperand(1));
  if (!CI)
    return std::make_pair(nullptr, nullptr);

  return std::make_pair(AST->getStmtFromIdx(CI->getZExtValue()), AST);
}

MappedInstruction MappedModule::getMapping(llvm::Instruction const *I) const {
  auto DeclMap = getDeclAndMappedAST(I);
  auto StmtMap = getStmtAndMappedAST(I);
  
  // Ensure that the Decl and Stmt come from the same AST.
  if (DeclMap.first && StmtMap.first)
    assert(DeclMap.second == StmtMap.second);
  
  // Find the file path from either the Decl or the Stmt mapping. If there is
  // no mapping, return an empty path.
  llvm::sys::Path FilePath;
  
  if (DeclMap.first) {
    auto DeclIdxNode = I->getMetadata(MDDeclIdxKind);
    auto FileNode = dyn_cast<MDNode>(DeclIdxNode->getOperand(0));
    FilePath = getPathFromFileNode(FileNode);
  }
  else if (StmtMap.first) {
    auto StmtIdxNode = I->getMetadata(MDStmtIdxKind);
    auto FileNode = dyn_cast<MDNode>(StmtIdxNode->getOperand(0));
    FilePath = getPathFromFileNode(FileNode);
  }
  
  return MappedInstruction(I,
                           FilePath,
                           DeclMap.second ? DeclMap.second : StmtMap.second,
                           DeclMap.first,
                           StmtMap.first);
}


} // namespace seec_clang (in seec)

} // namespace seec
