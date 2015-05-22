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
#include "seec/Transforms/BreakConstantGEPs/BreakConstantGEPs.h"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Version.h"
#include "clang/CodeGen/SeeCMapping.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Serialization/ASTWriter.h"

#include "llvm/PassManager.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

using namespace clang;
using namespace llvm;

namespace seec {

namespace seec_clang {

/// \brief Prepare the Module for instrumentation.
/// This uses SAFECode's BreakConstantGEPs pass to transform ConstantExpr
/// getelementptrs into Instructions, so that their values can be recorded.
///
static void PrepareForInstrumentation(llvm::Module &Module)
{
  llvm::PassManager Passes;

  // Add SAFEcode's BreakConstantGEPs pass
  auto const BreakConstantGEPsPass = new llvm::BreakConstantGEPs();
  assert(BreakConstantGEPsPass);
  Passes.add(BreakConstantGEPsPass);

  // Verify the final module
  Passes.add(llvm::createVerifierPass());

  // Run the passes
  Passes.run(Module);
}

//===----------------------------------------------------------------------===//
// class SeeCCodeGenAction
//===----------------------------------------------------------------------===//

std::unique_ptr< ::clang::ASTConsumer >
SeeCCodeGenAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  std::unique_ptr< ::clang::ASTConsumer > Ret;

  Compiler = &CI;
  
  File = InFile.str();
  
  Ret.reset(new SeeCASTConsumer(*this,
                                CodeGenAction::CreateASTConsumer(CI, InFile)));
  
