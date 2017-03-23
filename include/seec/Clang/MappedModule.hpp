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

#include "seec/Clang/MappedLocal.hpp"
#include "seec/Clang/MappedParam.hpp"

#include "clang/Frontend/ASTUnit.h"
#include "clang/Lex/DirectoryLookup.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace clang {
  class CompilerInvocation;
  class Decl;
  class DiagnosticsEngine;
  class FileManager;
  class FileSystemOptions;
  class SourceManager;
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
  std::string FilePath;
  
  MappedAST const &AST;

  clang::Decl const *Decl;

  llvm::Function const *Function;
  
  std::vector<seec::cm::MappedParam> MappedParameters;
  
  std::vector<seec::cm::MappedLocal> MappedLocals;

public:
  /// \brief Constructor.
  ///
  MappedFunctionDecl(std::string WithFilePath,
                     MappedAST const &WithAST,
                     clang::Decl const *WithDecl,
                     llvm::Function const *WithFunction,
                     std::vector<seec::cm::MappedParam> WithMappedParameters,
                     std::vector<seec::cm::MappedLocal> WithMappedLocals)
  : FilePath(std::move(WithFilePath)),
    AST(WithAST),
    Decl(WithDecl),
    Function(WithFunction),
    MappedParameters(std::move(WithMappedParameters)),
    MappedLocals(std::move(WithMappedLocals))
  {}

  /// \brief Copy constructor.
  ///
  MappedFunctionDecl(MappedFunctionDecl const &) = default;

  /// \brief Copy assignment.
  ///
  MappedFunctionDecl &operator=(MappedFunctionDecl const &) = default;

  /// \brief Get the path to the source file that this mapping refers to.
  ///
  std::string const &getFilePath() const { return FilePath; }

  /// \brief Get the AST that this clang::Decl belongs to.
  ///
  MappedAST const &getAST() const { return AST; }
  
  /// \brief Get the clang::Decl that is mapped to.
  ///
  clang::Decl const *getDecl() const { return Decl; }

  /// \brief Get the llvm::Function that is mapped from.
  ///
  llvm::Function const *getFunction() const { return Function; }
  
  /// \brief Get the mapped parameters.
  ///
  decltype(MappedParameters) const &getMappedParameters() const {
    return MappedParameters;
  }
  
  /// \brief Get the mapped locals.
  ///
  decltype(MappedLocals) const &getMappedLocals() const {
    return MappedLocals;
  }
  
  /// \name Queries.
  /// @{
  
  /// \brief Check if this Function is defined in a system header.
  ///
  bool isInSystemHeader() const;
  
  /// @} (Queries)
};


/// \brief Represents a mapping from an llvm::GlobalVariable to a clang::Decl.
///
class MappedGlobalVariableDecl {
  MappedAST const &AST;

  clang::ValueDecl const *Decl;

  llvm::GlobalVariable const *Global;
  
  bool const InSystemHeader;
  
  bool const Referenced;

public:
  /// \brief Constructor.
  ///
  MappedGlobalVariableDecl(MappedAST const &AST,
                           clang::ValueDecl const *Decl,
                           llvm::GlobalVariable const *Global);

  /// \brief Copy constructor.
  ///
  MappedGlobalVariableDecl(MappedGlobalVariableDecl const &) = default;

  /// \brief Copy assignment.
  ///
  MappedGlobalVariableDecl &operator=(MappedGlobalVariableDecl const &) =
    default;

  /// \brief Get the AST that this clang::Decl belongs to.
  ///
  MappedAST const &getAST() const { return AST; }
  
  /// \brief Get the clang::Decl that is mapped to.
  ///
  clang::ValueDecl const *getDecl() const { return Decl; }

  /// \brief Get the llvm::GlobalVariable that is mapped from.
  ///
  llvm::GlobalVariable const *getGlobal() const { return Global; }
  
  /// \name Queries.
  /// @{
  
