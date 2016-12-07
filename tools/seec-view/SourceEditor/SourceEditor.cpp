//===- tools/seec-view/SourceEditor/SourceEditor.cpp ----------------------===//
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

#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeFunction.hpp"
#include "seec/wxWidgets/Config.hpp"
#include "seec/wxWidgets/QueueEvent.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/ADT/STLExtras.h"

#include "../ColourSchemeSettings.hpp"
#include "../CommonMenus.hpp"
#include "../LocaleSettings.hpp"
#include "../SourceViewerSettings.hpp"
#include "../TraceViewerApp.hpp"
#include "SourceEditor.hpp"

#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/fswatcher.h>
#include <wx/sizer.h>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/process.h>
#include <wx/wfstream.h>
#include <wx/sstream.h>
#include <wx/textctrl.h>
#include <wx/stc/stc.h>
#include <wx/aui/aui.h>
#include <wx/aui/framemanager.h>

namespace {

void setSTCPreferences(wxStyledTextCtrl &Text)
{
  // Setup styles according to the user's colour scheme.
  auto &Scheme = *wxGetApp().getColourSchemeSettings().getColourScheme();
  setupStylesFromColourScheme(Text, Scheme);
  
  // Set the lexer to C++.
  Text.SetLexer(wxSTC_LEX_CPP);
  
  // Setup the keywords used by Scintilla's C++ lexer.
  // TODO: this should be shared with SourceViewer
  UErrorCode Status = U_ZERO_ERROR;
  auto KeywordRes = seec::getResource("TraceViewer",
                                      getLocale(),
                                      Status,
                                      "ScintillaKeywords",
                                      "C");
  if (U_SUCCESS(Status)) {
    auto Size = KeywordRes.getSize();
    
    for (int32_t i = 0; i < Size; ++i) {
      auto UniStr = KeywordRes.getStringEx(i, Status);
      if (U_FAILURE(Status))
        break;
      
      Text.SetKeyWords(i, seec::towxString(UniStr));
    }
  }

  // Misc. settings.
  Text.SetExtraDescent(2);
}

wxFileName getBinaryNameForSource(wxFileName const &Source)
{
  wxFileName BinaryName = Source;
  BinaryName.SetExt("");
  return BinaryName;
}

class wxExecuteArgBuilder
{
  std::vector<char> m_Arguments;
  std::vector<size_t> m_ArgIndices;
  std::vector<char *> m_ArgPointers;
  
public:
  wxExecuteArgBuilder()
  : m_Arguments(),
    m_ArgIndices(),
    m_ArgPointers()
  {}
  
  wxExecuteArgBuilder &add(llvm::StringRef Argument)
  {
    // Store the index of the argument's first character.
    m_ArgIndices.push_back(m_Arguments.size());
    
    // Add the argument's raw characters, and a null-terminator.
    m_Arguments.insert(m_Arguments.end(),
                       Argument.begin(),
                       Argument.end());
    m_Arguments.push_back('\0');
    
    return *this;
  }
  
  template<typename T>
  wxExecuteArgBuilder &add(wxScopedCharTypeBuffer<T> const &Buffer)
  {
    return add(llvm::StringRef(Buffer.data(), Buffer.length()));
  }
  
  wxExecuteArgBuilder &add(char const * const CStr)
  {
    return add(llvm::StringRef(CStr));
  }
  
  char **getArguments()
  {
    m_ArgPointers.clear();
    m_ArgPointers.reserve(m_ArgIndices.size() + 1);
    
    for (auto const Index : m_ArgIndices) {
      m_ArgPointers.push_back(m_Arguments.data() + Index);
    }
    
    m_ArgPointers.push_back(nullptr);
    
    return m_ArgPointers.data();
  }
};

} // anonymous namespace


/// \brief 
///
class ExternalCompileEvent : public wxEvent
{
  std::string m_Message;

public:
  // Make this class known to wxWidgets' class hierarchy.
  wxDECLARE_CLASS(ExternalCompileEvent);

