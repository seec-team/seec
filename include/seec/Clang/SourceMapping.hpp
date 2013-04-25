//===- SourceMapping.hpp --------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_SOURCEMAPPING_HPP
#define SEEC_CLANG_SOURCEMAPPING_HPP

#include "seec/Util/Maybe.hpp"

#include "clang/Frontend/ASTUnit.h"


namespace seec {

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {


/// \brief Hold a simple character location (line, column).
///
struct SimpleLocation {
  int const Line;
  
  int const Column;
  
  SimpleLocation(int TheLine, int TheColumn)
  : Line(TheLine),
    Column(TheColumn)
  {}
};

/// \brief Hold a simple character range.
///
struct SimpleRange {
  SimpleLocation const Start;
  
  SimpleLocation const End;
  
  SimpleRange(SimpleLocation TheStart,
              SimpleLocation TheEnd)
  : Start(TheStart),
    End(TheEnd)
  {}
};


/// \name clang::Decl ranges.
/// @{

/// \brief Get the spelling range of a declaration.
///
seec::Maybe<SimpleRange>
getSpellingRange(clang::Decl const *Decl, clang::ASTUnit const &AST) {
  if (!Decl)
    return seec::Maybe<SimpleRange>();
  
  auto &SourceManager = AST.getSourceManager();
  
  auto SpellStart = SourceManager.getSpellingLoc(Decl->getLocStart());
  auto SpellEnd = SourceManager.getSpellingLoc(Decl->getLocEnd());
  
  bool Invalid = false;
  
  auto StartLine = SourceManager.getSpellingLineNumber(SpellStart, &Invalid);
  auto StartCol = SourceManager.getSpellingColumnNumber(SpellStart, &Invalid);
  
  auto EndLine = SourceManager.getSpellingLineNumber(SpellEnd, &Invalid);
  auto EndCol = SourceManager.getSpellingColumnNumber(SpellEnd, &Invalid);
  
  if (Invalid)
    return seec::Maybe<SimpleRange>();
  
  return SimpleRange(SimpleLocation(StartLine, StartCol),
                     SimpleLocation(EndLine, EndCol));
}

/// \brief Get the "pretty" spelling range, as we want to show it to the user.
///
/// This performs some simple transformations on the spelling range.
seec::Maybe<SimpleRange>
getPrettyVisibleRange(clang::Decl const *Decl, clang::ASTUnit const &AST) {
  auto Range = getSpellingRange(Decl, AST);
  if (!Range.assigned())
    return Range;
  
  // TODO: Transformations.
  
  return Range;
}

/// @}


/// \name clang::Stmt ranges.
/// @{

/// \brief Get the spelling range of a statement.
///
seec::Maybe<SimpleRange>
getSpellingRange(clang::Stmt const *Stmt, clang::ASTUnit const &AST) {
  if (!Stmt)
    return seec::Maybe<SimpleRange>();
  
  auto &SourceManager = AST.getSourceManager();
  
  auto SpellStart = SourceManager.getSpellingLoc(Stmt->getLocStart());
  auto SpellEnd = SourceManager.getSpellingLoc(Stmt->getLocEnd());
  
  bool Invalid = false;
  
  auto StartLine = SourceManager.getSpellingLineNumber(SpellStart, &Invalid);
  auto StartCol = SourceManager.getSpellingColumnNumber(SpellStart, &Invalid);
  
  auto EndLine = SourceManager.getSpellingLineNumber(SpellEnd, &Invalid);
  auto EndCol = SourceManager.getSpellingColumnNumber(SpellEnd, &Invalid);
  
  if (Invalid)
    return seec::Maybe<SimpleRange>();
  
  return SimpleRange(SimpleLocation(StartLine, StartCol),
                     SimpleLocation(EndLine, EndCol));
}

/// \brief Get the "pretty" spelling range, as we want to show it to the user.
///
/// This performs some simple transformations on the spelling range.
seec::Maybe<SimpleRange>
getPrettyVisibleRange(clang::Stmt const *Stmt, clang::ASTUnit const &AST) {
  auto MaybeRange = getSpellingRange(Stmt, AST);
  if (!MaybeRange.assigned())
    return MaybeRange;
  
  auto &Range = MaybeRange.get<0>();
  
  // TODO: We need to handle the situation where we increase the column past
  // the length of the line.
  if (auto Statement = llvm::dyn_cast<clang::DeclRefExpr>(Stmt)) {
    auto Length = Statement->getFoundDecl()->getName().size();
    
    return SimpleRange(Range.Start,
                       SimpleLocation(Range.End.Line,
                                      Range.End.Column + Length - 1));
  }
  
  return std::move(MaybeRange);
}

/// @}


} // namespace seec_clang (in seec)

} // namespace seec


#endif // SEEC_CLANG_SOURCEMAPPING_HPP
