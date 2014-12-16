//===- tools/seec-trace-view/AugmentationSettings.cpp ---------------------===//
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
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/AugmentationCollectionDataViewModel.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <llvm/Support/FileSystem.h>

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/mstream.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>
#include <wx/uri.h>
#include <wx/wfstream.h>
#include <wx/xml/xml.h>

#include <curl/curl.h>

#include <cstdlib>

#include "AugmentationSettings.hpp"
#include "TraceViewerApp.hpp"

using namespace seec;

//------------------------------------------------------------------------------
// CURLDownloadDialog
//------------------------------------------------------------------------------

/// \brief Show a progress dialog while downloading a file into memory.
///
class CURLDownloadDialog final : public wxProgressDialog
{
private:
  std::string m_URL;

  std::vector<char> m_Data;

  CURLcode m_Result;

  static size_t WriteData(char *Contents, size_t Size, size_t Count, void *User)
  {
    assert(User);
    auto &Storage = *static_cast<std::vector<char> *>(User);
    Storage.insert(Storage.end(), Contents, Contents + (Size * Count));
    return Size * Count;
  }

  static int Progress(void *ThisPtr,
                      double const dltotal,
                      double const dlnow,
                      double const ultotal,
                      double const ulnow)
  {
    assert(ThisPtr);
    auto const TypedThis = reinterpret_cast<CURLDownloadDialog *>(ThisPtr);

    if (dltotal) {
      TypedThis->SetRange(static_cast<int>(dltotal));
      auto const ShouldContinue = TypedThis->Update(static_cast<int>(dlnow));

      if (!ShouldContinue)
        return 1; // Cancel the CURL transfer.
    }

    return 0;
  }

public:
  CURLDownloadDialog(wxString const &Title,
                     wxString const &Message,
                     wxString const &URL,
                     wxWindow *Parent = nullptr)
  : wxProgressDialog(Title, Message, 1, Parent,
                     wxPD_AUTO_HIDE | wxPD_CAN_ABORT),
    m_URL(URL.ToStdString()),
    m_Data(),
    m_Result(CURLE_OK)
  {
    Pulse();
  }

  virtual ~CURLDownloadDialog() override = default;

  bool DoDownload();

  std::vector<char> &getData() { return m_Data; }

  CURLcode getResult() { return m_Result; }

  char const *getResultString() { return curl_easy_strerror(m_Result); }
};

bool CURLDownloadDialog::DoDownload()
{
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;

  // Clean up our CURL handle when we exit scope.
  auto Cleanup = seec::scopeExit([=] () { curl_easy_cleanup(curl); });

  curl_easy_setopt(curl, CURLOPT_URL, m_URL.c_str());

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CURLDownloadDialog::WriteData);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void *>(&m_Data));

  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
  curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,
                   &CURLDownloadDialog::Progress);
  curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);

  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  m_Result = curl_easy_perform(curl);

  return (m_Result == CURLE_OK);
}


//------------------------------------------------------------------------------
// AugmentationSettingsWindow
//------------------------------------------------------------------------------

wxString SaveAugmentation(wxXmlDocument &Doc, wxString &OutErr)
{
  // Save into the user local augmentation dir.
  auto const DirPath =
    AugmentationCollection::getUserLocalDataDirForAugmentations();

  // Create the directory if it doesn't already exist.
  wxFileName::DirName(DirPath).Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

  auto const Path = DirPath.ToStdString() + "%%%%%%%%.xml";

  llvm::SmallString<128> UniquePath;
  int UniqueFD = 0;
  auto const Err = llvm::sys::fs::createUniqueFile(Path, UniqueFD, UniquePath);

  if (Err != llvm::errc::success) {
    return wxString{};
  }

  auto const UniqueStream = fdopen(UniqueFD, "w");
  if (!UniqueStream) {
    OutErr = wxString(strerror(errno));
    return wxString{};
  }

  wxFFile File(UniqueStream);
  wxFFileOutputStream OutStream(File);
  if (!Doc.Save(OutStream)) {
    return wxString{};
  }

  OutStream.Close();
  File.Close();

  return wxString(UniquePath.str().str());
}

void AugmentationSettingsWindow::OnDownloadClick(wxCommandEvent &Ev)
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];

  wxTextEntryDialog URLDlg(this,
                           towxString(Res["DownloadMessage"]),
                           towxString(Res["DownloadCaption"]));

  if (URLDlg.ShowModal() != wxID_OK)
    return;

  CURLDownloadDialog DlDlg(towxString(Res["DownloadingTitle"]),
                           towxString(Res["DownloadingMessage"]),
                           URLDlg.GetValue(),
                           this);

  DlDlg.DoDownload();

  if (DlDlg.WasCancelled())
    return;

  if (DlDlg.getResult() != CURLE_OK) {
    wxMessageDialog Dlg(this,
                        DlDlg.getResultString(),
                        towxString(Res["FailCaption"]));
    Dlg.ShowModal();
    return;
  }

  auto const &DocData = DlDlg.getData();
  wxMemoryInputStream DocStream(DocData.data(), DocData.size());
  auto DocXml = seec::makeUnique<wxXmlDocument>(DocStream);

  if (!DocXml || !seec::isAugmentation(*DocXml)) {
    wxMessageDialog Dlg(this,
                        towxString(Res["InvalidTitle"]),
                        towxString(Res["InvalidMessage"]));
    Dlg.ShowModal();
    return;
  }

  // Generate an ID for this augmentation, if it doesn't already have one.
  auto RootNode = DocXml->GetRoot();
  if (!RootNode->HasAttribute("id"))
    RootNode->AddAttribute("id", URLDlg.GetValue());

  if (!RootNode->HasAttribute("source"))
    RootNode->AddAttribute("source", URLDlg.GetValue());

  if (RootNode->HasAttribute("downloaded"))
    RootNode->DeleteAttribute("downloaded");
  RootNode->AddAttribute("downloaded", wxDateTime::Now().FormatISOCombined());

  // Save the augmentation into a file.
  wxString Err;
  auto const Path = SaveAugmentation(*DocXml, Err);

  if (Path.empty()) {
    wxMessageDialog Dlg(this, towxString(Res["SaveFailTitle"]), Err);
    Dlg.ShowModal();
    return;
  }

  // Give to our App's collection.
  auto &Augmentations = wxGetApp().getAugmentations();
  Augmentations.loadFromFile(Path, Augmentation::EKind::UserLocal);
}

