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
#include "clang/Frontend/PCHContainerOperations.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Debug.h"

#include <algorithm>

using namespace clang;
using namespace llvm;

namespace seec {

namespace seec_clang {


template<typename T>
T const *getConstantFrom(llvm::Metadata const *Node)
{
  auto const C = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(Node);
  if (!C)
    return nullptr;

  return llvm::dyn_cast_or_null<T>(C->getValue());
}

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
  if (!CompileInfo || CompileInfo->getNumOperands() != 4)
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

    auto DataNode = llvm::cast<llvm::ConstantAsMetadata>
                              (SourceNode->getOperand(1u).get())
                              ->getValue();
    auto Contents = llvm::dyn_cast<llvm::ConstantDataSequential>(DataNode);

    if (Contents) {
      SourceFiles.emplace_back(Name->getString().str(),
                               Contents->getRawDataValues());
    }
    else {
      SourceFiles.emplace_back(Name->getString().str(), "");
    }
  }
  
  // Extract the invocation arguments.
  std::vector<std::string> InvocationArguments;
  
  for (unsigned i = 0u; i < ArgsNode->getNumOperands(); ++i) {
    auto Str = llvm::dyn_cast<llvm::MDString>(ArgsNode->getOperand(i));
    if (!Str)
      continue;
    
    InvocationArguments.push_back(Str->getString().str());
  }

  // Extract header search information.
  auto const HeaderSearch =
    llvm::dyn_cast<llvm::MDNode>(CompileInfo->getOperand(3u));
  if (!HeaderSearch) {
    llvm::errs() << "no header search node.\n";
    return nullptr;
  }

  auto const HeaderSearchList =
    llvm::dyn_cast<llvm::MDNode>(HeaderSearch->getOperand(0u));
  if (!HeaderSearchList) {
    llvm::errs() << "no header search list node.\n";
    return nullptr;
  }

  std::vector<HeaderSearchEntry> HeaderSearchEntries;
  for (auto const &Op : HeaderSearchList->operands()) {
    auto const OpNode = llvm::dyn_cast<llvm::MDNode>(&*Op);
    if (!OpNode) {
      llvm::errs() << "header search list entry is not an MDNode\n";
      return nullptr;
    }

    auto const Type = llvm::dyn_cast<llvm::MDString>(OpNode->getOperand(0));
    auto const Path = llvm::dyn_cast<llvm::MDString>(OpNode->getOperand(1));
    auto const Kind = llvm::dyn_cast<llvm::MDString>(OpNode->getOperand(2));
    bool IsIndexHeaderMap = false;
    if (!Type || !Path || !Kind) {
      llvm::errs() << "list entry is invalid\n";
      return nullptr;
    }

    ::clang::DirectoryLookup::LookupType_t TypeValue;
    if (Type->getString() == "LT_NormalDir")
      TypeValue = ::clang::DirectoryLookup::LookupType_t::LT_NormalDir;
    else if (Type->getString() == "LT_Framework")
      TypeValue = ::clang::DirectoryLookup::LookupType_t::LT_Framework;
    else if (Type->getString() == "LT_HeaderMap") {
      TypeValue = ::clang::DirectoryLookup::LookupType_t::LT_HeaderMap;

      auto const IdxHeader =
        getConstantFrom<llvm::ConstantInt>(&*OpNode->getOperand(3));

      if (!IdxHeader) {
        llvm::errs() << "HeaderMap with no IsIdxHeader entry\n";
        return nullptr;
      }

      if (IdxHeader->getZExtValue()) {
        IsIndexHeaderMap = true;
      }
    }
    else {
      llvm::errs() << "unknown header type: " << Type->getString() << "\n";
      return nullptr;
    }

    ::clang::SrcMgr::CharacteristicKind KindValue;
    if (Kind->getString() == "C_User")
      KindValue = ::clang::SrcMgr::CharacteristicKind::C_User;
    else if (Kind->getString() == "C_System")
      KindValue = ::clang::SrcMgr::CharacteristicKind::C_System;
    else if (Kind->getString() == "C_ExternCSystem")
      KindValue = ::clang::SrcMgr::CharacteristicKind::C_ExternCSystem;
    else {
      llvm::errs() << "unknown CharacteristicKind: " << Kind->getString()<<"\n";
      return nullptr;
    }

    HeaderSearchEntries.emplace_back(TypeValue,
                                     Path->getString(),
                                     KindValue,
                                     IsIndexHeaderMap);
  }

