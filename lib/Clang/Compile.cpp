//===- lib/Clang/Compile.cpp ----------------------------------------------===//
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
#include "seec/Clang/MDNames.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Version.h"
#include "clang/CodeGen/SeeCMapping.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Serialization/ASTWriter.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

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
                        *Diagnostics);

  // Find the location of the Clang resources, which should be fixed relative
  // to our executable path.
  // For Bundles find: ../../Resources/clang/CLANG_VERSION_STRING
  // Otherwise find:   ../lib/seec/resources/clang/CLANG_VERSION_STRING
  
  llvm::sys::Path ResourcePath (ExecutablePath);
  ResourcePath.eraseComponent(); // remove executable name
  ResourcePath.eraseComponent(); // remove "bin" or "MacOS" (bundle)
  
  if (llvm::StringRef(ResourcePath.str()).endswith("Contents")) { // Bundle
    ResourcePath.eraseComponent(); // remove "Contents" (bundle)
    ResourcePath.appendComponent("Resources");
    ResourcePath.appendComponent("clang");
    ResourcePath.appendComponent(CLANG_VERSION_STRING);
  }
  else {
    ResourcePath.appendComponent("lib");
    ResourcePath.appendComponent("seec");
    ResourcePath.appendComponent("resources");
    ResourcePath.appendComponent("clang");
    ResourcePath.appendComponent(CLANG_VERSION_STRING);
  }
  
  if (!ResourcePath.canRead()) {
    llvm::errs() << "Couldn't find resources!\n";
    llvm::errs() << "Path = " << ResourcePath.str() << "\n";
    exit(EXIT_FAILURE);
  }
  
  Driver.ResourceDir = ResourcePath.str();

  // Setup the command-line arguments for the compilation
  char const * CompilationArgs[] {
    "-std=c99",
    "-Wall",
    "-pedantic",
    "-fno-builtin",
    "-fno-stack-protector",
    "-D_FORTIFY_SOURCE=0",
    "-g",
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
T *GetPointerFromMetadata(llvm::Value const *V) {
  auto CI = llvm::dyn_cast<llvm::ConstantInt>(V);
  assert(CI && "GetPointerFromMetadata requires a ConstantInt.");
  return reinterpret_cast<T *>(static_cast<uintptr_t>(CI->getZExtValue()));
}

template<typename T>
T const *GetMetadataPointer(Instruction const &I, unsigned MDKindID) {
  MDNode *Node = I.getMetadata(MDKindID);
  if (!Node)
    return nullptr;
  
  assert(Node->getNumOperands() > 0 && "Insufficient operands in MDNode.");
  
  return GetPointerFromMetadata<T const>(Node->getOperand(0));
}

//===----------------------------------------------------------------------===//
// GenerateSerializableMappings
//===----------------------------------------------------------------------===//
llvm::Value *MakeValueMapSerializable(llvm::Value *ValueMap,
                                      seec::ModuleIndex const &ModIndex) {
  if (!ValueMap)
    return ValueMap;
  
  auto &ModContext = ModIndex.getModule().getContext();
  
  auto ValueMapMD = llvm::dyn_cast<llvm::MDNode>(ValueMap);
  
  auto Type = llvm::dyn_cast<llvm::MDString>(ValueMapMD->getOperand(0u));
  auto TypeStr = Type->getString();
  
  if (TypeStr.equals("instruction")) {
    auto Func = llvm::dyn_cast<llvm::Function>(ValueMapMD->getOperand(1u));
    assert(Func);
    
    auto FuncIndex = ModIndex.getFunctionIndex(Func);
    assert(FuncIndex);
    
    auto InstrAddr = ValueMapMD->getOperand(2u);
    auto Instr = GetPointerFromMetadata<llvm::Instruction>(InstrAddr);
    auto InstrIndex = FuncIndex->getIndexOfInstruction(Instr);
    if (!InstrIndex.assigned())
      return nullptr;
    
    auto Int64Ty = llvm::Type::getInt64Ty(ModContext);
    
    llvm::Value *Ops[] = {
      Type,
      Func,
      ConstantInt::get(Int64Ty, InstrIndex.get<0>())
    };
    
    return MDNode::get(ModContext, Ops);
  }
  else {
    return ValueMap;
  }
}

/// Modify Mod's Metadata to use numbered mappings rather than pointers.
void GenerateSerializableMappings(SeeCCodeGenAction &Action,
                                  llvm::Module *Mod,
                                  SourceManager &SM,
                                  StringRef MainFilename) {
  seec::ModuleIndex ModIndex(*Mod, true);
  
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

  // Handle global Decl maps.
  llvm::NamedMDNode *GlobalPtrMD = Mod->getNamedMetadata(MDGlobalDeclPtrsStr);
  if (GlobalPtrMD) {
    llvm::NamedMDNode *GlobalIdxMD
      = Mod->getOrInsertNamedMetadata(MDGlobalDeclIdxsStr);

    unsigned NumOperands = GlobalPtrMD->getNumOperands();
    for (unsigned i = 0; i < NumOperands; ++i) {
      llvm::MDNode *PtrNode = GlobalPtrMD->getOperand(i);
      assert(PtrNode->getNumOperands() == 2 && "Unexpected NumOperands!");

      auto MDDeclPtr = PtrNode->getOperand(1);
      auto D = GetPointerFromMetadata< ::clang::Decl const>(MDDeclPtr);

      auto It = DeclMap.find(D);
      if (It == DeclMap.end()) {
        llvm::errs() << "couldn't map clang::Decl.\n";
        continue;
      }
      
      llvm::Value *Ops[] = {
        MainFileNode,
        PtrNode->getOperand(0),
        ConstantInt::get(Int64Ty, It->second)
      };

      GlobalIdxMD->addOperand(llvm::MDNode::get(ModContext, Ops));
    }
    
    GlobalPtrMD->eraseFromParent();
  }
  
  // Handle global Stmt maps. Each element in the global Stmt map has one
  // reference to a clang::Stmt, which is currently a constant int holding the
  // runtime address of the clang::Stmt. We must get this value, cast it back
  // to a clang::Stmt *, find the index of that statement using the StmtMap,
  // and then replace the constant int with an Stmt identifier MDNode of the
  // form: [MainFileNode, Stmt Index].
  auto StmtMapName = seec::clang::StmtMapping::getGlobalMDNameForMapping();
  llvm::NamedMDNode *GlobalStmtPtrMD = Mod->getNamedMetadata(StmtMapName);
  if (GlobalStmtPtrMD) {
    llvm::NamedMDNode *GlobalStmtIdxMD
      = Mod->getOrInsertNamedMetadata(MDGlobalValueMapStr);
    
    unsigned NumOperands = GlobalStmtPtrMD->getNumOperands();
    for (unsigned i = 0; i < NumOperands; ++i) {
      auto MappingNode = GlobalStmtPtrMD->getOperand(i);
      assert(MappingNode->getNumOperands() == 4 && "Unexpected NumOperands!");
      
      auto MDStmtPtr = MappingNode->getOperand(1);
      auto Stmt = GetPointerFromMetadata< ::clang::Stmt const>(MDStmtPtr);
      assert(Stmt && "Couldn't get clang::Stmt pointer.");
      
      auto It = StmtMap.find(Stmt);
      if (It == StmtMap.end()) {
        llvm::errs() << "couldn't map clang::Stmt.\n";
        continue;
      }
      
      llvm::Value *StmtIdentifierOps[] = {
        MainFileNode,
        ConstantInt::get(Int64Ty, It->second)
      };
      
      // Must convert Instruction and Argument maps to use indices rather than
      // pointers.
      auto Val1 = MakeValueMapSerializable(MappingNode->getOperand(2),
                                           ModIndex);
      
      auto Val2 = MakeValueMapSerializable(MappingNode->getOperand(3),
                                           ModIndex);
      
      // It's possible that an Instruction is deleted after the mapping has
      // been created for it. In this case, discard the entire mapping.
      if (Val1 == nullptr)
        continue;
      
      llvm::Value *MappingOps[] = {
        MappingNode->getOperand(0),
        llvm::MDNode::get(ModContext, StmtIdentifierOps),
        Val1,
        Val2
      };
      
      GlobalStmtIdxMD->addOperand(llvm::MDNode::get(ModContext, MappingOps));
    }
  }
}

void StoreCompileInformationInModule(llvm::Module *Mod,
                                     ::clang::CompilerInstance &Compiler,
                                     std::vector<std::string> const &Args) {
  assert(Mod && "No module?");
  
  auto &LLVMContext = Mod->getContext();
  auto &SrcManager = Compiler.getSourceManager();
  
  std::vector<llvm::Value *> FileInfoNodes;
  std::vector<llvm::Value *> ArgNodes;
  
  // Get information about the main file.
  auto MainFileID = SrcManager.getMainFileID();
  auto MainFileEntry = SrcManager.getFileEntryForID(MainFileID);
  auto CurrentDirectory = llvm::sys::Path::GetCurrentDirectory();
  
  llvm::Value *MainFileOperands[] = {
    llvm::MDString::get(LLVMContext, MainFileEntry->getName()),
    llvm::MDString::get(LLVMContext, CurrentDirectory.str())
  };
  
  auto MainFileNode = llvm::MDNode::get(LLVMContext, MainFileOperands);
  
  // Get all source file information nodes.
  for (auto It = SrcManager.fileinfo_begin(), End = SrcManager.fileinfo_end();
       It != End;
       ++It) {
    // Get the filename as an MDString.
    auto NameNode = llvm::MDString::get(LLVMContext, It->first->getName());
    
    // Get the file contents as a constant data array.
    llvm::Constant *ContentsNode = nullptr;
    auto Buffer = It->second->getRawBuffer();
    
    if (Buffer) {
      auto Start = reinterpret_cast<uint8_t const *>(Buffer->getBufferStart());
      llvm::ArrayRef<uint8_t> BufferRef(Start, Buffer->getBufferSize());
      ContentsNode = llvm::ConstantDataArray::get(LLVMContext, BufferRef);
    }
    
    // Get an MDNode with the filename and contents.
    llvm::Value *Pair[] = {NameNode, ContentsNode};
    auto PairNode = llvm::MDNode::get(LLVMContext, Pair);
    
    FileInfoNodes.push_back(PairNode);
  }
  
  // Get all compile argument nodes.
  for (auto &Arg : Args) {
    ArgNodes.push_back(llvm::MDString::get(LLVMContext, Arg));
  }
  
  // Create the compile info node for this unit.
  llvm::Value *CompileInfoOperands[] = {
    MainFileNode,
    llvm::MDNode::get(LLVMContext, FileInfoNodes),
    llvm::MDNode::get(LLVMContext, ArgNodes)
  };
  
  auto CompileInfoNode = llvm::MDNode::get(LLVMContext, CompileInfoOperands);
  auto GlobalCompileInfo = Mod->getOrInsertNamedMetadata(MDCompileInfo);
  GlobalCompileInfo->addOperand(CompileInfoNode);
}

} // namespace clang (in seec)

} // namespace seec