  return Ret;
}

void SeeCCodeGenAction::ModuleComplete(llvm::Module *Mod) {
  auto &SM = Compiler->getSourceManager();
  
  // Transform the Module to be friendlier for the recording process.
  if (Mod)
    PrepareForInstrumentation(*Mod);
  
  // Transform the mapping information to use indices rather than pointers.
  GenerateSerializableMappings(*this, Mod, SM, File);
  
  // Store all used source files into the LLVM Module.
  StoreCompileInformationInModule(Mod, *Compiler, ArgBegin, ArgEnd);
}

void SeeCEmitAssemblyAction::anchor() {}
SeeCEmitAssemblyAction::SeeCEmitAssemblyAction(const char * const *ArgBegin,
                                               const char * const *ArgEnd,
                                               llvm::LLVMContext *_VMContext)
: SeeCCodeGenAction(ArgBegin, ArgEnd, ::clang::Backend_EmitAssembly, _VMContext)
{}

void SeeCEmitBCAction::anchor() {}
SeeCEmitBCAction::SeeCEmitBCAction(const char * const *ArgBegin,
                                   const char * const *ArgEnd,
                                   llvm::LLVMContext *_VMContext)
: SeeCCodeGenAction(ArgBegin, ArgEnd, ::clang::Backend_EmitBC, _VMContext) {}

void SeeCEmitLLVMAction::anchor() {}
SeeCEmitLLVMAction::SeeCEmitLLVMAction(const char * const *ArgBegin,
                                       const char * const *ArgEnd,
                                       llvm::LLVMContext *_VMContext)
: SeeCCodeGenAction(ArgBegin, ArgEnd, ::clang::Backend_EmitLL, _VMContext) {}

void SeeCEmitLLVMOnlyAction::anchor() {}
SeeCEmitLLVMOnlyAction::SeeCEmitLLVMOnlyAction(const char * const *ArgBegin,
                                               const char * const *ArgEnd,
                                               llvm::LLVMContext *_VMContext)
: SeeCCodeGenAction(ArgBegin, ArgEnd, ::clang::Backend_EmitNothing, _VMContext)
{}

void SeeCEmitCodeGenOnlyAction::anchor() {}
SeeCEmitCodeGenOnlyAction::
  SeeCEmitCodeGenOnlyAction(const char * const *ArgBegin,
                            const char * const *ArgEnd,
                            llvm::LLVMContext *_VMContext)
: SeeCCodeGenAction(ArgBegin, ArgEnd, ::clang::Backend_EmitMCNull, _VMContext)
{}

void SeeCEmitObjAction::anchor() {}
SeeCEmitObjAction::SeeCEmitObjAction(const char * const *ArgBegin,
                                     const char * const *ArgEnd,
                                     llvm::LLVMContext *_VMContext)
: SeeCCodeGenAction(ArgBegin, ArgEnd, ::clang::Backend_EmitObj, _VMContext) {}


//===----------------------------------------------------------------------===//
// class SeeCASTConsumer
//===----------------------------------------------------------------------===//

SeeCASTConsumer::~SeeCASTConsumer() {}

bool SeeCASTConsumer::HandleTopLevelDecl(DeclGroupRef D) {
  return Child->HandleTopLevelDecl(D);
}

void SeeCASTConsumer::HandleTranslationUnit(ASTContext &Ctx) {
  TraverseDecl(Ctx.getTranslationUnitDecl());

  for (auto const VAType : VATypes)
    TraverseStmt(VAType->getSizeExpr());

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

bool SeeCASTConsumer::VisitVariableArrayType(::clang::VariableArrayType *T)
{
  VATypes.push_back(T);
  return true;
}


//===----------------------------------------------------------------------===//
// getResourcesDirectory
//===----------------------------------------------------------------------===//

std::string getResourcesDirectory(llvm::StringRef ExecutablePath)
{
  // Find the location of the Clang resources, which should be fixed relative
  // to our executable path.
  // For Bundles find: ../../Resources/clang/CLANG_VERSION_STRING
  // Otherwise find:   ../lib/seec/resources/clang/CLANG_VERSION_STRING
  
  llvm::SmallString<256> ResourcePath {ExecutablePath};
  llvm::sys::path::remove_filename(ResourcePath); // remove executable name
  llvm::sys::path::remove_filename(ResourcePath); // remove "bin" or "MacOS"
  
  if (ResourcePath.str().endswith("Contents")) { // Bundle
    llvm::sys::path::remove_filename(ResourcePath); // remove "Contents"
    llvm::sys::path::append(ResourcePath,
                            "Resources", "clang", CLANG_VERSION_STRING);
  }
  else {
    llvm::sys::path::append(ResourcePath, "lib", "seec", "resources", "clang");
    llvm::sys::path::append(ResourcePath, CLANG_VERSION_STRING);
  }
  
  return ResourcePath.str().str();
}


//===----------------------------------------------------------------------===//
// getRuntimeLibraryDirectory
//===----------------------------------------------------------------------===//

std::string getRuntimeLibraryDirectory(llvm::StringRef ExecutablePath)
{
  // The runtime library location should be fixed relative to our executable
  // path.
  
  // For Windows the runtime is in the same directory.
  // For Bundles find: ???
  // Otherwise find:   ../lib
  
  llvm::SmallString<256> ResourcePath {ExecutablePath};
  llvm::sys::path::remove_filename(ResourcePath); // remove executable name

  // TODO: this should perhaps depend on the target.
#if !defined(_WIN32)
  llvm::sys::path::remove_filename(ResourcePath); // remove "bin" or "MacOS"
  
  if (ResourcePath.str().endswith("Contents")) { // Bundle
    llvm_unreachable("bundle support not implemented.");
  }
  else {
    llvm::sys::path::append(ResourcePath, "lib", "seec");
  }
#endif
  
  return ResourcePath.str().str();
}


//===----------------------------------------------------------------------===//
// GetMetadataPointer
//===----------------------------------------------------------------------===//
template<typename T>
T *GetPointerFromMetadata(llvm::Metadata const *MD) {
  auto CMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(MD);
  assert(CMD && "GetPointerFromMetadata requires ConstantAsMetadata");

  auto CI = llvm::dyn_cast<llvm::ConstantInt>(CMD->getValue());
  assert(CI && "GetPointerFromMetadata requires a ConstantInt.");

  return reinterpret_cast<T *>(static_cast<uintptr_t>(CI->getZExtValue()));
}

template<typename T>
T const *GetMetadataPointer(Instruction const &I, unsigned MDKindID) {
  llvm::MDNode *Node = I.getMetadata(MDKindID);
  if (!Node)
    return nullptr;
  
  assert(Node->getNumOperands() > 0 && "Insufficient operands in MDNode.");
  
  return GetPointerFromMetadata<T const>(Node->getOperand(0).get());
}

//===----------------------------------------------------------------------===//
// GenerateSerializableMappings
//===----------------------------------------------------------------------===//
llvm::Metadata *MakeValueMapSerializable(llvm::Metadata *ValueMap,
                                         seec::ModuleIndex const &ModIndex) {
  if (!ValueMap)
    return ValueMap;
  
  if (llvm::isa<llvm::ConstantAsMetadata>(ValueMap))
    return ValueMap;

  auto &ModContext = ModIndex.getModule().getContext();
  
  auto ValueMapMD = llvm::dyn_cast<llvm::MDNode>(ValueMap);
  
  auto Type = llvm::dyn_cast<llvm::MDString>(ValueMapMD->getOperand(0u));
  auto TypeStr = Type->getString();
  
  if (TypeStr.equals("instruction")) {
    auto const FuncMD = llvm::dyn_cast<llvm::ConstantAsMetadata>
                                      (ValueMapMD->getOperand(1u).get());
    if (!FuncMD)
      return nullptr;

    auto Func = llvm::dyn_cast<llvm::Function>(FuncMD->getValue());
    auto FuncIndex = ModIndex.getFunctionIndex(Func);
    assert(FuncIndex);
    
    auto InstrAddrMD = llvm::dyn_cast<llvm::ConstantAsMetadata>
                                     (ValueMapMD->getOperand(2u).get());
    auto Instr = GetPointerFromMetadata<llvm::Instruction>(InstrAddrMD);
    auto InstrIndex = FuncIndex->getIndexOfInstruction(Instr);
    if (!InstrIndex.assigned())
      return nullptr;
    
    auto Int64Ty = llvm::Type::getInt64Ty(ModContext);
    
    llvm::Metadata *Ops[] = {
      Type,
      FuncMD,
      ConstantAsMetadata::get(ConstantInt::get(Int64Ty, InstrIndex.get<0>()))
    };
    
    return llvm::MDNode::get(ModContext, Ops);
  }
  else {
    return ValueMap;
  }
}

class InstructionMetadataSerializer
{
  llvm::LLVMContext &ModContext;
  llvm::Type * const Int64Ty;
  SeeCCodeGenAction &Action;
  llvm::Metadata * const MainFileNode;
  unsigned const MDStmtPtrID;
  unsigned const MDStmtIdxID;
  unsigned const MDDeclPtrIDClang;
  unsigned const MDDeclPtrID;
  unsigned const MDDeclIdxID;
  unsigned const MDStmtCompletionPtrsID;
  unsigned const MDStmtCompletionIdxsID;
  unsigned const MDDeclCompletionPtrsID;
  unsigned const MDDeclCompletionIdxsID;

  void SerializeStmtMetadata(llvm::Instruction &Instruction) {
    if (auto const S = GetMetadataPointer<Stmt>(Instruction, MDStmtPtrID)) {
      auto &StmtMap = Action.getStmtMap();
      auto It = StmtMap.find(S);
      if (It != StmtMap.end()) {
        llvm::Metadata *Ops[] {
          MainFileNode,
          // StmtIdx
          ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
        };

        Instruction.setMetadata(MDStmtIdxID, MDNode::get(ModContext, Ops));
      }
      
      Instruction.setMetadata(MDStmtPtrID, nullptr);
    }
  }

  void SerializeDeclMetadata(llvm::Instruction &Instruction) {
    if (auto D = GetMetadataPointer<Decl>(Instruction, MDDeclPtrID)) {
      auto &DeclMap = Action.getDeclMap();
      auto It = DeclMap.find(D);
      if (It != DeclMap.end()) {
        llvm::Metadata *Ops[] {
          MainFileNode,
          // DeclIdx
          ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
        };

        Instruction.setMetadata(MDDeclIdxID, MDNode::get(ModContext, Ops));
      }
      
      Instruction.setMetadata(MDDeclPtrID, nullptr);
    }
  }

  void SerializeClangDeclMetadata(llvm::Instruction &Instruction) {
    if (auto D = GetMetadataPointer<Decl>(Instruction, MDDeclPtrIDClang)) {
      auto &DeclMap = Action.getDeclMap();
      auto It = DeclMap.find(D);
      if (It != DeclMap.end()) {
        llvm::Metadata *Ops[] {
          MainFileNode,
          // DeclIdx
          ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
        };

        Instruction.setMetadata(MDDeclIdxID, MDNode::get(ModContext, Ops));
      }
      
      Instruction.setMetadata(MDDeclPtrIDClang, nullptr);
    }
  }

  void SerializeStmtCompletionMetadata(llvm::Instruction &Instruction) {
    if (auto const MD = Instruction.getMetadata(MDStmtCompletionPtrsID)) {
      if (auto const NumOperands = MD->getNumOperands()) {
        auto &StmtMap = Action.getStmtMap();

        std::vector<llvm::Metadata *> Ops;
        Ops.reserve(NumOperands);

        for (unsigned i = 0; i < NumOperands; ++i) {
          if (auto const S = GetPointerFromMetadata<Stmt>(MD->getOperand(i))) {
            auto const It = StmtMap.find(S);
            if (It != StmtMap.end())
              Ops.emplace_back(
                ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second)));
          }
        }

        Instruction.setMetadata(MDStmtCompletionIdxsID,
                                MDNode::get(ModContext, Ops));
      }

      Instruction.setMetadata(MDStmtCompletionPtrsID, nullptr);
    }
  }

