//===- tools/seec-trace-view/ActionRecord.cpp -----------------------------===//
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

#include <wx/wx.h>
#include <wx/debug.h>

#include "ActionRecord.hpp"

#include <utility>
#include <vector>


constexpr inline uint32_t formatVersion() { return 1; }


/// \brief Create a list of XML attributes from a vector of string pairs.
///
static wxXmlAttribute *
CreateAttributes(std::vector<std::pair<wxString, wxString>> const &As)
{
  wxXmlAttribute *First = nullptr;
  wxXmlAttribute *Last = nullptr;
  
  for (auto const &A : As) {
    auto const Attr = new wxXmlAttribute(A.first, A.second);
    if (Last) {
      Last->SetNext(Attr);
    }
    else {
      First = Attr;
    }
    Last = Attr;
  }
  
  return First;
}

/// \brief Get the elapsed time attribute.
///
static std::pair<std::string, std::string>
GetElapsedTime(std::chrono::time_point<std::chrono::steady_clock> const Since)
{
  auto const Elapsed = std::chrono::steady_clock::now() - Since;
  auto const ElapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                   (Elapsed);
  return std::make_pair("time", std::to_string(ElapsedMS.count()));
}


//------------------------------------------------------------------------------
// IAttributeReadOnly
//------------------------------------------------------------------------------

IAttributeReadOnly::~IAttributeReadOnly()
{}


//------------------------------------------------------------------------------
// ActionRecord
//------------------------------------------------------------------------------

ActionRecord::ActionRecord()
: Enabled(false),
  Started(std::chrono::steady_clock::now()),
  RecordDocument(new wxXmlDocument()),
  LastNode(nullptr)
{
  auto const Attrs = CreateAttributes({
    std::make_pair("version", std::to_string(formatVersion()))
  });
  
  auto const Root = new wxXmlNode(nullptr,
                                  wxXML_ELEMENT_NODE,
                                  "recording",
                                  wxEmptyString,
                                  Attrs);
  
  RecordDocument->SetRoot(Root);
}

ActionRecord::~ActionRecord()
{
  RecordDocument->Save("record.xml");
}

void ActionRecord::enable()
{
  Enabled = true;
}

void ActionRecord::disable()
{
  Enabled = false;
}

void
ActionRecord::recordEventV(std::string const &Handler,
                           std::vector<IAttributeReadOnly const *> const &Attrs)
{
  if (!Enabled)
    return;
  
  auto const Root = RecordDocument->GetRoot();
  if (!Root)
    return;
  
  // Create the standard attributes.
  std::vector<std::pair<wxString, wxString>> AttrStrings {
    std::make_pair("handler", Handler),
    GetElapsedTime(Started)
  };
  
  // Add the user-provided attributes.
  AttrStrings.reserve(AttrStrings.size() + Attrs.size());
  for (auto const &Attr : Attrs)
    AttrStrings.emplace_back(Attr->get_name(), Attr->to_string());
  
  auto const Node = new wxXmlNode(nullptr,
                                  wxXML_ELEMENT_NODE,
                                  "event",
                                  wxEmptyString,
                                  CreateAttributes(AttrStrings));
  
  if (Root->InsertChildAfter(Node, LastNode))
    LastNode = Node;
}
