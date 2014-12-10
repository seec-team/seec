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

class wxArchiveOutputStream;
class wxXmlDocument;

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
};

#endif // SEEC_TRACE_VIEW_ANNOTATIONS_HPP
