//===- tools/seec-view/SourceEditor/SourceEditor.hpp ----------------------===//
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

#ifndef SEEC_TRACE_VIEW_SOURCEEDITOR_SOURCEEDITOR_HPP
#define SEEC_TRACE_VIEW_SOURCEEDITOR_SOURCEEDITOR_HPP

#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/Observer.hpp"
#include "seec/wxWidgets/AuiManagerHandle.hpp"

#include "type_safe/boolean.hpp"

#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/fswatcher.h>
#include <wx/string.h>

class ExternalCompileEvent;
class wxStyledTextCtrl;
class wxStyledTextEvent;
class wxAuiManager;
class wxProcess;
class wxProcessEvent;
class wxStatusBar;

class SourceEditorFile
{
public:
  enum class EBufferKind {
    ScratchPad,
    File
  };

private:  
  EBufferKind m_BufferKind;

  wxFileName m_FileName;

public:  
  /// \brief Construct a \c ScratchPad .
  /// A file that is not yet saved to a permanent location on disk, but has a
  /// temporary location on disk that can be used for compiling, executing etc.
  ///
  SourceEditorFile();
  
  /// \brief Construct a \c File - a permanent location on disk.
  ///
  SourceEditorFile(wxFileName FileName)
  : m_BufferKind(EBufferKind::File),
    m_FileName(std::move(FileName))
  {}
  
  /// \brief If this is a \c ScratchPad , clean up temporary files.
  ///
  ~SourceEditorFile();
  
  SourceEditorFile(SourceEditorFile &&RHS);
  SourceEditorFile &operator=(SourceEditorFile &&RHS);
  
  SourceEditorFile(SourceEditorFile const &RHS) = delete;
  SourceEditorFile &operator=(SourceEditorFile const &RHS) = delete;
  
  EBufferKind getBufferKind() const { return m_BufferKind; }
  
  wxFileName const &getFileName() const { return m_FileName; }
  
  wxFileName getPermanentFileName() const {
    switch (m_BufferKind) {
      case EBufferKind::File: return m_FileName;
      case EBufferKind::ScratchPad: SEEC_FALLTHROUGH;
      default: return wxFileName();
    }
  }
};

class SourceEditorFrame : public wxFrame
{
  enum class ETask {
    Nothing,
    Compile,
    Run
  };
  
  enum class EStatusField : int {
    Dummy = 0,
    Action,
    NumberOfFields
  };
  
  /// Registration to ColourSchemeSettings changes.
  seec::observer::registration m_ColourSchemeSettingsRegistration;
  
  std::unique_ptr<wxFileSystemWatcher> m_FSWatcher;
  
  seec::wxAuiManagerHandle m_Manager;
  
  SourceEditorFile m_File;
  
  wxStyledTextCtrl *m_Scintilla;
  
  wxTextCtrl *m_CompileOutputCtrl;
  
  wxProcess *m_CompileProcess;
  
  ETask m_CurrentTask;
  
  wxStatusBar *m_StatusBar;
  
  std::pair<std::unique_ptr<wxMenu>, wxString> createProjectMenu();
  
  void SetFileName(wxFileName NewName);
  
  void SetTitleFromFileName();
  
  void SetStatusMessage(EStatusField const Field, wxString const &Message);
  
  type_safe::boolean DoCompile();
  
  type_safe::boolean DoRun();
  
  type_safe::boolean DoSave();
  
  type_safe::boolean DoSaveAs();
  
  type_safe::boolean DoEnsureBufferIsWritten();
  
  void OnFSEvent(wxFileSystemWatcherEvent &Event);
  
  void OnModified(wxStyledTextEvent &Event);
  
  void OnEndProcess(wxProcessEvent &Event);
  
  void ShowStatusActionMessage(char const * const MessageKey);
  
  void OnCompileStarted(ExternalCompileEvent &Event);
  
  void OnCompileOutput(ExternalCompileEvent &Event);
  
  void OnCompileComplete(ExternalCompileEvent &Event);
  
  void OnCompileFailed(ExternalCompileEvent &Event);
  
  void OnEscapePressed(wxKeyEvent &Event);
  
public:
  SourceEditorFrame();
  
  virtual ~SourceEditorFrame();
  
  void Open(wxFileName const &FileName);
  
  void OnSave(wxCommandEvent &Event);
  
  void OnSaveAs(wxCommandEvent &Event);
  
  void OnClose(wxCloseEvent &event);
};

#endif // define SEEC_TRACE_VIEW_SOURCEEDITOR_SOURCEEDITOR_HPP
