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

class SourceEditorFrame : public wxFrame
{
  /// Registration to ColourSchemeSettings changes.
  seec::observer::registration m_ColourSchemeSettingsRegistration;
  
  std::unique_ptr<wxFileSystemWatcher> m_FSWatcher;
  
  seec::wxAuiManagerHandle m_Manager;
  
  wxFileName m_FileName;
  
  wxStyledTextCtrl *m_Scintilla;
  
  wxTextCtrl *m_CompileOutputCtrl;
  
  wxProcess *m_CompileProcess;
  
  std::pair<std::unique_ptr<wxMenu>, wxString> createProjectMenu();
  
  void SetFileName(wxFileName NewName);
  
  void SetTitleFromFileName();
  
  type_safe::boolean DoCompile();
  
  type_safe::boolean DoRun();
  
  type_safe::boolean DoSave();
  
  type_safe::boolean DoSaveAs();
  
  void OnFSEvent(wxFileSystemWatcherEvent &Event);
  
  void OnModified(wxStyledTextEvent &Event);
  
  void OnEndProcess(wxProcessEvent &Event);
  
  void OnCompileStarted(ExternalCompileEvent &Event);
  
  void OnCompileOutput(ExternalCompileEvent &Event);
  
  void OnCompileComplete(ExternalCompileEvent &Event);
  
  void OnCompileFailed(ExternalCompileEvent &Event);
  
public:
  SourceEditorFrame();
  
  virtual ~SourceEditorFrame();
  
  void Open(wxFileName const &FileName);
  
  void OnSave(wxCommandEvent &Event);
  
  void OnSaveAs(wxCommandEvent &Event);
  
  void OnClose(wxCloseEvent &event);
};

#endif // define SEEC_TRACE_VIEW_SOURCEEDITOR_SOURCEEDITOR_HPP
