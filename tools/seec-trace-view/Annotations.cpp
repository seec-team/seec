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
#include "seec/ICU/Indexing.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/wxWidgets/XmlNodeIterator.hpp"

#include <unicode/regex.h>

#include <wx/archive.h>
#include <wx/log.h>
#include <wx/xml/xml.h>

#include "Annotations.hpp"

using namespace seec;

//------------------------------------------------------------------------------
// AnnotationIndex
//------------------------------------------------------------------------------

clang::Decl const *AnnotationIndex::getDecl() const
{
  // Example of our Decl identifier: decl:0,10
  UErrorCode Status = U_ZERO_ERROR;
  ::icu::RegexMatcher Matcher("^decl:(\\d+),(\\d+)$", 0, Status);
  if (U_FAILURE(Status))
    return nullptr;

  Matcher.reset(m_Index);
  if (!Matcher.find())
    return nullptr;

  auto const StrASTIndex  = towxString(Matcher.group(1, Status));
  unsigned long ASTIndex = 0;
  if (!StrASTIndex.ToULong(&ASTIndex))
    return nullptr;

  auto const StrDeclIndex = towxString(Matcher.group(2, Status));
  unsigned long DeclIndex = 0;
  if (!StrDeclIndex.ToULong(&DeclIndex))
    return nullptr;

  auto const &ASTs = m_Trace.getMapping().getASTs();
  if (ASTIndex >= ASTs.size())
    return nullptr;

  return ASTs[ASTIndex]->getDeclFromIdx(DeclIndex);
}

clang::Stmt const *AnnotationIndex::getStmt() const
{
  // Example of our Stmt identifier: stmt:0,10
  UErrorCode Status = U_ZERO_ERROR;
  ::icu::RegexMatcher Matcher("^stmt:(\\d+),(\\d+)$", 0, Status);
  if (U_FAILURE(Status))
    return nullptr;

  Matcher.reset(m_Index);
  if (!Matcher.find())
    return nullptr;

  auto const StrASTIndex  = towxString(Matcher.group(1, Status));
  unsigned long ASTIndex = 0;
  if (!StrASTIndex.ToULong(&ASTIndex))
    return nullptr;

  auto const StrStmtIndex = towxString(Matcher.group(2, Status));
  unsigned long StmtIndex = 0;
  if (!StrStmtIndex.ToULong(&StmtIndex))
    return nullptr;

  auto const &ASTs = m_Trace.getMapping().getASTs();
  if (ASTIndex >= ASTs.size())
    return nullptr;

  return ASTs[ASTIndex]->getStmtFromIdx(StmtIndex);
}

//------------------------------------------------------------------------------
// IndexedAnnotationText
//------------------------------------------------------------------------------

IndexedAnnotationText::
IndexedAnnotationText(cm::ProcessTrace const &WithTrace,
                      std::unique_ptr<seec::icu::IndexedString> WithText)
: m_Trace(WithTrace),
  m_Text(std::move(WithText))
{}

Maybe<IndexedAnnotationText>
IndexedAnnotationText::create(cm::ProcessTrace const &WithTrace,
                              wxString const &WithText)
{
  using namespace seec::icu;

  auto MaybeIndexed = IndexedString::from(toUnicodeString(WithText));
  if (!MaybeIndexed.assigned<IndexedString>())
    return Maybe<IndexedAnnotationText>();

  return IndexedAnnotationText(WithTrace,
                               makeUnique<IndexedString>
                                         (MaybeIndexed.move<IndexedString>()));
}

IndexedAnnotationText::~IndexedAnnotationText() = default;

wxString IndexedAnnotationText::getText() const
{
  return towxString(m_Text->getString());
}

Maybe<AnnotationIndex>
IndexedAnnotationText::getPrimaryIndexAt(int32_t const CharPosition) const
{
  auto const It = m_Text->lookupPrimaryIndexAtCharacter(CharPosition);
  if (It != m_Text->getNeedleLookup().end()) {
    return AnnotationIndex(m_Trace,
                           It->first,
                           It->second.getStart(),
                           It->second.getEnd());
  }

  return Maybe<AnnotationIndex>();
}

//------------------------------------------------------------------------------
// AnnotationPoint
//------------------------------------------------------------------------------

bool AnnotationPoint::isForThreadState() const
{
  return m_Node->GetName() == "threadState";
}

bool AnnotationPoint::isForProcessState() const
{
  return m_Node->GetName() == "processState";
}

bool AnnotationPoint::isForDecl() const
{
  return m_Node->GetName() == "decl";
}

bool AnnotationPoint::isForStmt() const
{
  return m_Node->GetName() == "stmt";
}

wxString AnnotationPoint::getText() const
{
  for (auto const &Node : *m_Node) {
    if (Node.GetName() == "text") {
      return Node.GetNodeContent();
    }
  }

  return wxEmptyString;
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
                                       wxString const &NodeType,
                                       unsigned const ForASTIndex,
                                       uint64_t const ForNodeIndex)
{
  auto const It = std::find_if(
    wxXmlNodeIterator(Root.GetChildren()),
    wxXmlNodeIterator(),
    [ForASTIndex, ForNodeIndex, &NodeType] (wxXmlNode const &Node) -> bool {
      if (Node.GetName() != NodeType)
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
                           "decl",
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
                           "stmt",
                           MaybeASTIndex.get<0>(),
                           MaybeIndex.get<uint64_t>());
}