void AugmentationSettingsWindow::OnDeleteClick(wxCommandEvent &Ev)
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];

  wxDataViewItemArray SelectedItems;
  auto const Count = m_DataView->GetSelections(SelectedItems);

  if (Count < 1) {
    wxMessageBox(towxString(Res["DeleteNoneMessage"]),
                 towxString(Res["DeleteNoneCaption"]));
    return;
  }

  auto &Collection = wxGetApp().getAugmentations();
  auto const &Augmentations = Collection.getAugmentations();

  for (int i = 0; i < Count; ++i) {
    auto const Row = m_DataModel->GetRow(SelectedItems[i]);
    assert(0 <= Row && Row < Augmentations.size());

    if (Augmentations[Row].getKind() != Augmentation::EKind::UserLocal) {
      wxMessageBox(towxString(Res["DeleteNonUserLocalMessage"]),
                   towxString(Res["DeleteNonUserLocalCaption"]));
    }
    else {
      auto const Result = Collection.deleteUserLocalAugmentation(Row);
      if (!Result) {
        wxMessageBox(towxString(Res["DeleteFailedMessage"]),
                     towxString(Res["DeleteFailedCaption"]));
      }
    }
  }
}

bool AugmentationSettingsWindow::SaveValuesImpl()
{
  return true;
}

wxString AugmentationSettingsWindow::GetDisplayNameImpl()
{
  auto const ResTraceViewer = Resource("TraceViewer");
  auto const Title = ResTraceViewer["GUIText"]["AugmentationSettings"]["Title"]
                      .asStringOrDefault("Augmentations");

  return towxString(Title);
}

AugmentationSettingsWindow::AugmentationSettingsWindow()
: m_DataView(nullptr),
  m_DataModel(nullptr)
{}

AugmentationSettingsWindow::AugmentationSettingsWindow(wxWindow* Parent)
: AugmentationSettingsWindow()
{
  Create(Parent);
}

bool AugmentationSettingsWindow::Create(wxWindow* Parent)
{
  if (!wxWindow::Create(Parent, wxID_ANY))
    return false;

  auto const ResTraceViewer = Resource("TraceViewer");
  auto const ResText = ResTraceViewer["GUIText"]["AugmentationSettings"];

  // Button for downloading new augmentations.
  auto const DownloadButton =
    new wxButton(this, wxID_ANY, towxString(ResText["Download"]));

  DownloadButton->Bind(wxEVT_BUTTON,
    seec::make_function(this, &AugmentationSettingsWindow::OnDownloadClick));

  // Button for deleting existing augmentations.
  auto const DeleteButton =
    new wxButton(this, wxID_ANY, towxString(ResText["Delete"]));

  DeleteButton->Bind(wxEVT_BUTTON,
    seec::make_function(this, &AugmentationSettingsWindow::OnDeleteClick));

  // Setup the data view showing all loaded augmentations.
  auto &Augmentations = wxGetApp().getAugmentations();
  auto const Data = m_DataView = new wxDataViewCtrl(this, wxID_ANY);

  m_DataModel = new AugmentationCollectionDataViewModel(Augmentations);
  Data->AssociateModel(m_DataModel);
  Data->AppendColumn(AugmentationCollectionDataViewModel::getEnabledColumn()
                      .release());
  Data->AppendColumn(AugmentationCollectionDataViewModel::getNameColumn()
                      .release());
  Data->AppendColumn(AugmentationCollectionDataViewModel::getSourceColumn()
                      .release());
  Data->AppendColumn(AugmentationCollectionDataViewModel::getVersionColumn()
                      .release());

  // Vertical sizer to hold each row of input.
  auto const ParentSizer = new wxBoxSizer(wxVERTICAL);

  ParentSizer->Add(Data, wxSizerFlags().Proportion(1)
                                       .Expand()
                                       .Border(wxALL, 5));

  // Horizontal sizer for the buttons.
  auto const ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
  ButtonSizer->Add(DownloadButton, wxSizerFlags());
  ButtonSizer->AddStretchSpacer();
  ButtonSizer->Add(DeleteButton, wxSizerFlags());

  ParentSizer->Add(ButtonSizer, wxSizerFlags().Expand().Border(wxALL, 5));

  SetSizerAndFit(ParentSizer);

  return true;
}

AugmentationSettingsWindow::~AugmentationSettingsWindow() = default;