  /// \brief Constructor.
  ///
  ExternalCompileEvent(wxEventType EventType, int WinID)
  : wxEvent(WinID, EventType)
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }
  
  /// \brief Constructor with message.
  ///
  ExternalCompileEvent(wxEventType EventType, int WinID, std::string Message)
  : wxEvent(WinID, EventType),
    m_Message(std::move(Message))
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// \brief Constructor with message.
  ///
  ExternalCompileEvent(wxEventType EventType, int WinID,
                       seec::Resource const &Message)
  : wxEvent(WinID, EventType),
    m_Message(seec::toUTF8String(Message))
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new ExternalCompileEvent(*this);
  }
  
  std::string const &getMessage() const { return m_Message; }
};

wxIMPLEMENT_CLASS(ExternalCompileEvent, wxEvent)

wxDEFINE_EVENT(SEEC_EV_COMPILE_STARTED, ExternalCompileEvent);
wxDEFINE_EVENT(SEEC_EV_COMPILE_OUTPUT, ExternalCompileEvent);
wxDEFINE_EVENT(SEEC_EV_COMPILE_COMPLETE, ExternalCompileEvent);
wxDEFINE_EVENT(SEEC_EV_COMPILE_FAILED, ExternalCompileEvent);


void SourceEditorFrame::SetFileName(wxFileName NewName)
{
  NewName.MakeAbsolute();
  m_FileName = std::move(NewName);
  m_FSWatcher->RemoveAll();
  m_FSWatcher->Add(m_FileName.GetPathWithSep());
  SetTitleFromFileName();
}

void SourceEditorFrame::SetTitleFromFileName()
{
  wxString Title = m_FileName.GetFullName();

  if (Title.empty()) {
    Title = seec::towxString(seec::Resource("TraceViewer")["SourceEditor"]
                                                          ["UnsavedFileName"]);
  }

  if (m_Scintilla->IsModified()) {
    Title.Append("*");
  }

  SetTitle(Title);
}

std::pair<std::unique_ptr<wxMenu>, wxString>
SourceEditorFrame::createProjectMenu()
{
  auto const Res = seec::Resource("TraceViewer")["SourceEditor"]["ProjectMenu"];
  
  auto Menu = llvm::make_unique<wxMenu>();
  
  auto const MICompile = Menu->Append(wxID_ANY,
                                      seec::towxString(Res["Compile"]));
  BindMenuItem(MICompile, [this] (wxEvent &) -> void { this->DoCompile(); });
  
  auto const MIRun = Menu->Append(wxID_ANY, seec::towxString(Res["Run"]));
  BindMenuItem(MIRun, [this] (wxEvent &) -> void { this->DoRun(); });
  
  return std::make_pair(std::move(Menu), seec::towxString(Res["Title"]));
}

type_safe::boolean SourceEditorFrame::DoCompile()
{
  auto const Res = seec::Resource("TraceViewer")["SourceEditor"];
  queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_STARTED);
  
  auto const PathToCC = seec::getPathToSeeCCC();
  if (!PathToCC) {
    queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_OUTPUT,
                                     Res["ErrorCCNotFound"]);
    return false;
  }
  
  if (m_CompileProcess) {
    auto const Message = seec::towxString(Res["ErrorAlreadyCompiling"]);
    wxMessageBox(Message);
    return false;
  }
  
  // If the file doesn't have a name, or if it has been modified,
  // ask the user to save it now.
  if (!m_FileName.HasName() || m_Scintilla->IsModified()) {
    auto const Message = seec::towxString(Res["SaveBeforeCompile"]);
    auto const Choice = wxMessageBox(Message, wxEmptyString, wxYES_NO);
    
    if (Choice != wxYES || !DoSave()) {
      return false;
    }
  }
  
  auto const Output = getBinaryNameForSource(m_FileName);
  
  m_CompileProcess = new wxProcess(this);
  m_CompileProcess->Redirect();

  wxExecuteArgBuilder Args;
  Args.add(*PathToCC)
      .add("-std=c99")
      .add("-Wall")
      .add("-Werror")
      .add("-pedantic")
      .add("-o")
      .add(Output.GetFullName().ToUTF8())
      .add(m_FileName.GetFullName().ToUTF8());

  wxExecuteEnv Env;
  Env.cwd = m_FileName.GetPath();
  
  auto const PID = wxExecute(Args.getArguments(),
                             wxEXEC_ASYNC, m_CompileProcess, &Env);

  if (PID == 0) {
    queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_OUTPUT,
                                     Res["ErrorExecuteFailed"]);
    queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_FAILED);
    return false;
  }
  else {
    return true;
  }
}