  /// \brief Check if this global is declared in a system header.
  ///
  bool isInSystemHeader() const { return InSystemHeader; }
  
  /// \brief Check if this global is reference by user code.
  ///
  bool isReferenced() const { return Referenced; }
  
  /// @} (Queries)
};


/// \brief Mapping of an Instruction to a Decl or Stmt (possibly neither).
///
class MappedInstruction {
  llvm::Instruction const *Instruction;
  
  std::string FilePath;
  
  MappedAST const *AST;
  
  clang::Decl const *Decl;
  
  clang::Stmt const *Stmt;
  
public:
  /// \brief Constructor.
  MappedInstruction(llvm::Instruction const *Instruction,
                    std::string SourceFilePath,
                    MappedAST const *AST,
                    clang::Decl const *Decl,
                    clang::Stmt const *Stmt)
  : Instruction(Instruction),
    FilePath(std::move(SourceFilePath)),
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
  std::string const &getFilePath() const { return FilePath; }
  
  /// \brief Get the AST for the mapping (if one exists).
  MappedAST const *getAST() const { return AST; }
  
  /// \brief Get the Decl that the Instruction is mapped to (if any).
  clang::Decl const *getDecl() const { return Decl; }
  
  /// \brief Get the Stmt that the Instruction is mapped to (if any).
  clang::Stmt const *getStmt() const { return Stmt; }
};


/// \brief Information about the original Clang compilation.
///
class MappedCompileInfo {
public:
  /// \brief Info for one source file used during compilation.
  class FileInfo {
    /// Name of the source file.
    std::string Name;
    
    /// Contents of the source file.
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
    
    llvm::MemoryBuffer const &getContents() const { return *Contents; }
  };
  
  /// \brief Info for one header search entry used during compilation.
  class HeaderSearchEntry {
    /// Type of entry.
    ::clang::DirectoryLookup::LookupType_t const LookupType;

    /// Path of the file/directory.
    std::string const Path;

    /// Kind of files in directory.
    ::clang::SrcMgr::CharacteristicKind const CharacteristicKind;

    /// Is this an index header map
    bool const IndexHeaderMap;

  public:
    /// \brief Constructor.
    HeaderSearchEntry(::clang::DirectoryLookup::LookupType_t WithLookupType,
                      llvm::StringRef WithPath,
                      ::clang::SrcMgr::CharacteristicKind WithKind,
                      bool IsIndexHeaderMap)
    : LookupType(WithLookupType),
      Path(WithPath.str()),
      CharacteristicKind(WithKind),
      IndexHeaderMap(IsIndexHeaderMap)
    {}

    ::clang::DirectoryLookup::LookupType_t getLookupType() const {
      return LookupType;
    }

    std::string const &getPath() const { return Path; }

    ::clang::SrcMgr::CharacteristicKind getCharacteristicKind() const {
      return CharacteristicKind;
    }

    bool isIndexHeaderMap() const { return IndexHeaderMap; }
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

  /// Header search entries used in this compilation.
  std::vector<HeaderSearchEntry> HeaderSearchEntries;

  /// Index of the first angle bracket directory in \c HeaderSearchEntries.
  unsigned HeaderAngledDirIdx;

  /// Index of the first system directory in \c HeaderSearchEntries.
  unsigned HeaderSystemDirIdx;
  
  /// \brief Constructor.
  ///
  MappedCompileInfo(std::string TheDirectory,
                    std::string TheMainFileName,
                    std::vector<FileInfo> TheSourceFiles,
                    std::vector<std::string> TheInvocationArguments,
                    std::vector<HeaderSearchEntry> TheHeaderSearchEntries,
                    unsigned TheHeaderAngledDirIdx,
                    unsigned TheHeaderSystemDirIdx)
  : MainDirectory(std::move(TheDirectory)),
    MainFileName(std::move(TheMainFileName)),
    SourceFiles(std::move(TheSourceFiles)),
    InvocationArguments(std::move(TheInvocationArguments)),
    HeaderSearchEntries(std::move(TheHeaderSearchEntries)),
    HeaderAngledDirIdx(TheHeaderAngledDirIdx),
    HeaderSystemDirIdx(TheHeaderSystemDirIdx)
  {}

public:
  static std::unique_ptr<MappedCompileInfo> get(llvm::MDNode *CompileInfo);
  
