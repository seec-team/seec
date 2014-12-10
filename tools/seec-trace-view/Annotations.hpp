//===- tools/seec-trace-view/Annotations.hpp ------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_ANNOTATIONS_HPP
#define SEEC_TRACE_VIEW_ANNOTATIONS_HPP

#include "seec/Util/Maybe.hpp"

#include <unicode/unistr.h>

#include <wx/string.h>

#include <memory>

namespace seec {
  namespace cm {
    class ProcessState;
    class ProcessTrace;
    class ThreadState;
  }
  namespace icu {
    class IndexedString;
  }
}

namespace clang {
  class Decl;
  class Stmt;
}

class wxArchiveOutputStream;
class wxXmlDocument;
class wxXmlNode;

/// \brief A single index in an \c IndexedAnnotationText.
/// This must not outlive the \c IndexedAnnotationText that it comes from.
///
class AnnotationIndex final
{
  /// The \c seec::cm::ProcessTrace that the annotation's text relates to.
  seec::cm::ProcessTrace const &m_Trace;

  /// The index key.
  UnicodeString const &m_Index;

  /// The start of this index in the annotation's text.
  int32_t const m_Start;

  /// The end of this index in the annotation's text.
  int32_t const m_End;

public:
  /// \brief Construct a new \c AnnotationIndex.
  ///
  AnnotationIndex(seec::cm::ProcessTrace const &WithTrace,
                  UnicodeString const &WithIndex,
                  int32_t const Start,
                  int32_t const End)
  : m_Trace(WithTrace),
    m_Index(WithIndex),
    m_Start(Start),
    m_End(End)
  {}

  /// \brief Get the index key.
  ///
  UnicodeString const &getIndex() const { return m_Index; }

  /// \brief Get the start position of this index in the annotation's text.
  ///
  int32_t getStart() const { return m_Start; }

  /// \brief Get the end position of this index in the annotation's text.
  ///
  int32_t getEnd() const { return m_End; }

  /// \brief Get the \c clang::Decl that this index refers to, if any.
  ///
  clang::Decl const *getDecl() const;

  /// \brief Get the \c clang::Stmt that this index refers to, if any.
  ///
  clang::Stmt const *getStmt() const;
};

/// \brief Indexed annotation text.
/// This text might contain links to AST nodes or to URLs.
///
class IndexedAnnotationText final
{
  /// The \c seec::cm::ProcessTrace that this annotation text relates to.
  seec::cm::ProcessTrace const &m_Trace;

  /// Holds the underlying \c seec::icu::IndexedString.
  std::unique_ptr<seec::icu::IndexedString> m_Text;

  /// \brief Constructor.
  IndexedAnnotationText(seec::cm::ProcessTrace const &WithTrace,
                        std::unique_ptr<seec::icu::IndexedString> WithText);

public:
  /// \brief Create a new \c IndexedAnnotationText for the given trace, from
  ///        the given text.
  /// If the text is not valid indexed text, then this will return an unassigned
  /// \c Maybe.
  ///
  static seec::Maybe<IndexedAnnotationText>
  create(seec::cm::ProcessTrace const &WithTrace, wxString const &WithText);

  /// \brief Destructor.
  ~IndexedAnnotationText();

  /// \brief Move constructor.
  IndexedAnnotationText(IndexedAnnotationText &&) = default;

  /// \brief Move assignment.
  IndexedAnnotationText &operator=(IndexedAnnotationText &&) = default;

  /// \brief Get the underlying \c seec::icu::IndexedString.
  ///
  seec::icu::IndexedString const &getIndexedString() const { return *m_Text; }

  /// \brief Get the processed annotation text.
  ///
  wxString getText() const;

  /// \brief Get the innermost \c AnnotationIndex at the given character.
  ///
  seec::Maybe<AnnotationIndex> getPrimaryIndexAt(int32_t CharPosition) const;
};

/// \brief Represents a single annotation point.
/// This might be for a particular AST node, a process state, or a thread state.
///
class AnnotationPoint final
{
  wxXmlNode *m_Node;

public:
  /// \brief Construct a new \c AnnotationPoint wrapping the given XML node.
  ///
  AnnotationPoint(wxXmlNode &ForNode)
  : m_Node(&ForNode)
  {}

  /// \brief Check if this point is for a \c seec::cm::ThreadState.
  ///
  bool isForThreadState() const;

  /// \brief Check if this point is for a \c seec::cm::ProcessState.
  ///
  bool isForProcessState() const;

  /// \brief Check if this point is for a \c clang::Decl.
  ///
  bool isForDecl() const;

  /// \brief Check if this point is for a \c clang::Stmt.
  ///
  bool isForStmt() const;

  /// \brief Get annotation text (if any).
  /// \return annotation text or, if there is none, an empty string.
  ///
  wxString getText() const;

  /// \brief Indicates that ClangEPV explanations should be suppressed.
  ///
  bool hasSuppressEPV() const;
};

/// \brief Holds all annotations for an execution trace.
///
class AnnotationCollection final
{
  /// The \c wxXmlDocument defining this augmentation.
  std::unique_ptr<wxXmlDocument> m_XmlDocument;

  /// \brief Construct from the given \c wxXmlDocument.
  ///
  AnnotationCollection(std::unique_ptr<wxXmlDocument> XmlDocument);

public:
  /// \brief Construct an empty \c AnnotationCollection.
  ///
  AnnotationCollection();

  /// \brief Move constructor.
  ///
  AnnotationCollection(AnnotationCollection &&) = default;

  /// \brief Move assignment.
  ///
  AnnotationCollection &operator=(AnnotationCollection &&) = default;

  /// \brief Destructor.
  ///
  ~AnnotationCollection();

  /// \brief Attempt to construct from a \c wxXmlDocument.
  /// If the \c Doc is not a valid annotation document then this method returns
  /// an unassigned \c Maybe.
  ///
  static seec::Maybe<AnnotationCollection>
  fromDoc(std::unique_ptr<wxXmlDocument> Doc);

  /// \brief Get the underlying \c wxXmlDocument.
  ///
  wxXmlDocument const &getXmlDocument() const { return *m_XmlDocument; }

  /// \brief Write to "annotations.xml" in the given archive.
  ///
  bool writeToArchive(wxArchiveOutputStream &Stream);

  /// \brief Get the \c AnnotationPoint for a \c ThreadState (if it exists).
  ///
  seec::Maybe<AnnotationPoint>
  getPointForThreadState(seec::cm::ThreadState const &);

  /// \brief Get the \c AnnotationPoint for a \c ProcessState (if it exists).
  ///
  seec::Maybe<AnnotationPoint>
  getPointForProcessState(seec::cm::ProcessState const &);

  /// \brief Get the \c AnnotationPoint for a Decl (if it exists).
  ///
  seec::Maybe<AnnotationPoint> getPointForNode(seec::cm::ProcessTrace const &,
                                               clang::Decl const *);

  /// \brief Get the \c AnnotationPoint for a Stmt (if it exists).
  ///
  seec::Maybe<AnnotationPoint> getPointForNode(seec::cm::ProcessTrace const &,
                                               clang::Stmt const *);
};

#endif // SEEC_TRACE_VIEW_ANNOTATIONS_HPP
