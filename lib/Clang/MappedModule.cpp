//===- lib/Clang/MappedModule.cpp -----------------------------------------===//
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

#define DEBUG_TYPE "seec-clang"

#include "seec/Clang/Compile.hpp"
#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedParam.hpp"
#include "seec/Clang/MappedStmt.hpp"
#include "seec/Clang/MDNames.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/AST/RecursiveASTVisitor.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Debug.h"

#include <algorithm>

using namespace clang;
using namespace llvm;

namespace seec {

namespace seec_clang {


//===----------------------------------------------------------------------===//
// MappedFunctionDecl
//===----------------------------------------------------------------------===//

bool MappedFunctionDecl::isInSystemHeader() const
{
  auto const &SrcMgr = AST.getASTUnit().getSourceManager();
  
  auto const BodyStmt = Decl->getBody();
  if (!BodyStmt)
    return false;
  
  return SrcMgr.isInSystemHeader(BodyStmt->getLocStart());
}


//===----------------------------------------------------------------------===//
// MappedGlobalVariableDecl
//===----------------------------------------------------------------------===//

MappedGlobalVariableDecl::
MappedGlobalVariableDecl(MappedAST const &AST,
                         clang::ValueDecl const *Decl,
                         llvm::GlobalVariable const *Global)
: AST(AST),
  Decl(Decl),
  Global(Global),
  InSystemHeader(AST.getASTUnit()
                    .getSourceManager()
                    .isInSystemHeader(Decl->getLocation())),
  Referenced(AST.isReferenced(Decl))
{}


//===----------------------------------------------------------------------===//
// class MappedCompileInfo
//===----------------------------------------------------------------------===//

MappedCompileInfo::FileInfo const *MappedCompileInfo::getMainFileInfo() const
{
  for (auto const &FI : SourceFiles)
    if (FI.getName() == MainFileName)
      return &FI;

  return nullptr;
}

std::unique_ptr<MappedCompileInfo>
MappedCompileInfo::get(llvm::MDNode *CompileInfo) {
  if (!CompileInfo || CompileInfo->getNumOperands() != 3)
    return nullptr;
  
  // Get the main file info.
  auto MainFile = llvm::dyn_cast<llvm::MDNode>(CompileInfo->getOperand(0u));
  if (!MainFile)
    return nullptr;
  
  assert(MainFile->getNumOperands() == 2u);
  
  auto MainFileName = llvm::dyn_cast<llvm::MDString>(MainFile->getOperand(0u));
  auto MainDirectory = llvm::dyn_cast<llvm::MDString>(MainFile->getOperand(1u));
  
  // Get the source file information.
  auto SourcesNode = llvm::dyn_cast<llvm::MDNode>(CompileInfo->getOperand(1u));
  if (!SourcesNode)
    return nullptr;
  
  // Get the arguments.
  auto ArgsNode = llvm::dyn_cast<llvm::MDNode>(CompileInfo->getOperand(2u));
  if (!ArgsNode)
    return nullptr;
  
  // Extract the source file information.
  std::vector<FileInfo> SourceFiles;
  
  for (unsigned i = 0u; i < SourcesNode->getNumOperands(); ++i) {
    auto SourceNode = llvm::dyn_cast<llvm::MDNode>(SourcesNode->getOperand(i));
    if (!SourceNode)
      continue;
    
    assert(SourceNode->getNumOperands() == 2u);
    
    auto Name = llvm::dyn_cast<llvm::MDString>(SourceNode->getOperand(0u));
    assert(Name);

    auto DataNode = SourceNode->getOperand(1u);
    auto Contents = llvm::dyn_cast<llvm::ConstantDataSequential>(DataNode);
    assert(Contents);
    
    SourceFiles.emplace_back(Name->getString().str(),
                             Contents->getRawDataValues());
  }
  
  // Extract the invocation arguments.
  std::vector<std::string> InvocationArguments;
  
  for (unsigned i = 0u; i < ArgsNode->getNumOperands(); ++i) {
    auto Str = llvm::dyn_cast<llvm::MDString>(ArgsNode->getOperand(i));
    if (!Str)
      continue;
    
    InvocationArguments.push_back(Str->getString().str());
  }
  
  return std::unique_ptr<MappedCompileInfo>(
            new MappedCompileInfo(MainDirectory->getString().str(),
                                  MainFileName->getString().str(),
                                  std::move(SourceFiles),
                                  std::move(InvocationArguments)));
}

std::unique_ptr<CompilerInvocation>
MappedCompileInfo::createCompilerInvocation(DiagnosticsEngine &Diags) const
{
  std::unique_ptr<CompilerInvocation> CI;
  llvm::SmallString<256> FilePath {MainDirectory};
  llvm::sys::path::append(FilePath, MainFileName);

  if (!FilePath.empty()) {
    // We have the original compile arguments, so we can build a compiler
    // invocation from that.
    std::vector<char const *> Args;
    for (auto &Arg : InvocationArguments)
      Args.emplace_back(Arg.c_str());

    CI = seec::makeUnique<CompilerInvocation>();
    bool Created = CompilerInvocation::CreateFromArgs(*CI,
                                                      Args.data(),
                                                      Args.data() + Args.size(),
                                                      Diags);
    if (!Created)
      CI.reset();
  }

  return CI;
}

void MappedCompileInfo::createVirtualFiles(clang::FileManager &FM,
                                           clang::SourceManager &SM) const
{
  // SeeC-Clang specific: only allow the files and contents that we set below.
  FM.setDisableNonVirtualFiles(true);

  for (auto const &FileInfo : SourceFiles) {
    auto &Contents = FileInfo.getContents();
    auto const Entry = FM.getVirtualFile(FileInfo.getName(),
                                         Contents.getBufferSize(),
                                         0 /* ModificationTime */);

    SM.overrideFileContents(Entry, &Contents, true /* DoNotFree */);
  }
}

//===----------------------------------------------------------------------===//
// class MappedModule
//===----------------------------------------------------------------------===//

static std::string getPathFromFileNode(llvm::MDNode const *FileNode) {
  auto FilenameStr = dyn_cast<MDString>(FileNode->getOperand(0u));
  if (!FilenameStr)
    return std::string();

  auto PathStr = dyn_cast<MDString>(FileNode->getOperand(1u));
  if (!PathStr)
    return std::string();

  llvm::SmallString<256> FilePath {PathStr->getString()};
  llvm::sys::path::append(FilePath, FilenameStr->getString());

  return FilePath.str().str();
}

MappedAST const *
MappedModule::createASTForFile(llvm::MDNode const *FileNode) {
  // TODO: We should return a seec::Error when this is unsuccessful, so that
  //       we can describe the problem to the user rather than asserting.
  
  // Check lookup to see if we've already loaded the AST.
  auto It = ASTLookup.find(FileNode);
  if (It != ASTLookup.end())
    return It->second;

  // If not, we will try to load the AST from the source file.
  auto const FilenameStr = dyn_cast<MDString>(FileNode->getOperand(0u));
  auto const FileCompileInfo =
    getCompileInfoForMainFile(FilenameStr->getString());
  
  if (!FileCompileInfo) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }
  
