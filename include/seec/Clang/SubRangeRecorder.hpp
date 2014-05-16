//===- include/seec/Clang/SubRangeRecorder.hpp ----------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_SUBRANGERECORDER_HPP
#define SEEC_CLANG_SUBRANGERECORDER_HPP

#include "llvm/ADT/DenseMap.h"

#include <string>
#include <utility>

namespace llvm {
  class raw_ostream;
}
namespace clang {
  class ASTUnit;
  struct PrintingPolicy;
  class Stmt;
}
namespace seec {
  namespace seec_clang {
    class MappedAST;
  }
}

namespace seec {

class PrintedStmtRange
{
  uint64_t Start;

  std::size_t Length;

public:
  PrintedStmtRange(uint64_t WithStart, std::size_t WithLength)
  : Start(WithStart),
    Length(WithLength)
  {}

  uint64_t getStart() const { return Start; }

  std::size_t getLength() const { return Length; }
};

llvm::DenseMap<clang::Stmt *, PrintedStmtRange>
printStmtAndRecordRanges(llvm::raw_ostream &OS,
                         clang::Stmt const *E,
                         clang::PrintingPolicy &Policy);

/// \brief Represents a range occupied by a \c clang::Stmt in the text of a
///        \c FormattedStmt.
///
class FormattedStmtRange final
{
  /// The first character in this \c clang::Stmt 's range.
  uint64_t const Start;

  /// The length of this \c clang::Stmt 's range.
  std::size_t const Length;

  /// \c true iff this \c clang::Stmt 's real start location is within an
  /// unexpanded macro (thus not truly visible in the \c FormattedStmt).
  bool const StartHidden : 1;

  /// \c true iff this \c clang::Stmt 's real end location is within an
  /// unexpanded macro (thus not truly visible in the \c FormattedStmt).
  bool const EndHidden : 1;

public:
  /// \brief Constructor.
  ///
  FormattedStmtRange(uint64_t const WithStart,
                     std::size_t const WithLength,
                     bool const WithStartHidden,
                     bool const WithEndHidden)
  : Start(WithStart),
    Length(WithLength),
    StartHidden(WithStartHidden),
    EndHidden(WithEndHidden)
  {}

  /// \brief Get the first character in this \c clang::Stmt 's range.
  ///
  uint64_t getStart() const { return Start; }

  /// \brief Get the length of this \c clang::Stmt 's range.
  ///
  std::size_t getLength() const { return Length; }

  /// \return \c true iff this \c clang::Stmt 's real start location is within
  /// an unexpanded macro (thus not truly visible in the \c FormattedStmt).
  bool isStartHidden() const { return StartHidden; }

  /// \return \c true iff this \c clang::Stmt 's real end location is within
  /// an unexpanded macro (thus not truly visible in the \c FormattedStmt).
  bool isEndHidden() const { return EndHidden; }
};

/// \brief A formatted \c clang::Stmt with ranges of sub-statements.
///
/// This class is used to represent a \c clang::Stmt that has been formatted
/// for displaying to the user.
///
class FormattedStmt final
{
  /// The formatted code.
  std::string Code;

  /// Information about the range in the formatted code that is occupied by
  /// the top-level \c clang::Stmt and all of its children.
  llvm::DenseMap<clang::Stmt const *, FormattedStmtRange> StmtRanges;

public:
  /// \brief Constructor.
  ///
  FormattedStmt(std::string WithCode,
                llvm::DenseMap<clang::Stmt const *, FormattedStmtRange> Rs)
  : Code(std::move(WithCode)),
    StmtRanges(std::move(Rs))
  {}

  /// \brief Get the formatted code.
  ///
  std::string const &getCode() const { return Code; }

  /// \brief Get information about the range in the formatted code that is
  /// occupied by the top-level \c clang::Stmt and all of its children.
  ///
  decltype(StmtRanges) const &getStmtRanges() const { return StmtRanges; }

  /// \brief Find the range in the formatted code that is occupied by the given
  ///        \c clang::Stmt - or \c nullptr if the \c clang::Stmt is not
  ///        represented in the formatted code.
  ///
  FormattedStmtRange const *getStmtRange(clang::Stmt const * const S) const;
};

/// \brief Generate a formatted \c clang::Stmt with ranges of sub-statements.
///
/// See \c FormattedStmt for more information.
///
FormattedStmt formatStmtSource(clang::Stmt const *S,
                               seec::seec_clang::MappedAST const &MappedAST);

} // namespace seec

#endif // SEEC_CLANG_SUBRANGERECORDER_HPP
