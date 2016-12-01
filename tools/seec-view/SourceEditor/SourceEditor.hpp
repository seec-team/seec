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

#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/string.h>

class wxStyledTextCtrl;

class SourceEditorFrame : public wxFrame
{
  /// Registration to ColourSchemeSettings changes.
  seec::observer::registration m_ColourSchemeSettingsRegistration;
  
  wxFileName m_FileName;
  
  wxStyledTextCtrl *m_Scintilla;
  
public:
  SourceEditorFrame();
  
  virtual ~SourceEditorFrame();
  
  void Open(wxFileName const &FileName);
  
  void OnSave(wxCommandEvent &Event);
  
  void OnSaveAs(wxCommandEvent &Event);
  
  void OnClose(wxCloseEvent &event);
};

#endif // define SEEC_TRACE_VIEW_SOURCEEDITOR_SOURCEEDITOR_HPP