  auto CI = FileCompileInfo->createCompilerInvocation(*Diags);
  if (!CI) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }
  
  auto const Invocation = CI.release();
  
  // Create a new ASTUnit.
  std::unique_ptr<ASTUnit> ASTUnit {
    ASTUnit::create(Invocation,
                    Diags,
                    false /* CaptureDiagnostics */,
                    false /* UserFilesAreVolatile */)};
  
  if (!ASTUnit) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }
  
  // Override files in ASTUnit using compile info.
  FileCompileInfo->createVirtualFiles(ASTUnit->getFileManager(),
                                      ASTUnit->getSourceManager());
  
  // Load the ASTUnit.
  auto const LoadedASTUnit =
    ::clang::ASTUnit::LoadFromCompilerInvocationAction(Invocation,
                                                       Diags,
                                                       nullptr /* Action */,
                                                       ASTUnit.get(),
                                                       true /* Persistent */);
  
  if (!LoadedASTUnit) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }
  
  // Create MappedAST from ASTUnit.
  auto AST = MappedAST::FromASTUnit(ASTUnit.release());
  if (!AST) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }

  // Get the raw pointer, because we have to push the unique_ptr onto the list.
  auto const ASTRaw = AST.get();

  ASTLookup[FileNode] = ASTRaw;
  ASTList.emplace_back(std::move(AST));

  return ASTRaw;
}