type_safe::boolean SourceEditorFrame::DoRun()
{
  auto const Res = seec::Resource("TraceViewer")["SourceEditor"];
  
  // If the file doesn't have a name, ask the user to save it now.
  if (!m_FileName.HasName()) {
    auto const Message = seec::towxString(Res["SaveBeforeCompile"]);
    auto const Choice = wxMessageBox(Message, wxEmptyString, wxYES_NO);
    
    if (Choice != wxYES || !DoSave()) {
      return false;
    }
  }
  
  // If the file has been modified, ask the user to save it now.
  if (m_Scintilla->IsModified()) {
    auto const Message = seec::towxString(Res["SaveBeforeCompile"]);
    auto const Choice = wxMessageBox(Message, wxEmptyString, wxYES_NO);
    
    if (Choice != wxYES || !DoSave()) {
      return false;
    }
  }
  
  auto const Output = getBinaryNameForSource(m_FileName);
  
  if (!Output.FileExists()) {
    auto const Message = seec::towxString(Res["CompileBeforeRun"]);
    wxMessageBox(Message);
    return false;
  }
  
  if (Output.GetModificationTime() < m_FileName.GetModificationTime()) {
    auto const Message = seec::towxString(Res["CompileBeforeRun"]);
    wxMessageBox(Message);
    return false;
  }
  
  auto TheProcess = llvm::make_unique<wxProcess>(this);

  // TODO: allow the user to configure which terminal is used.
  // xterm -e
  // gnome-terminal -x
  wxExecuteArgBuilder Args;
  Args.add("gnome-terminal")
      .add("-x")
      .add("bash")
      .add("-c");

  std::string BashCmd = "\"";
  BashCmd += Output.GetFullName().Prepend("./").ToUTF8();
  BashCmd += "\" ";
  // TODO: allow user-configurable arguments to the process.
  // TODO: localize the below message.
  BashCmd += "; echo \"\"";
  BashCmd += "; read -rsp \"";
  BashCmd += seec::toUTF8String(Res["PressAnyKeyToClose"]);
  BashCmd += "\" -n 1";
  Args.add(BashCmd);

  wxExecuteEnv Env;
  Env.cwd = m_FileName.GetPath();
  if (!wxGetEnvMap(&Env.env)) {
    return false;
  }
  
  // .seec is automatically appended, so just use the executable's name.
  Env.env["SEEC_TRACE_NAME"] = Output.GetFullName();
  
  auto const PID = wxExecute(Args.getArguments(), wxEXEC_ASYNC,
                             TheProcess.release(), &Env);
  
  if (PID == 0) {
    // TODO: notify the user.
    wxLogDebug("failed to execute terminal process");
    return false;
  }
  else {
    return true;
  }
}

type_safe::boolean SourceEditorFrame::DoSave()
{
  if (m_FileName.HasName()) {
    auto const Result = m_Scintilla->SaveFile(m_FileName.GetFullPath());
    SetTitleFromFileName();
    return Result;
  }
  else {
    return DoSaveAs();
  }
}

