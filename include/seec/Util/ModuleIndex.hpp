//===- ModuleIndex.hpp - Allow Module lookups by index -------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_MODULEINDEX_HPP
#define SEEC_UTIL_MODULEINDEX_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"

#include <vector>

namespace seec {

class FunctionIndex {
  // The indexed Function.
  // llvm::Function &Function;

  /// Lookup Instructions by their index.
  std::vector<llvm::Instruction *> InstructionPtrByIdx;
  
  /// Map Instructions to their indices.
  llvm::DenseMap<llvm::Instruction const *, uint32_t> InstructionIdxByPtr;

public:
  FunctionIndex(llvm::Function &Function)
  : // Function(Function),
    InstructionPtrByIdx(),
    InstructionIdxByPtr()
  {
    for (auto &BasicBlock: Function) {
      for (auto &Instruction: BasicBlock) {
        uint32_t Idx = static_cast<uint32_t>(InstructionPtrByIdx.size());
        InstructionIdxByPtr[&Instruction] = Idx;
        InstructionPtrByIdx.push_back(&Instruction);
      }
    }
  }

  /// Get the number of Instructions in the indexed Function.
  size_t getInstructionCount() const { return InstructionPtrByIdx.size(); }

  /// Get the Instruction at the given Index in the indexed Function.
  llvm::Instruction *getInstruction(uint32_t Index) const {
    if (Index < InstructionPtrByIdx.size())
      return InstructionPtrByIdx[Index];
    return nullptr;
  }

  /// \brief Get the index of the given Instruction in the indexed Function.
  ///
  /// If the Instruction does not exist in the Function, then the Maybe returned
  /// will be unassigned.
  util::Maybe<uint32_t>
  getIndexOfInstruction(llvm::Instruction const *Instruction) const {
    auto It = InstructionIdxByPtr.find(Instruction);
    if (It != InstructionIdxByPtr.end())
      return util::Maybe<uint32_t>(It->second);
    return util::Maybe<uint32_t>();
  }
};

class ModuleIndex {
  // The indexed Module.
  llvm::Module const &Module;

  /// Lookup GlobalVariables by their index.
  std::vector<llvm::GlobalVariable *> GlobalPtrByIdx;
  
  /// Map GlobalVariables to their indices.
  llvm::DenseMap<llvm::GlobalVariable const *, uint32_t> GlobalIdxByPtr;

  /// Lookup Functions by their index.
  std::vector<llvm::Function *> mutable FunctionPtrByIdx;
  
  /// Map Functions to their indices.
  llvm::DenseMap<llvm::Function const *, uint32_t> mutable FunctionIdxByPtr;

  /// Store FunctionIndexs by the index of the Function.
  std::vector<std::unique_ptr<FunctionIndex>> mutable FunctionIndexByIdx;

  // do not implement
  ModuleIndex(ModuleIndex const &Other) = delete;
  ModuleIndex &operator=(ModuleIndex const &RHS) = delete;

public:
  ModuleIndex(llvm::Module &Module,
              bool const GenerateFunctionIndexForAll = false)
  : Module(Module),
    FunctionPtrByIdx(),
    FunctionIdxByPtr(),
    FunctionIndexByIdx()
  {
    // Index all GlobalVariables
    for (auto GIt = Module.global_begin(), GEnd = Module.global_end();
         GIt != GEnd; ++GIt) {
      auto GPtr = &*GIt;
      GlobalIdxByPtr[GPtr] = GlobalPtrByIdx.size();
      GlobalPtrByIdx.push_back(GPtr);
    }

    // Index all Functions
    for (auto &Function: Module) {
      FunctionIdxByPtr[&Function] = FunctionPtrByIdx.size();
      FunctionPtrByIdx.push_back(&Function);
      if (GenerateFunctionIndexForAll) {
        FunctionIndexByIdx.emplace_back(new FunctionIndex(Function));
      }
      else {
        FunctionIndexByIdx.emplace_back(nullptr); // will be lazily constructed
      }
    }
  }
  
  /// Get the Module.
  llvm::Module const &getModule() const { return Module; }
  
  /// Get the number of global variables in the indexed Module.
  size_t getGlobalCount() const { return GlobalPtrByIdx.size(); }

  /// Get the llvm::GlobalVariable at the given Index, or nullptr, if the Index
  /// is invalid.
  llvm::GlobalVariable *getGlobal(uint32_t Index) const {
    if (Index < GlobalPtrByIdx.size())
      return GlobalPtrByIdx[Index];
    return nullptr;
  }

  /// Get the Index of the given llvm::GlobalVariable.
  util::Maybe<uint32_t>
  getIndexOfGlobal(llvm::GlobalVariable const *Global) const {
    auto It = GlobalIdxByPtr.find(Global);
    if (It != GlobalIdxByPtr.end())
      return util::Maybe<uint32_t>(It->second);
    return util::Maybe<uint32_t>();
  }
  
  /// Get the number of functions in the indexed Module.
  size_t getFunctionCount() const { return FunctionPtrByIdx.size(); }

  /// Get the llvm::Function at the given Index, or nullptr, if the Index is
  /// invalid.
  llvm::Function *getFunction(uint32_t Index) const {
    if (Index < FunctionPtrByIdx.size())
      return FunctionPtrByIdx[Index];
    return nullptr;
  }

  /// Get the Index of the given llvm::Function.
  util::Maybe<uint32_t> getIndexOfFunction(llvm::Function const *Function) const {
    auto It = FunctionIdxByPtr.find(Function);
    if (It != FunctionIdxByPtr.end())
      return util::Maybe<uint32_t>(It->second);
    return util::Maybe<uint32_t>();
  }

  void generateFunctionIndexForAll() const {
    for (std::size_t i = 0; i < FunctionIndexByIdx.size(); ++i) {
      if (!FunctionIndexByIdx[i]) {
        auto &Function = *(FunctionPtrByIdx[i]);
        FunctionIndexByIdx[i].reset(new FunctionIndex(Function));
      }
    }
  }

  FunctionIndex *getFunctionIndex(uint32_t Index) const {
    if (Index >= FunctionIndexByIdx.size())
      return nullptr;

    // if no FunctionIndex exists, construct one now
    if (!FunctionIndexByIdx[Index]) {
      auto &Function = *(FunctionPtrByIdx[Index]);
      FunctionIndexByIdx[Index].reset(new FunctionIndex(Function));
    }

    return FunctionIndexByIdx[Index].get();
  }

  FunctionIndex *getFunctionIndex(llvm::Function const *Function) const {
    auto Idx = getIndexOfFunction(Function);
    if (!Idx.assigned())
      return nullptr;
    return getFunctionIndex(Idx.get<0>());
  }
};

}

#endif // SEEC_UTIL_MODULEINDEX_HPP