MappedModule::MappedModule(
                ModuleIndex const &ModIndex,
                llvm::StringRef ExecutablePath,
                llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags)
: ModIndex(ModIndex),
  ExecutablePath(ExecutablePath),
  Diags(Diags),
  ASTLookup(),
  ASTList(),
  MDStmtIdxKind(ModIndex.getModule().getMDKindID(MDStmtIdxStr)),
  MDDeclIdxKind(ModIndex.getModule().getMDKindID(MDDeclIdxStr)),
  FunctionLookup(),
  GlobalVariableLookup(),
  CompileInfo(),
  StmtToMappedStmt(),
  ValueToMappedStmt()
{
  auto const &Module = ModIndex.getModule();
  
  // Load compile information from the Module.
  auto GlobalCompileInfo = Module.getNamedMetadata(MDCompileInfo);
  if (GlobalCompileInfo) {
    for (std::size_t i = 0u; i < GlobalCompileInfo->getNumOperands(); ++i) {
      auto Node = GlobalCompileInfo->getOperand(i);
      auto MappedInfo = MappedCompileInfo::get(Node);
      if (!MappedInfo)
        continue;
            
      CompileInfo.insert(std::make_pair(MappedInfo->getMainFileName(),
                                        std::move(MappedInfo)));
    }
  }
  
  // Create the ASTs for all files. These are required in the following steps.
  auto GlobalIdxMD = Module.getNamedMetadata(MDGlobalDeclIdxsStr);
  if (GlobalIdxMD) {
    for (std::size_t i = 0u; i < GlobalIdxMD->getNumOperands(); ++i) {
      auto Node = GlobalIdxMD->getOperand(i);
      assert(Node && Node->getNumOperands() == 3);

      auto FileNode = dyn_cast<MDNode>(Node->getOperand(0u));
      assert(FileNode);

      auto AST = createASTForFile(FileNode);
      assert(AST);
    }
  }
  
  // Load all parameter mappings (these will be assigned to functions in the
  // next step).
  std::vector<seec::cm::MappedParam> MappedParams;
  auto const MappedParamsMD = Module.getNamedMetadata(MDGlobalParamMapStr);
  
  if (MappedParamsMD) {
    for (std::size_t i = 0u; i < MappedParamsMD->getNumOperands(); ++i) {
      auto MaybeMapped =
        seec::cm::MappedParam::fromMetadata(MappedParamsMD->getOperand(i),
                                            *this);
      
      if (MaybeMapped.assigned<seec::Error>())
        continue; // TODO: Report this.
      
      MappedParams.emplace_back(MaybeMapped.move<seec::cm::MappedParam>());
    }
  }
  
  // Load all local mappings (these will be assigned to functions in the next
  // step).
  std::vector<seec::cm::MappedLocal> MappedLocals;
  auto const MappedLocalsMD = Module.getNamedMetadata(MDGlobalLocalMapStr);
  
  if (MappedLocalsMD) {
    for (std::size_t i = 0u; i < MappedLocalsMD->getNumOperands(); ++i) {
      auto MaybeMapped =
        seec::cm::MappedLocal::fromMetadata(MappedLocalsMD->getOperand(i),
                                            *this);
      
      if (MaybeMapped.assigned<seec::Error>())
        continue; // TODO: Report this.
      
      MappedLocals.emplace_back(MaybeMapped.move<seec::cm::MappedLocal>());
    }
  }
  
  // Create the FunctionLookup and GlobalVariableLookup.
  if (GlobalIdxMD) {
    for (std::size_t i = 0u; i < GlobalIdxMD->getNumOperands(); ++i) {
      // We know that the AST lookup will be successful, because all ASTs have
      // been loaded (above).
      auto Node = GlobalIdxMD->getOperand(i);
      auto FileNode = dyn_cast<MDNode>(Node->getOperand(0u));
      auto AST = getASTForFile(FileNode);
      
      auto FilePath = getPathFromFileNode(FileNode);
      assert(!FilePath.empty());
      
      auto const Global = Node->getOperand(1u);
      if (!Global) {
        DEBUG(dbgs() << "Global is null.\n");
        continue;
      }

      auto const DeclIdx = llvm::dyn_cast<ConstantInt>(Node->getOperand(2u));
      assert(DeclIdx);

      auto const Decl = AST->getDeclFromIdx(DeclIdx->getZExtValue());
      if (!Decl) {
        DEBUG(dbgs() << "Global's Decl is null.\n");
        continue;
      }
      
      if (auto const Func = llvm::dyn_cast<llvm::Function>(Global)) {
        auto FnDecl = llvm::dyn_cast<clang::FunctionDecl>(Decl);
        assert(FnDecl);
        
        clang::FunctionDecl const *FnDefinition = nullptr;
        if (FnDecl->hasBody(FnDefinition)) {
          FnDecl = FnDefinition;
          FunctionLookup.erase(Func);
        }
        else if (FunctionLookup.count(Func)) {
          // Don't overwrite the existing function mapping with one for a
          // non-definition Decl!
          continue;
        }
        
        auto const ParamBegin = FnDecl->param_begin();
        auto const ParamEnd = FnDecl->param_end();
      
        // Find the mapped parameters for this function.
        std::vector<seec::cm::MappedParam> FunctionMappedParams;
        
        for (auto const &MP : MappedParams)
          if (std::find(ParamBegin, ParamEnd, MP.getDecl()) != ParamEnd)
            FunctionMappedParams.emplace_back(MP);
        
        // Find the mapped locals for this function.
        std::vector<seec::cm::MappedLocal> FunctionMappedLocals;
        
        for (auto const &ML : MappedLocals)
          if (AST->isParent(FnDecl, ML.getDecl()))
            FunctionMappedLocals.emplace_back(ML);
        
        FunctionLookup.insert(
          std::make_pair(Func,
                         MappedFunctionDecl(std::move(FilePath),
                                            *AST,
                                            Decl,
                                            Func,
                                            std::move(FunctionMappedParams),
                                            std::move(FunctionMappedLocals))));
      }
      else if (auto const GV = llvm::dyn_cast<llvm::GlobalVariable>(Global)) {
        auto const Recent = Decl->getMostRecentDecl();
        auto const ValueDecl = llvm::dyn_cast<clang::ValueDecl>(Recent);
        if (!ValueDecl) {
          DEBUG(llvm::dbgs() << "Global is not a ValueDecl.\n");
          continue;
        }
        
        if (GlobalVariableLookup.count(GV)) {
          if (ValueDecl->getType()->isIncompleteType())
            continue;
          GlobalVariableLookup.erase(GV);
        }
        
        GlobalVariableLookup.insert(
          std::make_pair(GV,
                         MappedGlobalVariableDecl(*AST,
                                                  ValueDecl,
                                                  GV)));
      }
    }
  }
  
  // Load clang::Stmt to llvm::Value mapping from the Module.
  auto GlobalStmtMaps = Module.getNamedMetadata(MDGlobalValueMapStr);
  if (GlobalStmtMaps) {
    for (std::size_t i = 0u; i < GlobalStmtMaps->getNumOperands(); ++i) {
      auto Mapping = MappedStmt::fromMetadata(GlobalStmtMaps->getOperand(i),
                                              *this);
      if (!Mapping)
        continue;
      
      auto RawPtr = Mapping.get();
      auto Values = RawPtr->getValues();
      
      StmtToMappedStmt.insert(std::make_pair(RawPtr->getStatement(),
                                             std::move(Mapping)));
      
      if (Values.first) {
        ValueToMappedStmt.insert(std::make_pair(Values.first, RawPtr));
      }
      
      if (Values.second) {
        ValueToMappedStmt.insert(std::make_pair(Values.second, RawPtr));
      }
    }
  }
}