  // Get index of angle bracket and system header start points.
  auto const AngledIdx =
    getConstantFrom<llvm::ConstantInt>(&*HeaderSearch->getOperand(1u));
  auto const SystemIdx =
    getConstantFrom<llvm::ConstantInt>(&*HeaderSearch->getOperand(2u));
  if (!AngledIdx || !SystemIdx) {
    llvm::errs() << "malformed header search info\n";
    return nullptr;
  }

  return std::unique_ptr<MappedCompileInfo>(
            new MappedCompileInfo(MainDirectory->getString().str(),
                                  MainFileName->getString().str(),
                                  std::move(SourceFiles),
                                  std::move(InvocationArguments),
                                  std::move(HeaderSearchEntries),
                                  AngledIdx->getZExtValue(),
                                  SystemIdx->getZExtValue()));
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
    llvm::SmallString<256> Name {FileInfo.getName()};
    llvm::sys::path::native(Name);

    auto &Contents = FileInfo.getContents();
    auto const Entry = FM.getVirtualFile(Name,
                                         Contents.getBufferSize(),
                                         0 /* ModificationTime */);

    SM.overrideFileContents(Entry,
      llvm::MemoryBuffer::getMemBuffer(Contents.getBuffer(),
                                       Contents.getBufferIdentifier()));
  }
}

void MappedCompileInfo::setHeaderSearchOpts(clang::HeaderSearchOptions &HSOpts)
const
{
  for (unsigned i = 0; i < HeaderSearchEntries.size(); ++i) {
    auto &HS = HeaderSearchEntries[i];

    SmallString<256> Path {HS.getPath()};
    llvm::sys::path::native(Path);

    clang::frontend::IncludeDirGroup Group;

    if (i < HeaderAngledDirIdx) {
      // This is a quoted include path (e.g. "foo.h")
      Group = clang::frontend::IncludeDirGroup::Quoted;
    }
    else if (i < HeaderSystemDirIdx) {
      // Some kind of angled include path (e.g. <foo.h>)
      if (HS.isIndexHeaderMap())
        Group = clang::frontend::IncludeDirGroup::IndexHeaderMap;
      else
        Group = clang::frontend::IncludeDirGroup::Angled;
    }
    else {
      // Some kind of system include path.
      switch (HS.getCharacteristicKind()) {
        case ::clang::SrcMgr::CharacteristicKind::C_User:
          llvm::errs() << "system include path with C_User.\n";
          Group = clang::frontend::IncludeDirGroup::System;
          break;
        case ::clang::SrcMgr::CharacteristicKind::C_System:
          Group = clang::frontend::IncludeDirGroup::System;
          break;
        case ::clang::SrcMgr::CharacteristicKind::C_ExternCSystem:
          Group = clang::frontend::IncludeDirGroup::ExternCSystem;
          break;
      }
    }

    bool const isFramework =
      HS.getLookupType() == DirectoryLookup::LookupType_t::LT_Framework;

    HSOpts.AddPath(Path, Group, isFramework,
                   /* IgnoreSysRoot */ true);
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
  
  // Add header search options.
  auto &HSOpts = CI->getHeaderSearchOpts();
  FileCompileInfo->setHeaderSearchOpts(HSOpts);

  auto const Invocation = CI.release();

  // Create PCHContainerOperations for the ASTUnit load.
  auto PCHContainerOps = std::make_shared<PCHContainerOperations>();
  
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
                                                       PCHContainerOps,
                                                       Diags,
                                                       nullptr /* Action */,
                                                       ASTUnit.get(),
                                                       true /* Persistent */);
  
  if (!LoadedASTUnit) {
    ASTLookup[FileNode] = nullptr;
    return nullptr;
  }
  
  // Create MappedAST from ASTUnit.
  auto AST = MappedAST::FromASTUnit(*FileCompileInfo, ASTUnit.release());
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
                llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags)
