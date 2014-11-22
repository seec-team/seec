//===- tools/seec-trace-view/LocaleSettings.cpp ---------------------------===//
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
#include "seec/wxWidgets/ImageResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/bitmap.h>
#include <wx/bmpcbox.h>
#include <wx/config.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "LocaleSettings.hpp"

char const * const cConfigKeyForLocaleID = "/Locale/ID";

bool LocaleSettingsWindow::SaveValuesImpl()
{
  auto const Selection = m_Selector->GetSelection();
  if (Selection == wxNOT_FOUND)
    return false;

  auto const &TheLocale = m_AvailableLocales[Selection];

  auto const Config = wxConfig::Get();
  Config->Write(cConfigKeyForLocaleID, TheLocale.getName());
  Config->Flush();

  UErrorCode Status = U_ZERO_ERROR;
  icu::Locale::setDefault(getLocale(), Status);

  return true;
}

wxString LocaleSettingsWindow::GetDisplayNameImpl()
{
  auto const CurrentLocale = getLocale();
  auto const ResTraceViewer = seec::Resource("TraceViewer", CurrentLocale);
  auto const ResText = ResTraceViewer["GUIText"]["LocaleSettingsDialog"];
  return seec::towxString(ResText["Title"].asStringOrDefault("Locale"));
}

LocaleSettingsWindow::LocaleSettingsWindow()
: m_Selector(nullptr),
  m_AvailableLocales()
{}

LocaleSettingsWindow::LocaleSettingsWindow(wxWindow *Parent)
: m_Selector(nullptr),
  m_AvailableLocales()
{
  Create(Parent);
}

LocaleSettingsWindow::~LocaleSettingsWindow() = default;

bool LocaleSettingsWindow::Create(wxWindow *Parent)
{
  if (!wxWindow::Create(Parent, wxID_ANY)) {
    return false;
  }

  auto const CurrentLocale = getLocale();
  auto const ResTraceViewer = seec::Resource("TraceViewer", CurrentLocale);
  auto const ResFlags = ResTraceViewer["GUIImages"]["CountryFlags"];

  m_Selector = new wxBitmapComboBox(this,
                                    wxID_ANY,
                                    wxEmptyString,
                                    wxDefaultPosition,
                                    wxSize(300, wxDefaultSize.GetHeight()),
                                    0,
                                    nullptr,
                                    wxCB_READONLY);

  int CurrentLocaleIndex = wxNOT_FOUND;

  int32_t NumLocales = 0;
  if (auto const Locales = icu::Locale::getAvailableLocales(NumLocales)) {
    if (NumLocales > 0) {
      std::string FlagKey;
      UnicodeString DisplayName;

      // Get a "root" flag to use when we don't have a matching flag.
      auto const ResRootFlag = ResFlags["root"];
      UErrorCode RootFlagStatus = ResRootFlag.status();
      auto const RootFlag = seec::getwxImage(ResRootFlag.bundle(),
                                              RootFlagStatus);

      for (int32_t i = 0; i < NumLocales; ++i) {
        // Attempt to open the TraceViewer ResourceBundle using this Locale,
        // to check if SeeC has an appropriate translation.
        auto const ResForLocale = seec::Resource("TraceViewer", Locales[i]);

        if (ResForLocale.status() == U_ZERO_ERROR) {
          if (CurrentLocale == Locales[i])
            CurrentLocaleIndex = static_cast<int>(m_Selector->GetCount());

          Locales[i].getDisplayName(Locales[i], DisplayName);
          m_AvailableLocales.push_back(Locales[i]);

          FlagKey = Locales[i].getCountry();
          std::transform(FlagKey.begin(), FlagKey.end(), FlagKey.begin(),
                          ::tolower);

          auto const ResFlag = ResFlags[FlagKey.c_str()];
          UErrorCode Status = ResFlag.status();
          auto const Flag = seec::getwxImage(ResFlag.bundle(), Status);

          if (!FlagKey.empty() && U_FAILURE(Status)) {
            wxLogDebug("no flag found for '%s'", wxString(FlagKey));
          }

          if (U_SUCCESS(Status)) {
            m_Selector->Append(seec::towxString(DisplayName), wxBitmap(Flag));
          }
          else if (U_SUCCESS(RootFlagStatus)) {
            m_Selector->Append(seec::towxString(DisplayName),
                               wxBitmap(RootFlag));
          }
          else {
            m_Selector->Append(seec::towxString(DisplayName));
          }
        }
      }
    }
  }

  if (CurrentLocaleIndex != wxNOT_FOUND)
    m_Selector->SetSelection(CurrentLocaleIndex);

  // Vertical sizer to hold each row of input.
  auto const ParentSizer = new wxBoxSizer(wxVERTICAL);

  ParentSizer->Add(m_Selector, wxSizerFlags().Expand().Border(wxALL, 5));

  SetSizerAndFit(ParentSizer);

  return true;
}

icu::Locale getLocale()
{
  auto const Config = wxConfig::Get();

  wxString LocaleID;
  if (Config->Read(cConfigKeyForLocaleID, &LocaleID)) {
    auto const TheLocale = icu::Locale::createFromName(LocaleID);
    if (!TheLocale.isBogus())
      return TheLocale;
  }

  return icu::Locale();
}
