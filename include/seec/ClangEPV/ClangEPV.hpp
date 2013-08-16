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
    // MapDecl.emplace(std::move(Name), Value);
    MapDecl.insert(std::make_pair(std::move(Name), Value));
    return *this;
  }
  
  NodeLinks &add(UnicodeString Name, ::clang::Stmt const *Value) {
    // MapStmt.emplace(std::move(Name), Value);
    MapStmt.insert(std::make_pair(std::move(Name), Value));
    return *this;
  }
  
  /// @}
};


/// \brief Describes linking information for a single character in an
///        explanation.
///
class CharacterLinks {
  /// The primary (innermost) index for this character, if any.
  UnicodeString Index;
  
  /// The start of the primary index range, or -1 if none exists.
  int32_t IndexStart;
  
  /// The end of the primary index range, or -1 if none exists.
  int32_t IndexEnd;
  
  /// Decl associated with the Index, if any.
  ::clang::Decl const *Decl;
  
  /// Stmt associated with the Index, if any.
  ::clang::Stmt const *Stmt;
  
  /// \brief Constructor.
  CharacterLinks(UnicodeString PrimaryIndex,
                 int32_t PrimaryIndexStart,
                 int32_t PrimaryIndexEnd,
                 ::clang::Decl const *PrimaryDecl,
                 ::clang::Stmt const *PrimaryStmt)
  : Index(PrimaryIndex),
    IndexStart(PrimaryIndexStart),
    IndexEnd(PrimaryIndexEnd),
    Decl(PrimaryDecl),
    Stmt(PrimaryStmt)
  {}
  
public:
  static CharacterLinks create()
  {
    return CharacterLinks(UnicodeString(), -1, -1, nullptr, nullptr);
  }
  
  static CharacterLinks create(UnicodeString const &PrimaryIndex,
                               int32_t IndexStart,
                               int32_t IndexEnd,
                               NodeLinks const &Links)
  {
    return CharacterLinks(PrimaryIndex,
                          IndexStart,
                          IndexEnd,
                          Links.getDeclFor(PrimaryIndex),
                          Links.getStmtFor(PrimaryIndex));
  }
  
  /// \name Accessors
  /// @{
  
  UnicodeString const &getPrimaryIndex() const { return Index; }
  
  int32_t getPrimaryIndexStart() const { return IndexStart; }
  
  int32_t getPrimaryIndexEnd() const { return IndexEnd; }
  
  ::clang::Decl const *getPrimaryDecl() const { return Decl; }
  
  ::clang::Stmt const *getPrimaryStmt() const { return Stmt; }
  
  /// @}
};


/// \brief Interface for providing value information.
///
class RuntimeValueLookup {
  /// \brief Check if a value is available for a Statement.
  ///
  virtual
  bool isValueAvailableForImpl(::clang::Stmt const *Statement) const = 0;
  
  /// \brief Get a string describing the current runtime value of Statement.
  ///
  virtual
  std::string getValueStringImpl(::clang::Stmt const *Statement) const = 0;
  
  /// \brief Check if a value is considered to be true, if possible.
  ///
  /// pre: isValueAvailableFor(Statement) == true
  ///
  virtual seec::Maybe<bool>
  getValueAsBoolImpl(::clang::Stmt const *Statement) const = 0;
  
public:
  /// \brief Allow destruction from pointer to this interface.
  ///
  virtual ~RuntimeValueLookup() = default;
  
  /// \brief Check if a value is available for a Statement.
  ///
  bool isValueAvailableFor(::clang::Stmt const *Statement) const {
    return isValueAvailableForImpl(Statement);
  }
  
  /// \brief Get a string describing the current runtime value of Statement.
  ///
  std::string getValueString(::clang::Stmt const *Statement) const {
    return getValueStringImpl(Statement);
  }
  
  /// \brief Check if a value is considered to be true, if possible.
  ///
  /// pre: isValueAvailableFor(Statement) == true
  ///
  seec::Maybe<bool> getValueAsBool(::clang::Stmt const *Statement) const {
    return getValueAsBoolImpl(Statement);
  }
};


/// \brief Lambda-based implementation of RuntimeValueLookup.
///
template<typename IsValueAvailableT,
         typename GetValueStringT,
         typename GetValueAsBoolT>
class RuntimeValueLookupByLambda final : public RuntimeValueLookup {
  /// The callback for isValueAvailableForImpl().
  IsValueAvailableT IsValueAvailable;
  
  /// The callback for getValueStringImpl().
  GetValueStringT GetValueString;
  
  /// The callback for getValueAsBoolImpl().
  GetValueAsBoolT GetValueAsBool;
  
