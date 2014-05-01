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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Parsing.hpp"
#include "seec/wxWidgets/ImageResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/debug.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "ActionRecord.hpp"
#include "ActionRecordSettings.hpp"

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
// AttributeDeclReadWriteBase
//------------------------------------------------------------------------------

static std::string attributeDeclToString(clang::Decl const *Decl,
                                         seec::cm::ProcessTrace const &Trace)
{
  if (Decl == nullptr)
    return "nullptr";

  auto &Mapping = Trace.getMapping();
  auto const MappedAST = Mapping.getASTForDecl(Decl);
  if (!MappedAST)
    return "error: AST not found";

  auto const ASTIdx = Mapping.getASTIndex(MappedAST).get<0>();

  auto const MaybeDeclIdx = MappedAST->getIdxForDecl(Decl);
  if (!MaybeDeclIdx.assigned())
    return "error: Decl not found in AST";

  std::string Result;
  llvm::raw_string_ostream Stream(Result);

  Stream << ASTIdx
         << ' ' << MaybeDeclIdx.get<uint64_t>()
         << ' ' << Decl->getDeclKindName();

  auto const &SrcMgr = MappedAST->getASTUnit().getSourceManager();
  auto const LocStart = SrcMgr.getPresumedLoc(Decl->getLocStart());

  Stream << ' ' << LocStart.getFilename()
         << ' ' << LocStart.getLine()
         << ':' << LocStart.getColumn();

  Stream.flush();
  return Result;
}

std::string
AttributeDeclReadOnlyBase::to_string_impl(seec::cm::ProcessTrace const &Trace)
const
{
  return attributeDeclToString(Value, Trace);
}

std::string
AttributeDeclReadWriteBase::to_string_impl(seec::cm::ProcessTrace const &Trace)
const
{
  return attributeDeclToString(Value, Trace);
}

bool
AttributeDeclReadWriteBase::from_string_impl(seec::cm::ProcessTrace const &Tr,
                                             std::string const &String)
{
  if (String == "nullptr") {
    Value = nullptr;
    return true;
  }

  std::size_t CharsRead = 0;
  std::size_t ASTIndex;
  std::size_t DeclIndex;

  if (!seec::parseTo(String, CharsRead, ASTIndex, CharsRead))
    return false;

  if (!seec::parseTo(String, CharsRead, DeclIndex, CharsRead))
    return false;

  auto &Mapping = Tr.getMapping();
  auto const MappedAST = Mapping.getASTAtIndex(ASTIndex);
  if (!MappedAST)
    return false;

  auto const Decl = MappedAST->getDeclFromIdx(DeclIndex);
  if (!Decl)
    return false;

  Value = Decl;
  return true;
}


//------------------------------------------------------------------------------
// AttributeStmtReadOnlyBase, AttributeStmtReadWriteBase
//------------------------------------------------------------------------------

static std::string attributeStmtToString(clang::Stmt const *Stmt,
                                         seec::cm::ProcessTrace const &Trace)
{
  if (Stmt == nullptr)
    return "nullptr";

  auto &Mapping = Trace.getMapping();
  auto const MappedAST = Mapping.getASTForStmt(Stmt);
  if (!MappedAST)
    return "error: AST not found";

  auto const ASTIdx = Mapping.getASTIndex(MappedAST).get<0>();

  auto const MaybeStmtIdx = MappedAST->getIdxForStmt(Stmt);
  if (!MaybeStmtIdx.assigned())
    return "error: Stmt not found in AST";

  std::string Result;
  llvm::raw_string_ostream Stream(Result);

  Stream << ASTIdx
         << ' ' << MaybeStmtIdx.get<uint64_t>()
         << ' ' << Stmt->getStmtClassName();

  auto const &SrcMgr = MappedAST->getASTUnit().getSourceManager();
  auto const LocStart = SrcMgr.getPresumedLoc(Stmt->getLocStart());

  Stream << ' ' << LocStart.getFilename()
         << ' ' << LocStart.getLine()
         << ':' << LocStart.getColumn();

  Stream.flush();
  return Result;
}

std::string
AttributeStmtReadOnlyBase::to_string_impl(seec::cm::ProcessTrace const &Trace)
const
{
  return attributeStmtToString(Value, Trace);
}

std::string
AttributeStmtReadWriteBase::to_string_impl(seec::cm::ProcessTrace const &Trace)
const
{
  return attributeStmtToString(Value, Trace);
}

bool
AttributeStmtReadWriteBase::from_string_impl(seec::cm::ProcessTrace const &Tr,
                                             std::string const &String)
{
  if (String == "nullptr") {
    Value = nullptr;
    return true;
  }

  std::size_t CharsRead = 0;
  std::size_t ASTIndex;
  std::size_t StmtIndex;

  if (!seec::parseTo(String, CharsRead, ASTIndex, CharsRead))
    return false;

  if (!seec::parseTo(String, CharsRead, StmtIndex, CharsRead))
    return false;

  auto &Mapping = Tr.getMapping();
  auto const MappedAST = Mapping.getASTAtIndex(ASTIndex);
  if (!MappedAST)
    return false;

  auto const Stmt = MappedAST->getStmtFromIdx(StmtIndex);
  if (!Stmt)
    return false;

  Value = Stmt;
  return true;
}


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

bool ActionRecord::enable()
{
  if (!hasValidActionRecordToken())
    return false;
  
  Enabled = true;
  
  return true;
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
    AttrStrings.emplace_back(Attr->get_name(), Attr->to_string(Trace));
  
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


//------------------------------------------------------------------------------
// ActionRecordingControl
//------------------------------------------------------------------------------

bool ActionRecordingControl::Create(wxWindow *Parent, ActionRecord &WithRecord)
{
  if (!wxPanel::Create(Parent, wxID_ANY))
    return false;
  
  Recording = &WithRecord;
  
  // Get the GUI elements from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto Resources = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "RecordingToolbar");
  
  ImgRecordingOn  = seec::getwxImageEx(Resources, "ButtonOnImg",  Status);
  ImgRecordingOff = seec::getwxImageEx(Resources, "ButtonOffImg", Status);
  
  if (U_FAILURE(Status))
    return false;
  
  if (ImgRecordingOn.IsOk())
    ImgRecordingOn.Rescale(50, 50, wxIMAGE_QUALITY_HIGH);
  else
    return false;
  
  if (ImgRecordingOff.IsOk())
    ImgRecordingOff.Rescale(50, 50, wxIMAGE_QUALITY_HIGH);
  else
    return false;
  
  // Make the button.
  if (Recording->isEnabled()) {
    ButtonEnable = new wxBitmapButton(this, wxID_ANY, ImgRecordingOn);
  }
  else {
    ButtonEnable = new wxBitmapButton(this, wxID_ANY, ImgRecordingOff);
  }
  
  ButtonEnable->Bind(wxEVT_BUTTON, std::function<void (wxCommandEvent &)>{
    [this] (wxCommandEvent &) -> void {
      if (Recording->isEnabled()) {
        Recording->disable();
        ButtonEnable->SetBitmap(ImgRecordingOff);
      }
      else {
        if (Recording->enable()) {
          ButtonEnable->SetBitmap(ImgRecordingOn);
        }
        else {
          // TODO: Complain that we couldn't enable the recording.
        }
      }
    }});
  
  auto TopSizer = new wxBoxSizer(wxHORIZONTAL);
  TopSizer->Add(ButtonEnable, wxSizerFlags());
  SetSizerAndFit(TopSizer);
  
  return true;
}