MappedModule::~MappedModule() = default;

//------------------------------------------------------------------------------
// MappedModule:: Accessors.
//------------------------------------------------------------------------------

MappedAST const *
MappedModule::getASTForFile(llvm::MDNode const *FileNode) const {
  auto const It = ASTLookup.find(FileNode);
  return It != ASTLookup.end() ? It->second : nullptr;
}

std::vector<MappedAST const *> MappedModule::getASTs() const {
  std::vector<MappedAST const *> ASTs;

  for (auto const &AST : ASTList)
    ASTs.emplace_back(AST.get());

  return ASTs;
}

auto
MappedModule::getASTIndex(MappedAST const *AST) const
-> seec::Maybe<decltype(ASTList)::size_type>
{
  auto const It = std::find_if(ASTList.begin(), ASTList.end(),
    [AST] (std::unique_ptr<MappedAST> const &Item) {
      return Item.get() == AST;
    });

  if (It != ASTList.end())
    return static_cast<decltype(ASTList)::size_type>
                      (std::distance(ASTList.begin(), It));

  return seec::Maybe<decltype(ASTList)::size_type>();
}

MappedAST const *
MappedModule::getASTAtIndex(decltype(ASTList)::size_type const Index) const
{
  return Index < ASTList.size() ? ASTList[Index].get()
                                : nullptr;
}