type_safe::boolean SourceEditorFrame::DoSaveAs()
{
  auto const Res = seec::Resource("TraceViewer")["GUIText"]["SaveSource"];

  wxFileDialog SaveDlg(this,
                       seec::towxString(Res["Title"]),
                       /* default dir  */ wxEmptyString,
                       /* default file */ wxEmptyString,
                       seec::towxString(Res["FileType"]),
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (SaveDlg.ShowModal() == wxID_CANCEL)
    return false;

  SetFileName(wxFileName(SaveDlg.GetDirectory(), SaveDlg.GetFilename()));

  return DoSave();
}

void SourceEditorFrame::OnFSEvent(wxFileSystemWatcherEvent &Event)
{
  if (Event.GetChangeType() == wxFSW_EVENT_RENAME) {
    if (Event.GetNewPath().GetExt() == "seec") {
      // TODO: instead of always opening this, we should notify the user and
      // give them the option to open it.
      wxGetApp().MacOpenFile(Event.GetNewPath().GetFullPath());
    }
  }
}

void SourceEditorFrame::OnModified(wxStyledTextEvent &Event)
{
  SetTitleFromFileName();
}

void SourceEditorFrame::OnEndProcess(wxProcessEvent &Event)
{
  if (m_CompileProcess && m_CompileProcess->GetPid() == Event.GetPid()) {
    // Show the compiler's error output (if any).
    if (m_CompileProcess->IsRedirected()) {
      wxStringOutputStream Output;
      m_CompileProcess->GetErrorStream()->Read(Output);
      if (!Output.GetString().empty()) {
        queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_OUTPUT,
                                         Output.GetString().ToStdString());
      }
    }

    if (Event.GetExitCode() == 0) {
      queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_COMPLETE);
    }
    else {
      queueEvent<ExternalCompileEvent>(*this, SEEC_EV_COMPILE_FAILED);
    }
    
    // Since we're skipping the event (below), the wxProcess will delete itself.
    m_CompileProcess = nullptr;
  }
  
  Event.Skip();
}

void SourceEditorFrame::OnCompileStarted(ExternalCompileEvent &Event)
{
  m_CompileOutputCtrl->Clear();
}

void SourceEditorFrame::OnCompileOutput(ExternalCompileEvent &Event)
{
  m_CompileOutputCtrl->AppendText(Event.getMessage());
  m_Manager->GetPane(m_CompileOutputCtrl).Show();
  m_Manager->Update();
}

void SourceEditorFrame::OnCompileComplete(ExternalCompileEvent &Event)
{
  // TODO: notify the user.
}

void SourceEditorFrame::OnCompileFailed(ExternalCompileEvent &Event)
{
  // TODO: notify the user.
}

SourceEditorFrame::SourceEditorFrame()
: m_ColourSchemeSettingsRegistration(),
  m_FSWatcher(llvm::make_unique<wxFileSystemWatcher>()),
  m_Manager(),
  m_FileName(),
  m_Scintilla(nullptr),
  m_CompileOutputCtrl(),
  m_CompileProcess(nullptr)
{
  if (!wxFrame::Create(nullptr, wxID_ANY, wxString()))
    return;
  
  auto const Res = seec::Resource("TraceViewer")["SourceEditor"];
  
  m_FSWatcher->Bind(wxEVT_FSWATCHER, &SourceEditorFrame::OnFSEvent, this);
  
  m_Manager = seec::wxAuiManagerHandle(new wxAuiManager(this));
  
  m_Scintilla = new wxStyledTextCtrl(this);
  setSTCPreferences(*m_Scintilla);
  
  m_Manager->AddPane(m_Scintilla,
                     wxAuiPaneInfo().Name("Scintilla").CentrePane());
  
  m_CompileOutputCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_MULTILINE | wxTE_READONLY |
                                       wxTE_RICH2 | wxTE_AUTO_URL | wxHSCROLL);
  
  wxTextAttr CompileOutputAttr;
  CompileOutputAttr.SetFontFamily(wxFONTFAMILY_MODERN);
  m_CompileOutputCtrl->SetDefaultStyle(CompileOutputAttr);
  
  m_Manager->AddPane(m_CompileOutputCtrl,
    wxAuiPaneInfo().Name("CompileOutput")
                   .Caption(seec::towxString(Res["CompileOutputCaption"]))
                   .Bottom()
                   .MinimizeButton(true)
                   .Hide());
  
  // Listen for colour scheme changes.
  m_ColourSchemeSettingsRegistration =
    wxGetApp().getColourSchemeSettings().addListener(
      [=] (ColourSchemeSettings const &Settings) {
        setupStylesFromColourScheme(*m_Scintilla, *Settings.getColourScheme());
      }
    );
  
  // Setup the menus.
  auto menuBar = new wxMenuBar();
  
  append(menuBar, createFileMenu({wxID_SAVE, wxID_SAVEAS}));
  
  {
    auto EditMenu = createEditMenu();
    if (EditMenu.first) {
      EditMenu.first->Prepend(wxID_UNDO);
      EditMenu.first->Prepend(wxID_REDO);
      EditMenu.first->Prepend(wxID_CUT);
      EditMenu.first->Prepend(wxID_COPY);
      EditMenu.first->Prepend(wxID_PASTE);
    }
    append(menuBar, std::move(EditMenu));
  }
  
  append(menuBar, createProjectMenu());
  
  SetMenuBar(menuBar);
  
  // Setup the event handling.
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       seec::make_function([this] (wxCommandEvent &Ev) { this->Close(); }),
       wxID_CLOSE);
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       &SourceEditorFrame::OnSave, this, wxID_SAVE);
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       &SourceEditorFrame::OnSaveAs, this, wxID_SAVEAS);
  Bind(wxEVT_CLOSE_WINDOW, &SourceEditorFrame::OnClose, this);
  
