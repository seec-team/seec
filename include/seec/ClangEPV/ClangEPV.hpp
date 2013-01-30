//===- include/seec/ClangEPV/ClangEPV.hpp ---------------------------------===//
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

#ifndef SEEC_CLANGEPV_CLANGEPV_HPP
#define SEEC_CLANGEPV_CLANGEPV_HPP


#include "seec/ICU/Indexing.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "unicode/unistr.h"

#include <map>
#include <memory>
#include <vector>


namespace clang {
  class Decl;
  class Stmt;
}


namespace seec {

namespace clang_epv {


/// \brief Reference various AST nodes by strings.
///
class NodeLinks {
  std::map<UnicodeString, ::clang::Decl const *> MapDecl;
  
  std::map<UnicodeString, ::clang::Stmt const *> MapStmt;
  
public:
  /// \name Constructors.
  /// @{
  
  NodeLinks()
  : MapDecl(),
    MapStmt()
  {}
  
  NodeLinks(NodeLinks const &) = default;
  
  NodeLinks(NodeLinks &&) = default;
  
  /// @}
  
  
  /// \name Assignment.
  /// @{
  
  NodeLinks &operator=(NodeLinks const &) = default;
  
  NodeLinks &operator=(NodeLinks &&) = default;
  
  /// @}
  
  
  /// \name Accessors.
  /// @{
  
  ::clang::Decl const *getDeclFor(UnicodeString const &Name) const {
    auto It = MapDecl.find(Name);
    return It != MapDecl.end() ? It->second : nullptr;
  }
  
  ::clang::Stmt const *getStmtFor(UnicodeString const &Name) const {
    auto It = MapStmt.find(Name);
    return It != MapStmt.end() ? It->second : nullptr;
  }
  
  /// @}
  
  
  /// \name Mutators.
  /// @{
  
  NodeLinks &add(UnicodeString Name, ::clang::Decl const *Value) {
    MapDecl.emplace(std::move(Name), Value);
    return *this;
  }
  
  NodeLinks &add(UnicodeString Name, ::clang::Stmt const *Value) {
    MapStmt.emplace(std::move(Name), Value);
    return *this;
  }
  
  /// @}
};


/// \brief A textual explanation of a Clang AST node.
///
class Explanation {
public:
  enum class ENodeType {
    Decl,
    Stmt
  };
  
protected:
  /// The type of this Explanation's node.
  ENodeType const NodeType;
  
  /// Textual description of the node.
  seec::icu::IndexedString Description;
  
  /// Links for this node.
  NodeLinks Links;
  
  /// \brief Constructor.
  ///
  Explanation(ENodeType ForNodeType,
              seec::icu::IndexedString WithDescription,
              NodeLinks WithLinks)
  : NodeType(ForNodeType),
    Description(std::move(WithDescription)),
    Links(std::move(WithLinks))
  {}
  
  Explanation(Explanation const &) = delete;
  
  Explanation(Explanation &&) = delete;
  
  Explanation &operator=(Explanation const &) = delete;
  
  Explanation &operator=(Explanation &&) = delete;
  
public:
  /// \name Accessors.
  /// @{
  
  /// \brief Get the type of this Explanation's node.
  ENodeType getNodeType() const { return NodeType; }
  
  /// \brief Get a textual description of the node.
  UnicodeString const &getString() const { return Description.getString(); }
  
  /// @}
};


/// \brief Get an Explanation for a clang::Decl.
///
seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Decl const *Node);


/// \brief Get an Explanation for a clang::Stmt.
///
seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Stmt const *Node);


/// \brief A textual explanation of a clang::Decl.
///
class ExplanationOfDecl : public Explanation {
  ::clang::Decl const * const TheDecl;
  
  /// \brief Constructor.
  ///
  ExplanationOfDecl(::clang::Decl const * const Decl,
                    seec::icu::IndexedString WithDescription,
                    NodeLinks WithLinks)
  : Explanation(Explanation::ENodeType::Decl,
                std::move(WithDescription),
                std::move(WithLinks)),
    TheDecl(Decl)
  {}
  
public:
  
  ::clang::Decl const *getDecl() const { return TheDecl; }
  
  static bool classof(Explanation const *Object) {
    return Object->getNodeType() == Explanation::ENodeType::Decl;
  }
  
  static seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
  create(::clang::Decl const *Node);
};


/// \brief A textual explanation of a clang::Stmt.
///
class ExplanationOfStmt : public Explanation {
  ::clang::Stmt const * const TheStmt;
  
  /// \brief Constructor.
  ///
  ExplanationOfStmt(::clang::Stmt const * const Stmt,
                    seec::icu::IndexedString WithDescription,
                    NodeLinks WithLinks)
  : Explanation(Explanation::ENodeType::Stmt,
                std::move(WithDescription),
                std::move(WithLinks)),
    TheStmt(Stmt)
  {}

public:
  
  ::clang::Stmt const *getStmt() const { return TheStmt; }
  
  static bool classof(Explanation const *Object) {
    return Object->getNodeType() == Explanation::ENodeType::Stmt;
  }
  
  static seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
  create(::clang::Stmt const *Node);
};


} // namespace epv (in seec)

} // namespace seec


#endif // SEEC_CLANGEPV_CLANGEPV_HPP