  void SerializeDeclCompletionMetadata(llvm::Instruction &Instruction) {
    if (auto const MD = Instruction.getMetadata(MDDeclCompletionPtrsID)) {
      if (auto const NumOperands = MD->getNumOperands()) {
        auto &DeclMap = Action.getDeclMap();

        std::vector<llvm::Metadata *> Ops;
        Ops.reserve(NumOperands);

        for (unsigned i = 0; i < NumOperands; ++i) {
          if (auto const S = GetPointerFromMetadata<Decl>(MD->getOperand(i))) {
            auto const It = DeclMap.find(S);
            if (It != DeclMap.end())
              Ops.emplace_back(
                ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second)));
          }
        }

        Instruction.setMetadata(MDDeclCompletionIdxsID,
                                MDNode::get(ModContext, Ops));
      }

      Instruction.setMetadata(MDDeclCompletionPtrsID, nullptr);
    }
  }

public:
  InstructionMetadataSerializer(llvm::Module *Mod,
                                SeeCCodeGenAction &WithAction,
                                llvm::Metadata * const WithMainFileNode)
  : ModContext(Mod->getContext()),
    Int64Ty(llvm::Type::getInt64Ty(ModContext)),
    Action(WithAction),
    MainFileNode(WithMainFileNode),
    MDStmtPtrID(Mod->getMDKindID(MDStmtPtrStr)),
    MDStmtIdxID(Mod->getMDKindID(MDStmtIdxStr)),
    MDDeclPtrIDClang(Mod->getMDKindID(MDDeclPtrStrClang)),
    MDDeclPtrID(Mod->getMDKindID(MDDeclPtrStr)),
    MDDeclIdxID(Mod->getMDKindID(MDDeclIdxStr)),
    MDStmtCompletionPtrsID(Mod->getMDKindID(MDStmtCompletionPtrsStr)),
    MDStmtCompletionIdxsID(Mod->getMDKindID(MDStmtCompletionIdxsStr)),
    MDDeclCompletionPtrsID(Mod->getMDKindID(MDDeclCompletionPtrsStr)),
    MDDeclCompletionIdxsID(Mod->getMDKindID(MDDeclCompletionIdxsStr))
  {}

  void SerializeInstructionMetadata(llvm::Instruction &Instruction)
  {
    SerializeStmtMetadata(Instruction);
    SerializeDeclMetadata(Instruction);
    SerializeClangDeclMetadata(Instruction);
    SerializeStmtCompletionMetadata(Instruction);
    SerializeDeclCompletionMetadata(Instruction);
  }
};

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

