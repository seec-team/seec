//===- tools/seec-view/SourceEditor/GlobalCompilerPreferences.cpp ---------===//
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

#include <wx/config.h>
#include <wx/filepicker.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/platinfo.h>
#include <wx/sizer.h>
#include <wx/stattext.h>


#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"

#include "GlobalCompilerPreferences.hpp"

using namespace seec;

char const * const cConfigKeyForMinGWGCCPath = "/Compiler/MinGW/GCCPath";

wxFileName getPathForMinGWGCC()
{
  auto const Config = wxConfig::Get();
  wxString GCCPath;
  if (Config->Read(cConfigKeyForMinGWGCCPath, &GCCPath)) {
    return wxFileName{GCCPath};
  }

#if defined(_WIN32)
  auto const GCCName = "gcc.exe";
#else
  auto const GCCName = "gcc";
#endif

  auto SearchEnvPath = llvm::sys::findProgramByName(GCCName);
  if (SearchEnvPath)
    return wxFileName{std::move(*SearchEnvPath)};

  return wxFileName{};
}

namespace {

bool setPathForMinGWGCC(wxString const &Path)
{
  auto const Config = wxConfig::Get();
  
  if (Path.empty()) {
    Config->DeleteEntry(cConfigKeyForMinGWGCCPath);
  }
  else {
    if (!Config->Write(cConfigKeyForMinGWGCCPath, Path))
      return false;
  }
  
  Config->Flush();
  return true;
}

}

bool GlobalCompilerPreferencesWindow::SaveValuesImpl()
{
  if (m_MinGWGCCPathCtrl) {
    auto const Path = m_MinGWGCCPathCtrl->GetPath();
    
    if (!Path.empty() && !llvm::sys::fs::can_execute(Path.ToStdString())) {
      auto const ResText = Resource("TraceViewer")["GlobalCompilerPreferences"];

      wxMessageDialog Dlg(this,
                          towxString(ResText["GCCNotExecutableMessage"]),
                          towxString(ResText["GCCNotExecutableCaption"]));
      Dlg.ShowModal();
      return false;
    }
    
    if (!setPathForMinGWGCC(m_MinGWGCCPathCtrl->GetPath())) {
      return false;
    }
  }
  
  return true;
}

void GlobalCompilerPreferencesWindow::CancelChangesImpl() {}

wxString GlobalCompilerPreferencesWindow::GetDisplayNameImpl()
{
  return towxString(Resource("TraceViewer")
                    ["GlobalCompilerPreferences"]["Title"]);
}

GlobalCompilerPreferencesWindow::GlobalCompilerPreferencesWindow()
: m_MinGWGCCPathCtrl(nullptr)
{}

GlobalCompilerPreferencesWindow
::GlobalCompilerPreferencesWindow(wxWindow *Parent)
: GlobalCompilerPreferencesWindow()
{
  Create(Parent);
}

GlobalCompilerPreferencesWindow::~GlobalCompilerPreferencesWindow()
  = default;

bool GlobalCompilerPreferencesWindow::Create(wxWindow *Parent)
{
  if (!wxWindow::Create(Parent, wxID_ANY)) {
    return false;
  }

  auto const Res = Resource("TraceViewer")["GlobalCompilerPreferences"];
  auto const &Platform = wxPlatformInfo::Get();

  // Vertical sizer to hold each row of input.
  auto const ParentSizer = new wxBoxSizer(wxVERTICAL);

  int const BorderDir = wxLEFT | wxRIGHT;
  int const BorderSize = 5;
  
  if (Platform.GetOperatingSystemId() & wxOS_WINDOWS) {
    auto const MinGWGCCFilePickerLabel =
      new wxStaticText(this, wxID_ANY,
                       towxString(Res["MinGWGCCLocationLabel"]));

    m_MinGWGCCPathCtrl =
      new wxFilePickerCtrl(this, wxID_ANY,
        /* path */ getPathForMinGWGCC().GetFullPath(),
        /* message */ towxString(Res["MinGWGCCLocationPrompt"]),
        wxFileSelectorDefaultWildcardStr,
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_DEFAULT_STYLE | wxFLP_USE_TEXTCTRL | wxFLP_FILE_MUST_EXIST);

    ParentSizer->AddSpacer(BorderSize);

    ParentSizer->Add(MinGWGCCFilePickerLabel,
                     wxSizerFlags().Border(BorderDir, BorderSize));

    ParentSizer->Add(m_MinGWGCCPathCtrl,
                     wxSizerFlags().Expand().Border(BorderDir, BorderSize));

    ParentSizer->AddSpacer(BorderSize);
  }

  SetSizerAndFit(ParentSizer);

  return true;
}