  /// \brief Move constructor.
  ///
  MappedCompileInfo(MappedCompileInfo &&Other) = default;
  
  /// \brief Move assignment.
  ///
  MappedCompileInfo &operator=(MappedCompileInfo &&RHS) = default;
  
  /// \brief Get the name of the main file for this compilation
  ///
  std::string const &getMainFileName() const { return MainFileName; }
  
  /// \brief Get information about the main file for this compilation.
  ///
  FileInfo const *getMainFileInfo() const;
  
  /// \brief Get information about all source files used in this compilation.
  ///
  decltype(SourceFiles) const &getSourceFiles() const {
    return SourceFiles;
  }
  
  /// \brief Get the arguments used for this compilation.
  ///
  decltype(InvocationArguments) const &getInvocationArguments() const {
    return InvocationArguments;
  }

  /// \brief Create a \c CompilerInvocation for this compilation.
  ///
  std::shared_ptr<clang::CompilerInvocation>
  createCompilerInvocation(clang::DiagnosticsEngine &Diags) const;

  /// \brief Create virtual files for all source files in this compilation.
  ///
  void createVirtualFiles(clang::FileManager &FM,
                          clang::SourceManager &SM) const;

  /// \brief Setup header search options from those used in this compilation.
  ///
  void setHeaderSearchOpts(clang::HeaderSearchOptions &HS) const;
};


/// \brief Clang mapping for an llvm::Module.
///
class MappedModule {
  /// Indexed view of the llvm::Module.
  seec::ModuleIndex const &ModIndex;

  /// DiagnosticsEngine used during parsing.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags;

  /// Map file descriptor MDNode pointers to MappedAST objects.
  llvm::DenseMap<llvm::MDNode const *, MappedAST const *> ASTLookup;

  /// Hold the MappedAST objects.
  std::vector<std::unique_ptr<MappedAST>> ASTList;

  /// Kind of clang::Stmt mapping metadata.
  unsigned MDStmtIdxKind;

  /// Kind of clang::Decl mapping metadata.
  unsigned MDDeclIdxKind;

  /// Kind of clang::Stmt completion mapping metadata.
  unsigned MDStmtCompletionIdxsKind;

  /// Kind of clang::Decl completion mapping metadata.
  unsigned MDDeclCompletionIdxsKind;

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

  /// \brief Get or create the AST for the given file.
  ///
  MappedAST const *createASTForFile(llvm::MDNode const *FileNode);

public:
  /// \brief Constructor.
  /// \param ModIndex Indexed view of the llvm::Module to map.
  /// \param Diags The diagnostics engine to use during compilation.
  ///
  MappedModule(seec::ModuleIndex const &ModIndex,
               llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags);

  /// \brief Destructor.
  ///
  ~MappedModule();


  /// \name Accessors.
  /// @{
  
  /// \brief Get the indexed view of the llvm::Module.
  ///
  seec::ModuleIndex const &getModuleIndex() const { return ModIndex; }
  
  /// \brief Get the FunctionLookup.
  ///
  decltype(FunctionLookup) const &getFunctionLookup() const {
    return FunctionLookup;
  }
  
  /// @}
  
  
  /// \name Access ASTs.
  /// @{
  
  /// \brief Get the AST for the given file.
  ///
  MappedAST const *getASTForFile(llvm::MDNode const *FileNode) const;
  
  /// \brief Get all loaded ASTs.
  ///
  std::vector<MappedAST const *> getASTs() const;
  
  /// \brief Get the index of an AST.
  ///
  seec::Maybe<decltype(ASTList)::size_type>
  getASTIndex(MappedAST const *) const;
  
