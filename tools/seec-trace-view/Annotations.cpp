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

#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/XmlNodeIterator.hpp"

#include <wx/archive.h>
#include <wx/xml/xml.h>

#include "Annotations.hpp"

using namespace seec;

//------------------------------------------------------------------------------
// AnnotationPoint
//------------------------------------------------------------------------------

bool AnnotationPoint::isForNode() const
{
  return m_Node->GetName() == "threadState";
}

bool AnnotationPoint::isForProcessState() const
{
  return m_Node->GetName() == "processState";
}

bool AnnotationPoint::isForThreadState() const
{
  return m_Node->GetName() == "threadState";
}

//------------------------------------------------------------------------------
// AnnotationCollection
//------------------------------------------------------------------------------

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
  m_XmlDocument = makeUnique<wxXmlDocument>();

  auto const Root = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "annotations");

  m_XmlDocument->SetRoot(Root);
}

AnnotationCollection::~AnnotationCollection() = default;

Maybe<AnnotationCollection>
AnnotationCollection::fromDoc(std::unique_ptr<wxXmlDocument> Doc)
{
  if (!isAnnotationCollection(*Doc))
    return Maybe<AnnotationCollection>();

  return AnnotationCollection(std::move(Doc));
}

bool AnnotationCollection::writeToArchive(wxArchiveOutputStream &Stream)
{
  return Stream.PutNextEntry("annotations.xml")
      && m_XmlDocument->Save(Stream);
}

Maybe<AnnotationPoint>
AnnotationCollection::getPointForThreadState(cm::ThreadState const &State)
{
  auto const StateID = State.getThreadID();
  auto const StateTime = State.getUnmappedState().getThreadTime();

  auto const It = std::find_if(
    wxXmlNodeIterator(m_XmlDocument->GetRoot()->GetChildren()),
    wxXmlNodeIterator(),
    [StateID, StateTime] (wxXmlNode const &Node) -> bool {
      if (Node.GetName() != "threadState")
        return false;

      auto const StrID = Node.GetAttribute("thread");
      unsigned long ID = 0;
      if (!StrID.ToULong(&ID) || ID != StateID)
        return false;

      auto const StrTime = Node.GetAttribute("time");
      unsigned long Time = 0;
      if (!StrTime.ToULong(&Time) || Time != StateTime)
        return false;

      return true;
    });

  if (It != wxXmlNodeIterator())
    return AnnotationPoint(*It);

  return Maybe<AnnotationPoint>();
}

Maybe<AnnotationPoint>
AnnotationCollection::getPointForProcessState(cm::ProcessState const &State)
{
  auto const StateTime = State.getUnmappedProcessState().getProcessTime();

  auto const It = std::find_if(
    wxXmlNodeIterator(m_XmlDocument->GetRoot()->GetChildren()),
    wxXmlNodeIterator(),
    [StateTime] (wxXmlNode const &Node) -> bool {
      if (Node.GetName() != "processState")
        return false;

      auto const StrTime = Node.GetAttribute("time");
      unsigned long Time = 0;
      if (!StrTime.ToULong(&Time) || Time != StateTime)
        return false;

      return true;
    });

  if (It != wxXmlNodeIterator())
    return AnnotationPoint(*It);

  return Maybe<AnnotationPoint>();
}

namespace {

Maybe<AnnotationPoint> getPointForNode(wxXmlNode &Root,
                                       unsigned const ForASTIndex,
                                       uint64_t const ForNodeIndex)
{
  auto const It = std::find_if(
    wxXmlNodeIterator(Root.GetChildren()),
    wxXmlNodeIterator(),
    [ForASTIndex, ForNodeIndex] (wxXmlNode const &Node) -> bool {
      if (Node.GetName() != "node")
        return false;

      auto const StrASTIndex = Node.GetAttribute("ASTIndex");
      unsigned long ASTIndex = 0;
      if (!StrASTIndex.ToULong(&ASTIndex) || ASTIndex != ForASTIndex)
        return false;

      auto const StrNodeIndex = Node.GetAttribute("nodeIndex");
      unsigned long NodeIndex = 0;
      if (!StrNodeIndex.ToULong(&NodeIndex) || NodeIndex != ForNodeIndex)
        return false;

      return true;
    });

  if (It != wxXmlNodeIterator())
    return AnnotationPoint(*It);

  return Maybe<AnnotationPoint>();
}

}

Maybe<AnnotationPoint>
AnnotationCollection::getPointForNode(cm::ProcessTrace const &Trace,
                                      clang::Decl const *Node)
{
  auto const &Mapping = Trace.getMapping();
  auto const AST = Mapping.getASTForDecl(Node);
  if (!AST)
    return Maybe<AnnotationPoint>();

  auto const MaybeIndex = AST->getIdxForDecl(Node);
  if (!MaybeIndex.assigned<uint64_t>())
    return Maybe<AnnotationPoint>();

  auto const MaybeASTIndex = Mapping.getASTIndex(AST);
  if (!MaybeASTIndex.assigned(0))
    return Maybe<AnnotationPoint>();

  return ::getPointForNode(*(m_XmlDocument->GetRoot()),
                           MaybeASTIndex.get<0>(),
                           MaybeIndex.get<uint64_t>());
}

Maybe<AnnotationPoint>
AnnotationCollection::getPointForNode(cm::ProcessTrace const &Trace,
                                      clang::Stmt const *Node)
{
  auto const &Mapping = Trace.getMapping();
  auto const AST = Mapping.getASTForStmt(Node);
  if (!AST)
    return Maybe<AnnotationPoint>();

  auto const MaybeIndex = AST->getIdxForStmt(Node);
  if (!MaybeIndex.assigned<uint64_t>())
    return Maybe<AnnotationPoint>();

  auto const MaybeASTIndex = Mapping.getASTIndex(AST);
  if (!MaybeASTIndex.assigned(0))
    return Maybe<AnnotationPoint>();

  return ::getPointForNode(*(m_XmlDocument->GetRoot()),
                           MaybeASTIndex.get<0>(),
                           MaybeIndex.get<uint64_t>());
}
