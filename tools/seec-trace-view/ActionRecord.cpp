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

#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Trace/TraceReader.hpp"

#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/debug.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

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

bool ActionRecord::archiveTo(wxOutputStream &Stream)
{
  wxZipOutputStream Output{Stream};
  if (!Output.IsOk())
    return false;
  
  // Save the recording of this session to the archive.
  Output.PutNextEntry("record.xml"); // CHECK ME
  RecordDocument->Save(Output); // CHECK ME
  
  // Save the contents of the trace to the archive.
  Output.PutNextDirEntry("trace");
  
  auto const &UnmappedTrace = *(Trace.getUnmappedTrace());
  auto MaybeFiles = UnmappedTrace.getAllFileData();
  if (MaybeFiles.assigned<seec::Error>())
    return false;
  
  auto const &Files = MaybeFiles.get<std::vector<seec::trace::TraceFile>>();
  
  for (auto const &File : Files) {
    Output.PutNextEntry(wxString{"trace/"} + File.getName());
    
    auto const &Buffer = *(File.getContents());
    Output.Write(Buffer.getBufferStart(), Buffer.getBufferSize());
  }
  
  if (!Output.Close())
    return false;
  
  return true;
}

ActionRecord::ActionRecord(seec::cm::ProcessTrace const &ForTrace)
: Trace(ForTrace),
  Enabled(false),
  Started(std::chrono::steady_clock::now()),
  RecordDocument(new wxXmlDocument()),
  LastNode(nullptr)
{
  auto const Attrs = CreateAttributes({
    std::make_pair("version", std::to_string(formatVersion())),
    std::make_pair("began", wxDateTime::Now().FormatISOCombined())
  });
  
  auto const Root = new wxXmlNode(nullptr,
                                  wxXML_ELEMENT_NODE,
                                  "recording",
                                  wxEmptyString,
                                  Attrs);
  
  RecordDocument->SetRoot(Root);
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

bool ActionRecord::finalize()
{
  if (!Enabled)
    return true;
  
  auto const DateStr = wxDateTime::Now().Format("%F.%H-%M-%S");
  
  wxFileName ArchivePath;
  ArchivePath.AssignDir(wxStandardPaths::Get().GetUserLocalDataDir());
  
  wxFFile ArchiveFile;
  
  // Attempt to generate a unique filename for the archive and open it.
  for (unsigned i = 0; ; ++i) {
    ArchivePath.SetFullName(DateStr + "." + std::to_string(i) + ".seecrecord");
    if (ArchivePath.FileExists())
      continue;
    
    if (!ArchiveFile.Open(ArchivePath.GetFullPath(), "wb"))
      return false;
    
    break;
  }
  
  wxFFileOutputStream ArchiveStream(ArchiveFile);
  
  if (archiveTo(ArchiveStream)) {
    // TODO: Upload the archive to the server.
    return true;
  }
  else {
    wxRemoveFile(ArchivePath.GetFullPath());
    return false;
  }
}
