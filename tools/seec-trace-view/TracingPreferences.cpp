//===- tools/seec-trace-view/TracingPreferences.cpp -----------------------===//
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
#include "seec/wxWidgets/ConfigTracing.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/config.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "TracingPreferences.hpp"

using namespace seec;

bool TracingPreferencesWindow::SaveValuesImpl()
{
  return setThreadEventLimit(m_ThreadEventLimit->GetValue())
      && setArchiveLimit(m_ArchiveLimit->GetValue());
}

wxString TracingPreferencesWindow::GetDisplayNameImpl()
{
  return towxString(Resource("TraceViewer")
                    ["GUIText"]["TracingPreferences"]["Title"]);
}

TracingPreferencesWindow::TracingPreferencesWindow()
: m_ThreadEventLimit(nullptr),
  m_ArchiveLimit(nullptr)
{}

TracingPreferencesWindow::TracingPreferencesWindow(wxWindow *Parent)
: TracingPreferencesWindow()
{
  Create(Parent);
}

TracingPreferencesWindow::~TracingPreferencesWindow() = default;

bool TracingPreferencesWindow::Create(wxWindow *Parent)
{
  if (!wxWindow::Create(Parent, wxID_ANY)) {
    return false;
  }

  auto const ResText = Resource("TraceViewer")["GUIText"]["TracingPreferences"];

  // Create slider to control the thread event limit.
  auto const ThreadEventLimitLabel =
    new wxStaticText(this, wxID_ANY, towxString(ResText["ThreadEventLimit"]));

  m_ThreadEventLimit = new wxSlider(this,
                                    wxID_ANY,
                                    /* value */   getThreadEventLimit(),
                                    /* minimum */ 1,
                                    /* maximum */ 1000,
                                    wxDefaultPosition,
                                    wxDefaultSize,
                                    wxSL_HORIZONTAL | wxSL_LABELS);

  // Create slider to control the archive limit.
  auto const ArchiveLimitLabel =
    new wxStaticText(this, wxID_ANY, towxString(ResText["ArchiveLimit"]));

  m_ArchiveLimit = new wxSlider(this,
                                wxID_ANY,
                                /* value */   getArchiveLimit(),
                                /* minimum */ 1,
                                /* maximum */ 1000,
                                wxDefaultPosition,
                                wxDefaultSize,
                                wxSL_HORIZONTAL | wxSL_LABELS);

  // Vertical sizer to hold each row of input.
  auto const ParentSizer = new wxBoxSizer(wxVERTICAL);

  int const BorderDir = wxLEFT | wxRIGHT;
  int const BorderSize = 5;

  ParentSizer->AddSpacer(BorderSize);

  ParentSizer->Add(ThreadEventLimitLabel,
                   wxSizerFlags().Border(BorderDir, BorderSize));

  ParentSizer->Add(m_ThreadEventLimit,
                   wxSizerFlags().Expand().Border(BorderDir, BorderSize));

  ParentSizer->Add(ArchiveLimitLabel,
                   wxSizerFlags().Border(BorderDir, BorderSize));

  ParentSizer->Add(m_ArchiveLimit,
                   wxSizerFlags().Expand().Border(BorderDir, BorderSize));

  ParentSizer->AddSpacer(BorderSize);

  SetSizerAndFit(ParentSizer);

  return true;
}