  /// \brief Get the AST at the given index.
  ///
  MappedAST const *getASTAtIndex(decltype(ASTList)::size_type const) const;
  
  /// \brief Get the AST and clang::Decl for the given Declaration Identifier.
  ///
  std::pair<MappedAST const *, clang::Decl const *>
  getASTAndDecl(llvm::MDNode const *DeclIdentifier) const;
  
  /// \brief Get the AST and clang::Stmt for the given Statement Identifier.
  ///
  std::pair<MappedAST const *, clang::Stmt const *>
  getASTAndStmt(llvm::MDNode const *StmtIdentifier) const;
  
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
  ///
  MappedFunctionDecl const *
  getMappedFunctionDecl(llvm::Function const *F) const;

  /// \brief Find the clang::Decl for an llvm::Function, if one exists.
  ///
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
  ///
  MappedInstruction getMapping(llvm::Instruction const *I) const;
  
  /// \brief For the given llvm::Instruction, find the clang::Decl.
  ///
  clang::Decl const *getDecl(llvm::Instruction const *I) const;

  /// For the given llvm::Instruction, find the clang::Decl and the MappedAST
  /// that it belongs to.
  std::pair<clang::Decl const *, MappedAST const *>
  getDeclAndMappedAST(llvm::Instruction const *I) const;

  /// \brief For the given llvm::Instruction, find the clang::Stmt.
  ///
  clang::Stmt const *getStmt(llvm::Instruction const *I) const;

  /// For the given llvm::Instruction, find the clang::Stmt and the MappedAST
  /// that it belongs to.
  std::pair<clang::Stmt const *, MappedAST const *>
  getStmtAndMappedAST(llvm::Instruction const *I) const;
  
  /// \brief Check if an Instruction is mapped to a Stmt.
  /// \return true iff the Instruction has Stmt mapping metadata.
  ///
  bool isMappedToStmt(llvm::Instruction const &A) const;
  
  /// \brief Check if two Instructions are mapped to the same Stmt.
  /// \return true iff the Stmt mapping for both A and B is the same.
  ///
  bool areMappedToSameStmt(llvm::Instruction const &A,
                           llvm::Instruction const &B) const;
  
  /// \brief Check if an Instruction completes a Stmt or Decl.
  ///
  bool hasCompletionMapping(llvm::Instruction const &I) const;
  
  /// \brief Get all Stmt completion mappings for an Instruction.
  ///
  bool getStmtCompletions(llvm::Instruction const &I,
                          MappedAST const &MappedAST,
                          llvm::SmallVectorImpl<clang::Stmt const *> &Out
                          ) const;
  
  /// \brief Get all Decl completion mappings for an Instruction.
  ///
  bool getDeclCompletions(llvm::Instruction const &I,
                          MappedAST const &MappedAST,
                          llvm::SmallVectorImpl<clang::Decl const *> &Out
                          ) const;
  
  /// @}
  
  
  /// \name Mapped compilation info.
  /// @{
  
  /// \name Get all mapped compile info.
  ///
  decltype(CompileInfo) const &getCompileInfoMap() const { return CompileInfo; }
  
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
  
  /// \brief Get all MappedStmt objects for the given clang::Stmt.
  ///
  std::pair<decltype(StmtToMappedStmt)::const_iterator,
            decltype(StmtToMappedStmt)::const_iterator>
  getMappedStmtsForStmt(::clang::Stmt const *S) const {
    return StmtToMappedStmt.equal_range(S);
  }
  
  /// \brief Get all MappedStmt objects containing the given llvm::Value.
  ///
  std::pair<decltype(ValueToMappedStmt)::const_iterator,
            decltype(ValueToMappedStmt)::const_iterator>
  getMappedStmtsForValue(llvm::Value const *Value) const {
    return ValueToMappedStmt.equal_range(Value);
  }
  
  /// @}
};

} // namespace seec_clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDMODULE_HPP