#define SEEC_FORWARD_COMMAND_TO_SCINTILLA(CMDID, METHOD)                       \
  Bind(wxEVT_COMMAND_MENU_SELECTED,                                            \
       seec::make_function([=] (wxCommandEvent &) { m_Scintilla->METHOD(); }), \
       CMDID)

  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_UNDO, Undo);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_REDO, Redo);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_CUT, Cut);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_COPY, Copy);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_PASTE, Paste);

#undef SEEC_FORWARD_COMMAND_TO_SCINTILLA

  Bind(wxEVT_STC_MODIFIED, &SourceEditorFrame::OnModified, this);

  Bind(wxEVT_END_PROCESS, &SourceEditorFrame::OnEndProcess, this);
  Bind(SEEC_EV_COMPILE_STARTED, &SourceEditorFrame::OnCompileStarted, this);
  Bind(SEEC_EV_COMPILE_OUTPUT, &SourceEditorFrame::OnCompileOutput, this);
  Bind(SEEC_EV_COMPILE_COMPLETE, &SourceEditorFrame::OnCompileComplete, this);
  Bind(SEEC_EV_COMPILE_FAILED, &SourceEditorFrame::OnCompileFailed, this);

  m_Manager->Update();
  
  // Notify the TraceViewerApp that we have been created.
  auto &App = wxGetApp();
  App.addTopLevelWindow(this);
}

SourceEditorFrame::~SourceEditorFrame()
{
  // Notify the TraceViewerApp that we have been destroyed.
  auto &App = wxGetApp();
  App.removeTopLevelWindow(this);
}

void SourceEditorFrame::Open(wxFileName const &FileName)
{
  if (m_Scintilla->LoadFile(FileName.GetFullPath())) {
    SetFileName(FileName);
  }
}

void SourceEditorFrame::OnSave(wxCommandEvent &Event)
{
  DoSave();
}

void SourceEditorFrame::OnSaveAs(wxCommandEvent &Event)
{
  DoSaveAs();
}

void SourceEditorFrame::OnClose(wxCloseEvent &Ev)
{
  if (m_Scintilla->IsModified()) {
    auto const Choices = Ev.CanVeto() ? (wxYES_NO | wxCANCEL)
                                      : (wxYES_NO);
    
    auto const Res = seec::Resource("TraceViewer")["SourceEditor"];
    auto const Message = seec::towxString(Res["SaveClosingModifiedFile"]);
    
    while (true)
    {
      auto const Choice = wxMessageBox(Message, wxEmptyString, Choices);
      
      if (Choice == wxCANCEL && Ev.CanVeto())
      {
        Ev.Veto();
        return;
      }
      else if (Choice == wxYES)
      {
        if (DoSave()) {
          break;
        }
        // If the save failed, loop back and ask the user what to do (again).
      }
      else if (Choice == wxNO)
      {
        break;
      }
    }
  }
  
  Ev.Skip();
}