: ModIndex(ModIndex),
  Diags(Diags),
  ASTLookup(),
  ASTList(),
  MDStmtIdxKind(ModIndex.getModule().getMDKindID(MDStmtIdxStr)),
  MDDeclIdxKind(ModIndex.getModule().getMDKindID(MDDeclIdxStr)),
  MDStmtCompletionIdxsKind(ModIndex.getModule()
                                   .getMDKindID(MDStmtCompletionIdxsStr)),
  MDDeclCompletionIdxsKind(ModIndex.getModule()
                                   .getMDKindID(MDDeclCompletionIdxsStr)),
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
      
      auto const Global = llvm::cast<llvm::ConstantAsMetadata>
                                    (Node->getOperand(1u).get())
                                    ->getValue();
      if (!Global) {
        DEBUG(dbgs() << "Global is null.\n");
        continue;
      }

      auto const DeclIdxMD = llvm::cast<llvm::ConstantAsMetadata>
                                       (Node->getOperand(2u).get());
      auto const DeclIdx = llvm::dyn_cast<ConstantInt>(DeclIdxMD->getValue());
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
  auto DeclIdxMD = llvm::cast<llvm::ConstantAsMetadata>
                             (DeclIdentifier->getOperand(1u).get());
  auto DeclIdx = llvm::dyn_cast<ConstantInt>(DeclIdxMD->getValue());
  
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
  auto StmtIdxMD = llvm::cast<llvm::ConstantAsMetadata>
                             (StmtIdentifier->getOperand(1u).get());
  auto StmtIdx = llvm::dyn_cast<ConstantInt>(StmtIdxMD->getValue());
  
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

  auto const IdxMD = cast<ConstantAsMetadata>(DeclIdxNode->getOperand(1).get());
  auto const CI = dyn_cast<ConstantInt>(IdxMD->getValue());
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

  auto const IdxMD = cast<ConstantAsMetadata>(DeclIdxNode->getOperand(1).get());
  auto const CI = dyn_cast<ConstantInt>(IdxMD->getValue());
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

  auto const IdxMD = cast<ConstantAsMetadata>(StmtIdxNode->getOperand(1).get());
  auto const CI = dyn_cast<ConstantInt>(IdxMD->getValue());
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

  auto const IdxMD = cast<ConstantAsMetadata>(StmtIdxNode->getOperand(1).get());
  auto const CI = dyn_cast<ConstantInt>(IdxMD->getValue());
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

bool MappedModule::hasCompletionMapping(llvm::Instruction const &I) const
{
  return I.getMetadata(MDStmtCompletionIdxsKind) != nullptr
      || I.getMetadata(MDDeclCompletionIdxsKind) != nullptr;
}

bool
MappedModule::getStmtCompletions(llvm::Instruction const &I,
                                 MappedAST const &MappedAST,
                                 llvm::SmallVectorImpl<clang::Stmt const *> &Out
                                 ) const
{
  auto const MD = I.getMetadata(MDStmtCompletionIdxsKind);
  if (!MD)
    return false;

  auto const NumOperands = MD->getNumOperands();
  for (unsigned i = 0; i < NumOperands; ++i) {
    if (auto const Op = dyn_cast<ConstantAsMetadata>(MD->getOperand(i).get())) {
      if (auto const CI = dyn_cast<ConstantInt>(Op->getValue())) {
        if (auto const Stmt = MappedAST.getStmtFromIdx(CI->getZExtValue())) {
          Out.push_back(Stmt);
        }
      }
    }
  }

  return true;
}

bool
MappedModule::getDeclCompletions(llvm::Instruction const &I,
                                 MappedAST const &MappedAST,
                                 llvm::SmallVectorImpl<clang::Decl const *> &Out
                                 ) const
{
  auto const MD = I.getMetadata(MDDeclCompletionIdxsKind);
  if (!MD)
    return false;

  auto const NumOperands = MD->getNumOperands();
  for (unsigned i = 0; i < NumOperands; ++i) {
    if (auto const Op = dyn_cast<ConstantAsMetadata>(MD->getOperand(i).get())) {
      if (auto const CI = dyn_cast<ConstantInt>(Op->getValue())) {
        if (auto const Decl = MappedAST.getDeclFromIdx(CI->getZExtValue())) {
          Out.push_back(Decl);
        }
      }
    }
  }

  return true;
}


} // namespace seec_clang (in seec)

} // namespace seec