std::pair<MappedAST const *, clang::Decl const *>
MappedModule::getASTAndDecl(llvm::MDNode const *DeclIdentifier) const {
  assert(DeclIdentifier && DeclIdentifier->getNumOperands() == 2);
  
  auto FileMD = llvm::dyn_cast<llvm::MDNode>(DeclIdentifier->getOperand(0u));
  if (!FileMD)
    return std::pair<MappedAST const *, clang::Decl const *>(nullptr, nullptr);
  
  auto AST = getASTForFile(FileMD);
  auto DeclIdx = llvm::dyn_cast<ConstantInt>(DeclIdentifier->getOperand(1u));
  
  if (!AST || !DeclIdx)
    return std::pair<MappedAST const *, clang::Decl const *>(nullptr, nullptr);
  
  return std::make_pair(AST,
                        AST->getDeclFromIdx(DeclIdx->getZExtValue()));
}

std::pair<MappedAST const *, clang::Stmt const *>
MappedModule::getASTAndStmt(llvm::MDNode const *StmtIdentifier) const {
  assert(StmtIdentifier && StmtIdentifier->getNumOperands() == 2);
  
  auto FileMD = llvm::dyn_cast<llvm::MDNode>(StmtIdentifier->getOperand(0u));
  if (!FileMD)
    return std::pair<MappedAST const *, clang::Stmt const *>(nullptr, nullptr);
  
  auto AST = getASTForFile(FileMD);
  auto StmtIdx = llvm::dyn_cast<ConstantInt>(StmtIdentifier->getOperand(1u));
  
  if (!AST || !StmtIdx)
    return std::pair<MappedAST const *, clang::Stmt const *>(nullptr, nullptr);
  
  return std::make_pair(AST,
                        AST->getStmtFromIdx(StmtIdx->getZExtValue()));
}

