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

#include <memory>

namespace seec {
  namespace cm {
    class ProcessState;
    class ProcessTrace;
    class ThreadState;
  }
}

namespace clang {
  class Decl;
  class Stmt;
}

class wxArchiveOutputStream;
class wxXmlDocument;
class wxXmlNode;

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

  /// \brief Check if this point is for an AST node.
  ///
  bool isForNode() const;
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
