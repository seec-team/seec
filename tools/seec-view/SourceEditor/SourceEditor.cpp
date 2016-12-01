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
#include "seec/wxWidgets/StringConversion.hpp"

#include "../ColourSchemeSettings.hpp"
#include "../CommonMenus.hpp"
#include "../LocaleSettings.hpp"
#include "../SourceViewerSettings.hpp"
#include "../TraceViewerApp.hpp"
#include "SourceEditor.hpp"

#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>

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

} // anonymous namespace

SourceEditorFrame::SourceEditorFrame()
: m_ColourSchemeSettingsRegistration(),
  m_FileName(),
  m_Scintilla(nullptr)
{
  if (!wxFrame::Create(nullptr, wxID_ANY, wxString()))
    return;
  
  m_Scintilla = new wxStyledTextCtrl(this);
  setSTCPreferences(*m_Scintilla);
  
  auto const TopSizer = new wxBoxSizer(wxVERTICAL);
  TopSizer->Add(m_Scintilla, wxSizerFlags(1).Expand());
  SetSizerAndFit(TopSizer);
  
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
  
  SetMenuBar(menuBar);
  
  // Setup the event handling.
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       [this] (wxCommandEvent &) { this->Close(); },
       wxID_CLOSE);
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       &SourceEditorFrame::OnSave, this, wxID_SAVE);
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       &SourceEditorFrame::OnSaveAs, this, wxID_SAVEAS);
  Bind(wxEVT_CLOSE_WINDOW, &SourceEditorFrame::OnClose, this);
  
#define SEEC_FORWARD_COMMAND_TO_SCINTILLA(CMDID, METHOD)                       \
  Bind(wxEVT_COMMAND_MENU_SELECTED,                                            \
       [=] (wxCommandEvent &) { m_Scintilla->METHOD(); }, CMDID)

  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_UNDO, Undo);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_REDO, Redo);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_CUT, Cut);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_COPY, Copy);
  SEEC_FORWARD_COMMAND_TO_SCINTILLA(wxID_PASTE, Paste);

#undef SEEC_FORWARD_COMMAND_TO_SCINTILLA

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
    m_FileName = FileName;
  }
}

void SourceEditorFrame::OnSave(wxCommandEvent &Event)
{
  if (!m_FileName.HasName()) {
    OnSaveAs(Event);
    return;
  }

  m_Scintilla->SaveFile(m_FileName.GetFullPath());
}

void SourceEditorFrame::OnSaveAs(wxCommandEvent &Event)
{
  auto const Res = seec::Resource("TraceViewer")["GUIText"]["SaveSource"];

  wxFileDialog SaveDlg(this,
                       seec::towxString(Res["Title"]),
                       /* default dir  */ wxEmptyString,
                       /* default file */ wxEmptyString,
                       seec::towxString(Res["FileType"]),
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (SaveDlg.ShowModal() == wxID_CANCEL)
    return;

  m_FileName.Assign(SaveDlg.GetDirectory(),
                    SaveDlg.GetFilename());
  OnSave(Event);
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
        wxCommandEvent Ev;
        OnSave(Ev);
        break;
      }
      else if (Choice == wxNO)
      {
        break;
      }
    }
  }
  
  Ev.Skip();
}
