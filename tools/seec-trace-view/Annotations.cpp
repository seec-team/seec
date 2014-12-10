//===- tools/seec-trace-view/Annotations.cpp ------------------------------===//
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

#include "seec/Util/MakeUnique.hpp"

#include <wx/archive.h>
#include <wx/xml/xml.h>

#include "Annotations.hpp"

namespace {

bool isAnnotationCollection(wxXmlDocument &Doc)
{
  if (!Doc.IsOk())
    return false;

  auto const RootNode = Doc.GetRoot();
  if (!RootNode || RootNode->GetName() != "annotations")
    return false;

  return true;
}

} // anonymous namespace

AnnotationCollection::
AnnotationCollection(std::unique_ptr<wxXmlDocument> XmlDocument)
: m_XmlDocument(std::move(XmlDocument))
{}

AnnotationCollection::AnnotationCollection()
{
  m_XmlDocument = seec::makeUnique<wxXmlDocument>();

  auto const Root = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "annotations");

  m_XmlDocument->SetRoot(Root);
}

AnnotationCollection::~AnnotationCollection() = default;

seec::Maybe<AnnotationCollection>
AnnotationCollection::fromDoc(std::unique_ptr<wxXmlDocument> Doc)
{
  if (!isAnnotationCollection(*Doc))
    return seec::Maybe<AnnotationCollection>();

  return AnnotationCollection(std::move(Doc));
}

bool AnnotationCollection::writeToArchive(wxArchiveOutputStream &Stream)
{
  return Stream.PutNextEntry("annotations.xml")
      && m_XmlDocument->Save(Stream);
}