//------------------------------------------------------------------------------
// MappedModule:: Find AST from Decl/Stmt.
//------------------------------------------------------------------------------

MappedAST const *
MappedModule::getASTForDecl(::clang::Decl const *Decl) const {
  for (auto const &ASTPtr : ASTList)
    if (ASTPtr->contains(Decl))
      return ASTPtr.get();
  
  return nullptr;
}

MappedAST const *
MappedModule::getASTForStmt(::clang::Stmt const *Stmt) const {
  for (auto const &ASTPtr : ASTList)
    if (ASTPtr->contains(Stmt))
      return ASTPtr.get();
  
  return nullptr;
}

//------------------------------------------------------------------------------
// MappedModule:: Mapped llvm::Function pointers.
//------------------------------------------------------------------------------

MappedFunctionDecl const *
MappedModule::getMappedFunctionDecl(llvm::Function const *F) const {
  auto It = FunctionLookup.find(F);
  if (It == FunctionLookup.end())
    return nullptr;

  return &(It->second);
}

clang::Decl const *MappedModule::getDecl(llvm::Function const *F) const {
  auto It = FunctionLookup.find(F);
  if (It == FunctionLookup.end())
    return nullptr;

  return It->second.getDecl();
}

//------------------------------------------------------------------------------
// MappedModule:: Mapped llvm::GlobalVariable pointers.
//------------------------------------------------------------------------------

MappedGlobalVariableDecl const *
MappedModule::getMappedGlobalVariableDecl(llvm::GlobalVariable const *GV) const
{
  auto It = GlobalVariableLookup.find(GV);
  if (It == GlobalVariableLookup.end())
    return nullptr;

  return &(It->second);
}

clang::Decl const *MappedModule::getDecl(llvm::GlobalVariable const *GV) const {
  auto It = GlobalVariableLookup.find(GV);
  if (It == GlobalVariableLookup.end())
    return nullptr;

  return It->second.getDecl();
}

//------------------------------------------------------------------------------
// MappedModule:: Mapped llvm::Instruction pointers.
//------------------------------------------------------------------------------

MappedInstruction MappedModule::getMapping(llvm::Instruction const *I) const {
  auto DeclMap = getDeclAndMappedAST(I);
  auto StmtMap = getStmtAndMappedAST(I);
  
  // Ensure that the Decl and Stmt come from the same AST.
  if (DeclMap.first && StmtMap.first)
    assert(DeclMap.second == StmtMap.second);
  
  // Find the file path from either the Decl or the Stmt mapping. If there is
  // no mapping, return an empty path.
  std::string FilePath;
  
  if (DeclMap.first) {
    auto DeclIdxNode = I->getMetadata(MDDeclIdxKind);
    auto FileNode = dyn_cast<MDNode>(DeclIdxNode->getOperand(0));
    assert(FileNode);
    FilePath = getPathFromFileNode(FileNode);
  }
  else if (StmtMap.first) {
    auto StmtIdxNode = I->getMetadata(MDStmtIdxKind);
    auto FileNode = dyn_cast<MDNode>(StmtIdxNode->getOperand(0));
    assert(FileNode);
    FilePath = getPathFromFileNode(FileNode);
  }
  
  return MappedInstruction(I,
                           FilePath,
                           DeclMap.second ? DeclMap.second : StmtMap.second,
                           DeclMap.first,
                           StmtMap.first);
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

bool MappedModule::isMappedToStmt(llvm::Instruction const &A) const
{
  return A.getMetadata(MDStmtIdxKind);
}

bool MappedModule::areMappedToSameStmt(llvm::Instruction const &A,
                                       llvm::Instruction const &B) const
{
  return A.getMetadata(MDStmtIdxKind) == B.getMetadata(MDStmtIdxKind);
}


} // namespace seec_clang (in seec)

} // namespace seec
