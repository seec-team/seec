//===- MappedStmt.hpp -----------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MAPPEDSTMT_HPP
#define SEEC_CLANG_MAPPEDSTMT_HPP

#include "llvm/Metadata.h"

#include <memory>

namespace clang {
  class Stmt;
}

namespace llvm {
  class Value;
}

namespace seec {

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {

class MappedAST;
class MappedFile;
class MappedModule;

class MappedStmt {
public:
  enum class Type {
    LValSimple,
    RValScalar,
    RValAggregate
  };

private:
  /// The type of this mapping.
  Type MapType;
  
  /// The AST that the mapped clang::Stmt belongs to.
  MappedAST const *AST;
  
  /// The mapped clang::Stmt.
  clang::Stmt const *Statement;
  
  /// The mapped llvm::Value.
  llvm::Value const *Value1;
  
  /// The (optional) second mapped llvm::Value.
  llvm::Value const *Value2;
  
  /// \brief Constructor.
  MappedStmt(Type TheMapType,
             MappedAST const *TheAST,
             clang::Stmt const *TheStatement,
             llvm::Value const *TheValue1,
             llvm::Value const *TheValue2)
  : MapType(TheMapType),
    AST(TheAST),
    Statement(TheStatement),
    Value1(TheValue1),
    Value2(TheValue2)
  {}
  
public:
  /// \name Constructors.
  /// @{
  
  /// \brief Copy constructor.
  MappedStmt(MappedStmt const &) = default;
  
  /// \brief Move constructor.
  MappedStmt(MappedStmt &&) = default;
  
  /// \brief Read a MappedStmt from metadata.
  static std::unique_ptr<MappedStmt> fromMetadata(llvm::MDNode *Root,
                                                  MappedModule const &Module);
  
  /// @}
  
  
  /// \name Operators.
  /// @{
  
  /// \brief Copy assignment.
  MappedStmt &operator=(MappedStmt const &) = default;
  
  /// \brief Move assignment.
  MappedStmt &operator=(MappedStmt &&) = default;
  
  /// @}
  
  
  /// \name Accessors.
  /// @{
  
  Type getMapType() const { return MapType; }
  
  MappedAST const &getAST() const { return *AST; }
  
  clang::Stmt const *getStatement() const { return Statement; }
  
  llvm::Value const *getValue() const { return Value1; }
  
  std::pair<llvm::Value const *, llvm::Value const *> getValues() const {
    return std::make_pair(Value1, Value2);
  }
  
  /// @}
};

} // namespace seec_clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDSTMT_HPP