#if defined(SEEC_DEBUG_NODE_MAPPING)
  {
    std::vector< ::clang::Decl const *> Decls;
    Decls.resize(DeclMap.size());
    for (auto const &Pair : DeclMap)
      Decls[Pair.second] = Pair.first;

    std::vector< ::clang::Stmt const *> Stmts;
    Stmts.resize(StmtMap.size());
    for (auto const &Pair : StmtMap)
      Stmts[Pair.second] = Pair.first;

    llvm::errs() << "decls:\n";
    for (auto const D : Decls)
      llvm::errs() << "  " << D->getDeclKindName() << "\n";

    llvm::errs() << "stmts:\n";
    for (auto const S : Stmts)
      llvm::errs() << "  " << S->getStmtClassName() << "\n";
  }
#endif

  llvm::SmallString<256> CurrentDirectory;
  auto const Err = llvm::sys::fs::current_path(CurrentDirectory);
  assert(!Err);

  // setup the file node for the main file and add it to the files node
  llvm::Metadata *MainFileNodeOps[] {
    MDString::get(ModContext, MainFilename),
    MDString::get(ModContext, CurrentDirectory.str())
  };

  auto MainFileNode = MDNode::get(ModContext, MainFileNodeOps);

  // Handle Instruction Metadata
  InstructionMetadataSerializer ISerializer(Mod, Action, MainFileNode);

  for (auto &Function: *Mod) {
    for (auto &BasicBlock: Function) {
      for (auto &Instruction: BasicBlock) {
        if (!Instruction.hasMetadata())
          continue;
        ISerializer.SerializeInstructionMetadata(Instruction);
      }
    }
  }

  // Handle global Decl maps.
  llvm::NamedMDNode *GlobalPtrMD = Mod->getNamedMetadata(MDGlobalDeclPtrsStr);
  if (GlobalPtrMD) {
    llvm::NamedMDNode *GlobalIdxMD
      = Mod->getOrInsertNamedMetadata(MDGlobalDeclIdxsStr);
    llvm::NamedMDNode *GlobalSystemDeclsMD
      = Mod->getOrInsertNamedMetadata(MDGlobalSystemDeclsStr);

    unsigned NumOperands = GlobalPtrMD->getNumOperands();
    for (unsigned i = 0; i < NumOperands; ++i) {
      llvm::MDNode *PtrNode = GlobalPtrMD->getOperand(i);
      assert(PtrNode->getNumOperands() == 2 && "Unexpected NumOperands!");

      auto MDDeclPtr = PtrNode->getOperand(1).get();
      auto D = GetPointerFromMetadata< ::clang::Decl const>(MDDeclPtr);

      auto It = DeclMap.find(D);
      if (It == DeclMap.end())
        continue;
      
      llvm::Metadata *Ops[] = {
        MainFileNode,
        PtrNode->getOperand(0).get(),
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
      };

      GlobalIdxMD->addOperand(llvm::MDNode::get(ModContext, Ops));
      
      if (SM.isInSystemHeader(D->getLocation())) {
        llvm::Metadata *DeclOp[] = { PtrNode->getOperand(0).get() };
        GlobalSystemDeclsMD->addOperand(llvm::MDNode::get(ModContext, DeclOp));
      }
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
      
      auto MDStmtPtr = MappingNode->getOperand(1).get();
      auto Stmt = GetPointerFromMetadata< ::clang::Stmt const>(MDStmtPtr);
      assert(Stmt && "Couldn't get clang::Stmt pointer.");
      
      auto It = StmtMap.find(Stmt);
      if (It == StmtMap.end()) {
        llvm::errs() << "Stmt mapping dropped because Stmt is unknown:\n";
        Stmt->dump();
        continue;
      }
      
      llvm::Metadata *StmtIdentifierOps[] = {
        MainFileNode,
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
      };
      
      // Must convert Instruction and Argument maps to use indices rather than
      // pointers.
      auto Val1 = MakeValueMapSerializable(MappingNode->getOperand(2),
                                           ModIndex);
      
      auto Val2 = MakeValueMapSerializable(MappingNode->getOperand(3),
                                           ModIndex);
      
      // It's possible that an Instruction is deleted after the mapping has
      // been created for it. In this case, discard the entire mapping.
      if (Val1 == nullptr) {
        llvm::errs() << "Stmt mapping has unknown Val1:\n";
        Stmt->dump();
        continue;
      }
      
      llvm::Metadata *MappingOps[] = {
        MappingNode->getOperand(0),
        llvm::MDNode::get(ModContext, StmtIdentifierOps),
        Val1,
        Val2
      };
      
      GlobalStmtIdxMD->addOperand(llvm::MDNode::get(ModContext, MappingOps));
    }
  }
  
  // Handle the global parameter map. Each element in this map has one
  // one reference to a clang::Decl, which is currently a constant int holding
  // the runtime address of the clang::Decl. We must get this value, cast it
  // back to a clang::Decl *, find the index of that Decl using the DeclMap,
  // and then replace the constant int with a Decl identifier MDNode of the
  // form: [MainFileNode, Decl Index].
  auto ParamMapName = seec::clang::ParamMapping::getGlobalMDNameForMapping();
  llvm::NamedMDNode *GlobalParamPtrMD = Mod->getNamedMetadata(ParamMapName);
  if (GlobalParamPtrMD) {
    llvm::NamedMDNode *GlobalParamIdxMD
      = Mod->getOrInsertNamedMetadata(MDGlobalParamMapStr);
    
    unsigned const NumOperands = GlobalParamPtrMD->getNumOperands();
    for (unsigned i = 0; i < NumOperands; ++i) {
      auto const MappingNode = GlobalParamPtrMD->getOperand(i);
      assert(MappingNode->getNumOperands() == 2 && "Unexpected NumOperands!");
      
      auto const MDDeclPtr = MappingNode->getOperand(0).get();
      auto const Decl = GetPointerFromMetadata< ::clang::Decl const>(MDDeclPtr);

      auto const It = DeclMap.find(Decl);
      if (It == DeclMap.end())
        continue;
      
      llvm::Metadata *DeclIdentifierOps[] = {
        MainFileNode,
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
      };

      // Must convert Instruction and Argument maps to use indices rather than
      // pointers.
      auto const Val = MakeValueMapSerializable(MappingNode->getOperand(1),
                                                ModIndex);
      
      // It's possible that an Instruction is deleted after the mapping has
      // been created for it. In this case, discard the entire mapping.
      if (Val == nullptr)
        continue;
      
      llvm::Metadata *MappingOps[] = {
        llvm::MDNode::get(ModContext, DeclIdentifierOps),
        Val
      };
      
      GlobalParamIdxMD->addOperand(llvm::MDNode::get(ModContext, MappingOps));
    }
  }

  // Handle the global local map. Each element in this map has one reference to
  // a clang::VarDecl, which is currently a constant int holding
  // the runtime address of the clang::VarDecl. We must get this value, cast it
  // back to a clang::VarDecl *, find the index of that Decl using the DeclMap,
  // and then replace the constant int with a Decl identifier MDNode of the
  // form: [MainFileNode, Decl Index].
  auto LocalMapName = seec::clang::LocalMapping::getGlobalMDNameForMapping();
  llvm::NamedMDNode *GlobalLocalPtrMD = Mod->getNamedMetadata(LocalMapName);
  if (GlobalLocalPtrMD) {
    llvm::NamedMDNode *GlobalLocalIdxMD
      = Mod->getOrInsertNamedMetadata(MDGlobalLocalMapStr);
    
    unsigned const NumOperands = GlobalLocalPtrMD->getNumOperands();
    for (unsigned i = 0; i < NumOperands; ++i) {
      auto const MappingNode = GlobalLocalPtrMD->getOperand(i);
      assert(MappingNode->getNumOperands() == 2 && "Unexpected NumOperands!");
      
      auto const MDDeclPtr = MappingNode->getOperand(0).get();
      auto const Decl =
        GetPointerFromMetadata< ::clang::VarDecl const>(MDDeclPtr);

      auto const It = DeclMap.find(Decl);
      if (It == DeclMap.end())
        continue;
      
      llvm::Metadata *DeclIdentifierOps[] = {
        MainFileNode,
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, It->second))
      };

      // Must convert Instruction and Argument maps to use indices rather than
      // pointers.
      auto const Val = MakeValueMapSerializable(MappingNode->getOperand(1),
                                                ModIndex);
      
      // It's possible that an Instruction is deleted after the mapping has
      // been created for it. In this case, discard the entire mapping.
      if (Val == nullptr)
        continue;
      
      llvm::Metadata *MappingOps[] = {
        llvm::MDNode::get(ModContext, DeclIdentifierOps),
        Val
      };
      
      GlobalLocalIdxMD->addOperand(llvm::MDNode::get(ModContext, MappingOps));
    }
  }
}

