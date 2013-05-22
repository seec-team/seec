//===- include/seec/clang/MappedModule.hpp --------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file Support use of SeeC's Clang-Mapped llvm::Module objects.
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MAPPEDMODULE_HPP
#define SEEC_CLANG_MAPPEDMODULE_HPP

#include "clang/Frontend/ASTUnit.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace clang {
  class Decl;
  class DiagnosticsEngine;
  class FileSystemOptions;
  class Stmt;
}

namespace seec {

class ModuleIndex;

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {

class MappedAST;
class MappedStmt;


/// \brief Represents a mapping from an llvm::Function to a clang::Decl.
///
class MappedFunctionDecl {
  llvm::sys::Path FilePath;
  
  MappedAST const &AST;

  clang::Decl const *Decl;

  llvm::Function const *Function;

public:
  /// Constructor.
  MappedFunctionDecl(llvm::sys::Path FilePath,
                     MappedAST const &AST,
                     clang::Decl const *Decl,
                     llvm::Function const *Function)
  : FilePath(FilePath),
    AST(AST),
    Decl(Decl),
    Function(Function)
  {}

  /// Copy constructor.
  MappedFunctionDecl(MappedFunctionDecl const &) = default;

  /// Copy assignment.
  MappedFunctionDecl &operator=(MappedFunctionDecl const &) = default;

  /// Get the path to the source file that this mapping refers to.
  llvm::sys::Path const &getFilePath() const { return FilePath; }

  /// Get the AST that this clang::Decl belongs to.
  MappedAST const &getAST() const { return AST; }
  
  /// Get the clang::Decl that is mapped to.
  clang::Decl const *getDecl() const { return Decl; }

  /// Get the llvm::Function that is mapped from.
  llvm::Function const *getFunction() const { return Function; }
};


/// \brief Represents a mapping from an llvm::GlobalVariable to a clang::Decl.
///
class MappedGlobalVariableDecl {
  llvm::sys::Path FilePath;
  
  MappedAST const &AST;

  clang::ValueDecl const *Decl;

  llvm::GlobalVariable const *Global;

public:
  /// Constructor.
  MappedGlobalVariableDecl(llvm::sys::Path FilePath,
                           MappedAST const &AST,
                           clang::ValueDecl const *Decl,
                           llvm::GlobalVariable const *Global)
  : FilePath(FilePath),
    AST(AST),
    Decl(Decl),
    Global(Global)
  {}

  /// Copy constructor.
  MappedGlobalVariableDecl(MappedGlobalVariableDecl const &) = default;

  /// Copy assignment.
  MappedGlobalVariableDecl &operator=(MappedGlobalVariableDecl const &) =
    default;

  /// Get the path to the source file that this mapping refers to.
  llvm::sys::Path const &getFilePath() const { return FilePath; }

  /// Get the AST that this clang::Decl belongs to.
  MappedAST const &getAST() const { return AST; }
  
  /// Get the clang::Decl that is mapped to.
  clang::ValueDecl const *getDecl() const { return Decl; }

  /// Get the llvm::GlobalVariable that is mapped from.
  llvm::GlobalVariable const *getGlobal() const { return Global; }
};


/// \brief Mapping of an Instruction to a Decl or Stmt (possibly neither).
///
class MappedInstruction {
  llvm::Instruction const *Instruction;
  
  llvm::sys::Path FilePath;
  
  MappedAST const *AST;
  
  clang::Decl const *Decl;
  
  clang::Stmt const *Stmt;
  
public:
  /// \brief Constructor.
  MappedInstruction(llvm::Instruction const *Instruction,
                    llvm::sys::Path SourceFilePath,
                    MappedAST const *AST,
                    clang::Decl const *Decl,
                    clang::Stmt const *Stmt)
  : Instruction(Instruction),
    FilePath(SourceFilePath),
    AST(AST),
    Decl(Decl),
    Stmt(Stmt)
  {}
  
  /// \brief Copy constructor.
  MappedInstruction(MappedInstruction const &) = default;
  
  /// \brief Copy assignment.
  MappedInstruction &operator=(MappedInstruction const &) = default;
  
  /// \brief Get the llvm::Instruction for this mapping.
  llvm::Instruction const *getInstruction() const { return Instruction; }
  
  /// \brief Get the path to the source code file.
  llvm::sys::Path getFilePath() const { return FilePath; }
  
  /// \brief Get the AST for the mapping (if one exists).
  MappedAST const *getAST() const { return AST; }
  
  /// \brief Get the Decl that the Instruction is mapped to (if any).
  clang::Decl const *getDecl() const { return Decl; }
  
  /// \brief Get the Stmt that the Instruction is mapped to (if any).
  clang::Stmt const *getStmt() const { return Stmt; }
};


///
class MappedCompileInfo {
public:
  /// \brief Info for one source file used during compilation.
  class FileInfo {
    /// Name of the source file.
    std::string Name;
    
    // Contents of the source file.
    std::unique_ptr<llvm::MemoryBuffer> Contents;
  
  public:
    /// \brief Constructor.
    FileInfo(llvm::StringRef Filename,
             llvm::StringRef FileContents)
    : Name(Filename.str()),
      Contents(llvm::MemoryBuffer::getMemBuffer(FileContents, "", false))
    {}
    
    FileInfo(FileInfo &&Other) = default;
    
    FileInfo &operator=(FileInfo &&RHS) = default;
    
    std::string const &getName() const { return Name; }
  };
  
private:
  /// Working directory of the compilation.
  std::string MainDirectory;
  
  /// Filename of the main file for this compilation.
  std::string MainFileName;
  
  /// Information about all source files used in the compilation.
  std::vector<FileInfo> SourceFiles;
  
  /// Arguments for the invocation of this compilation.
  std::vector<std::string> InvocationArguments;
  
  MappedCompileInfo(std::string &&TheDirectory,
                    std::string &&TheMainFileName,
                    std::vector<FileInfo> &&TheSourceFiles,
                    std::vector<std::string> &&TheInvocationArguments)
  : MainDirectory(std::move(TheDirectory)),
    MainFileName(std::move(TheMainFileName)),
    SourceFiles(std::move(TheSourceFiles)),
    InvocationArguments(std::move(TheInvocationArguments))
  {}

public:
  static std::unique_ptr<MappedCompileInfo> get(llvm::MDNode *CompileInfo);
  
  MappedCompileInfo(MappedCompileInfo &&Other) = default;
  
  MappedCompileInfo &operator=(MappedCompileInfo &&RHS) = default;
  
  std::string const &getMainFileName() const { return MainFileName; }
};


/// \brief Clang mapping for an llvm::Module.
///
class MappedModule {
  /// Indexed view of the llvm::Module.
  seec::ModuleIndex const &ModIndex;
  
  /// Path of the currently-running executable.
  llvm::StringRef ExecutablePath;

  /// DiagnosticsEngine used during parsing.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags;

  /// Map file descriptor MDNode pointers to MappedAST objects.
  llvm::DenseMap<llvm::MDNode const *, MappedAST const *> mutable ASTLookup;

  /// Hold the MappedAST objects.
  std::vector<std::unique_ptr<MappedAST>> mutable ASTList;

  /// Kind of clang::Stmt mapping metadata.
  unsigned MDStmtIdxKind;

  /// Kind of clang::Decl mapping metadata.
  unsigned MDDeclIdxKind;

  /// Map llvm::Function pointers to MappedFunctionDecl objects.
  llvm::DenseMap<llvm::Function const *, MappedFunctionDecl> FunctionLookup;
  
  /// Map llvm::GlobalVariable pointers to MappedGlobalVariableDecl objects.
  llvm::DenseMap<llvm::GlobalVariable const *,
                 MappedGlobalVariableDecl>
    GlobalVariableLookup;
  
  /// Compile information for each main file in this Module.
  std::map<std::string, std::unique_ptr<MappedCompileInfo>> CompileInfo;
  
  /// Lookup from clang::Stmt pointer to MappedStmt objects.
  std::multimap<clang::Stmt const *,
                std::unique_ptr<MappedStmt>> StmtToMappedStmt;
  
  /// Lookup from llvm::Value pointer to MappedStmt objects.
  std::multimap<llvm::Value const *, MappedStmt const *> ValueToMappedStmt;

  // Don't allow copying.
  MappedModule(MappedModule const &Other) = delete;
  MappedModule &operator=(MappedModule const &RHS) = delete;

public:
  /// Constructor.
  /// \param ModIndex Indexed view of the llvm::Module to map.
  /// \param ExecutablePath Used by the Clang driver to find resources.
  /// \param Diags The diagnostics engine to use during compilation.
  MappedModule(seec::ModuleIndex const &ModIndex,
               llvm::StringRef ExecutablePath,
               llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags);

  /// Destructor.
  ~MappedModule();


  /// \name Accessors.
  /// @{
  
  /// \brief Get the indexed view of the llvm::Module.
  seec::ModuleIndex const &getModuleIndex() const { return ModIndex; }
  
  /// \brief Get the AST for the given file.
  MappedAST const *getASTForFile(llvm::MDNode const *FileNode) const;
  
  /// \brief Get the AST and clang::Stmt for the given Statement Identifier.
  std::pair<MappedAST const *, clang::Stmt const *>
  getASTAndStmt(llvm::MDNode const *StmtIdentifier) const;
  
  /// \brief Get the FunctionLookup.
  decltype(FunctionLookup) const &getFunctionLookup() const {
    return FunctionLookup;
  }
  
  /// @}
  
  
  /// \name Find AST from Decl/Stmt.
  /// @{
  
  /// \brief Find the AST that contains the given Decl, if possible.
  ///
  MappedAST const *getASTForDecl(::clang::Decl const *Decl) const;
  
  /// \brief Find the AST that contains the given Stmt, if possible.
  ///
  MappedAST const *getASTForStmt(::clang::Stmt const *Stmt) const;
  
  /// @} (Find AST from Decl/Stmt)


  /// \name Mapped llvm::Function pointers.
  /// @{
  
  /// \brief Find the clang::Decl mapping for an llvm::Function, if one exists.
  MappedFunctionDecl const *
  getMappedFunctionDecl(llvm::Function const *F) const;

  /// \brief Find the clang::Decl for an llvm::Function, if one exists.
  clang::Decl const *getDecl(llvm::Function const *F) const;
  
  /// @}
  
  
  /// \name Mapped llvm::GlobalVariable pointers.
  /// @{
  
  /// \brief Get the GlobalVariableLookup.
  ///
  decltype(GlobalVariableLookup) const &getGlobalVariableLookup() const {
    return GlobalVariableLookup;
  }
  
  /// \brief Find the clang::Decl mapping for an llvm::GlobalVariable, if one
  ///        exists.
  ///
  MappedGlobalVariableDecl const *
  getMappedGlobalVariableDecl(llvm::GlobalVariable const *GV) const;
  
  /// \brief Find the clang::Decl for an llvm::GlobalVariable, if one exists.
  ///
  clang::Decl const *getDecl(llvm::GlobalVariable const *GV) const;
  
  /// @}
  

  /// \name Mapped llvm::Instruction pointers.
  /// @{
  
  /// \brief Get Clang mapping information for the given llvm::Instruction.
  MappedInstruction getMapping(llvm::Instruction const *I) const;
  
  /// \brief For the given llvm::Instruction, find the clang::Decl.
  clang::Decl const *getDecl(llvm::Instruction const *I) const;

  /// For the given llvm::Instruction, find the clang::Decl and the MappedAST
  /// that it belongs to.
  std::pair<clang::Decl const *, MappedAST const *>
  getDeclAndMappedAST(llvm::Instruction const *I) const;

  /// \brief For the given llvm::Instruction, find the clang::Stmt.
  clang::Stmt const *getStmt(llvm::Instruction const *I) const;

  /// For the given llvm::Instruction, find the clang::Stmt and the MappedAST
  /// that it belongs to.
  std::pair<clang::Stmt const *, MappedAST const *>
  getStmtAndMappedAST(llvm::Instruction const *I) const;
  
  /// @}
  
  
  /// \name Mapped compilation info.
  /// @{
  
  /// \name Get mapped compile info for a main file, if it exists.
  ///
  MappedCompileInfo const *
  getCompileInfoForMainFile(std::string const &Path) const {
    auto const It = CompileInfo.find(Path);
    return It != CompileInfo.end() ? It->second.get()
                                   : nullptr;
  }
  
  /// @}
  
  
  /// \name Mapped clang::Stmt pointers.
  /// @{
  
  /// \brief Get the MappedStmt object for the given clang::Stmt.
  ///
  MappedStmt const *getMappedStmtForStmt(::clang::Stmt const *S) const {
    auto It = StmtToMappedStmt.find(S);
    return It != StmtToMappedStmt.end() ? It->second.get() : nullptr;
  }
  
  /// \brief Get all MappedStmt objects containing the given llvm::Value.
  std::pair<decltype(ValueToMappedStmt)::const_iterator,
            decltype(ValueToMappedStmt)::const_iterator>
  getMappedStmtsForValue(llvm::Value const *Value) const {
    return ValueToMappedStmt.equal_range(Value);
  }
  
  /// @}
};

} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDMODULE_HPP