  /// \brief Check if a value is available for a Statement.
  ///
  virtual
  bool isValueAvailableForImpl(::clang::Stmt const *Statement) const override {
    return IsValueAvailable(Statement);
  }
  
  /// \brief Get a string describing the current runtime value of Statement.
  ///
  virtual
  std::string getValueStringImpl(::clang::Stmt const *Statement) const override
  {
    return GetValueString(Statement);
  }
  
  /// \brief Check if a value is considered to be true, if possible.
  ///
  /// pre: isValueAvailableFor(Statement) == true
  ///
  virtual seec::Maybe<bool>
  getValueAsBoolImpl(::clang::Stmt const *Statement) const override {
    return GetValueAsBool(Statement);
  }
  
public:
  /// \brief Construct a new RuntimeValueLookupByLambda.
  ///
  RuntimeValueLookupByLambda(IsValueAvailableT IsValueAvailableFn,
                             GetValueStringT GetValueStringFn,
                             GetValueAsBoolT GetValueAsBoolFn)
  : IsValueAvailable(std::move(IsValueAvailableFn)),
    GetValueString(std::move(GetValueStringFn)),
    GetValueAsBool(std::move(GetValueAsBoolFn))
  {}
};

/// \brief Helper function for creating RuntimeValueLookupByLambda objects.
///
template<typename IsValueAvailableT,
         typename GetValueStringT,
         typename GetValueAsBoolT>
RuntimeValueLookupByLambda<IsValueAvailableT, GetValueStringT, GetValueAsBoolT>
makeRuntimeValueLookupByLambda(IsValueAvailableT IsValueAvailable,
                               GetValueStringT GetValueString,
                               GetValueAsBoolT GetValueAsBool)
{
  return RuntimeValueLookupByLambda<IsValueAvailableT,
                                    GetValueStringT,
                                    GetValueAsBoolT>
                                   (std::move(IsValueAvailable),
                                    std::move(GetValueString),
                                    std::move(GetValueAsBool));
}


/// \brief A textual explanation of a Clang AST node.
///
class Explanation {
public:
  /// \brief All types of nodes that may be explained by an \c Explanation.
  ///
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
  
  /// \brief Get linking information for a single character.
  CharacterLinks getCharacterLinksAt(int32_t Position) const {
    auto const NeedleIt = Description.lookupPrimaryIndexAtCharacter(Position);
    
    if (NeedleIt != Description.getNeedleLookup().end()) {
      return CharacterLinks::create(NeedleIt->first,
                                    NeedleIt->second.getStart(),
                                    NeedleIt->second.getEnd(),
                                    Links);
    }
    else {
      return CharacterLinks::create();
    }
  }
  
  /// @}
};


/// \brief Get an Explanation for a clang::Decl.
///
seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Decl const *Node);

/// \brief Get an Explanation for a clang::Decl, with runtime values.
///
seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Decl const *Node,
        RuntimeValueLookup const &ValueLookup);

/// \brief Get an Explanation for a clang::Stmt.
///
seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Stmt const *Node);

/// \brief Get an Explanation for a clang::Stmt, with runtime values.
///
seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Stmt const *Node,
        RuntimeValueLookup const &ValueLookup);


/// \brief A textual explanation of a clang::Decl.
///
class ExplanationOfDecl : public Explanation {
  /// The \c clang::Decl that this explanation is for.
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
  /// \brief Get the \c clang::Decl that this explanation is for.
  ///
  ::clang::Decl const *getDecl() const { return TheDecl; }
  
  /// \brief Check if Object is an \c ExplanationOfDecl.
  ///
  static bool classof(Explanation const *Object) {
    return Object->getNodeType() == Explanation::ENodeType::Decl;
  }
  
  /// \brief Attempt to create an explanation for a \c clang::Decl.
  ///
  static seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
  create(::clang::Decl const *Node, RuntimeValueLookup const *ValueLookup);
};


/// \brief A textual explanation of a clang::Stmt.
///
class ExplanationOfStmt : public Explanation {
  /// The \c clang::Stmt that this explanation is for.
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
  /// \brief Get the \c clang::Stmt that this explanation is for.
  ///
  ::clang::Stmt const *getStmt() const { return TheStmt; }
  
  /// \brief Check if Object is an \c ExplanationOfStmt.
  ///
  static bool classof(Explanation const *Object) {
    return Object->getNodeType() == Explanation::ENodeType::Stmt;
  }
  
  /// \brief Attempt to create an explanation for a \c clang::Stmt.
  ///
  static seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
  create(::clang::Stmt const *Node, RuntimeValueLookup const *ValueLookup);
};


} // namespace epv (in seec)

} // namespace seec


#endif // SEEC_CLANGEPV_CLANGEPV_HPP