void StoreCompileInformationInModule(llvm::Module *Mod,
                                     ::clang::CompilerInstance &Compiler,
                                     const char * const *ArgBegin,
                                     const char * const *ArgEnd)
{
  assert(Mod && "No module?");
  
  auto &LLVMContext = Mod->getContext();
  auto &SrcManager = Compiler.getSourceManager();
  
  std::vector<llvm::Metadata *> FileInfoNodes;
  std::vector<llvm::Metadata *> ArgNodes;
  
  // Get information about the main file.
  auto MainFileID = SrcManager.getMainFileID();
  auto MainFileEntry = SrcManager.getFileEntryForID(MainFileID);
  
  llvm::SmallString<256> CurrentDirectory;
  auto const Err = llvm::sys::fs::current_path(CurrentDirectory);
  assert(!Err);
  
  llvm::Metadata *MainFileOperands[] = {
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
    llvm::Metadata *Pair[] = {
      NameNode,
      ConstantAsMetadata::get(ContentsNode)
    };

    auto PairNode = llvm::MDNode::get(LLVMContext, Pair);
    
    FileInfoNodes.push_back(PairNode);
  }
  
  // Get all compile argument nodes.
  for (auto ArgIt = ArgBegin; ArgIt != ArgEnd; ++ArgIt)
    if (*ArgIt)
      ArgNodes.push_back(llvm::MDString::get(LLVMContext, *ArgIt));
  
  // Create the compile info node for this unit.
  llvm::Metadata *CompileInfoOperands[] = {
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
