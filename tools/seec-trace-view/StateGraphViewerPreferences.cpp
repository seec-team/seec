//===- tools/seec-trace-view/StateGraphViewerPreferences.cpp --------------===//
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
#include <wx/sizer.h>
#include <wx/stattext.h>


#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"

#include "StateGraphViewerPreferences.hpp"

using namespace seec;

char const * const cConfigKeyForDotPath = "/StateGraphViewer/DotPath";

std::string getPathForDotExecutable()
{
  auto const Config = wxConfig::Get();
  wxString DotPath;
  if (Config->Read(cConfigKeyForDotPath, &DotPath)) {
    return DotPath.ToStdString();
  }

#if defined(_WIN32)
  auto const DotName = "dot.exe";
#else
  auto const DotName = "dot";
#endif

  auto SearchEnvPath = llvm::sys::findProgramByName(DotName);
  if (SearchEnvPath)
    return std::move(*SearchEnvPath);

  return std::string{};
}

namespace {

bool setPathForDotExecutable(wxString const &Path)
{
  auto const Config = wxConfig::Get();
  if (!Config->Write(cConfigKeyForDotPath, Path))
    return false;
  Config->Flush();
  return true;
}

}

bool StateGraphViewerPreferencesWindow::SaveValuesImpl()
{
  if (!m_DotFilePicker)
    return true;

  auto const Path = m_DotFilePicker->GetPath();
  if (!llvm::sys::fs::can_execute(Path.ToStdString())) {
    auto const ResText =
      Resource("TraceViewer")["GUIText"]["StateGraphViewerPreferences"];

    wxMessageDialog Dlg(this,
                        towxString(ResText["DotNotExecutableMessage"]),
                        towxString(ResText["DotNotExecutableCaption"]));
    Dlg.ShowModal();
    return false;
  }

  return setPathForDotExecutable(m_DotFilePicker->GetPath());
}

void StateGraphViewerPreferencesWindow::CancelChangesImpl() {}

wxString StateGraphViewerPreferencesWindow::GetDisplayNameImpl()
{
  return towxString(Resource("TraceViewer")
                    ["GUIText"]["StateGraphViewerPreferences"]["Title"]);
}

StateGraphViewerPreferencesWindow::StateGraphViewerPreferencesWindow()
: m_DotFilePicker(nullptr)
{}

StateGraphViewerPreferencesWindow
::StateGraphViewerPreferencesWindow(wxWindow *Parent)
: StateGraphViewerPreferencesWindow()
{
  Create(Parent);
}

StateGraphViewerPreferencesWindow::~StateGraphViewerPreferencesWindow()
  = default;

bool StateGraphViewerPreferencesWindow::Create(wxWindow *Parent)
{
  if (!wxWindow::Create(Parent, wxID_ANY)) {
    return false;
  }

  auto const ResText =
    Resource("TraceViewer")["GUIText"]["StateGraphViewerPreferences"];

  auto const DotFilePickerLabel =
    new wxStaticText(this, wxID_ANY, towxString(ResText["DotLocationLabel"]));

  auto const RestartForEffectLabel =
    new wxStaticText(this, wxID_ANY,
                     towxString(ResText["RestartForEffectLabel"]));

  m_DotFilePicker =
    new wxFilePickerCtrl(this, wxID_ANY,
      /* path */ getPathForDotExecutable(),
      /* message */ towxString(ResText["DotLocationPrompt"]),
      wxFileSelectorDefaultWildcardStr,
      wxDefaultPosition,
      wxDefaultSize,
      wxFLP_DEFAULT_STYLE | wxFLP_USE_TEXTCTRL | wxFLP_FILE_MUST_EXIST);

  // Vertical sizer to hold each row of input.
  auto const ParentSizer = new wxBoxSizer(wxVERTICAL);

  int const BorderDir = wxLEFT | wxRIGHT;
  int const BorderSize = 5;

  ParentSizer->AddSpacer(BorderSize);

  ParentSizer->Add(DotFilePickerLabel,
                   wxSizerFlags().Border(BorderDir, BorderSize));

  ParentSizer->Add(m_DotFilePicker,
                   wxSizerFlags().Expand().Border(BorderDir, BorderSize));

  ParentSizer->Add(RestartForEffectLabel,
                   wxSizerFlags().Border(BorderDir, BorderSize));

  ParentSizer->AddSpacer(BorderSize);

  SetSizerAndFit(ParentSizer);

  return true;
}
