//===- tools/seec-trace-view/TraceViewerApp.cpp ---------------------------===//
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
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Resources.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"
#include "seec/wxWidgets/Config.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"

#include <unicode/resbund.h>

#include "seec/wxWidgets/ICUBundleFSHandler.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/dir.h>
#include <wx/filesys.h>
#include <wx/ipc.h>
#include <wx/snglinst.h>
#include <wx/stdpaths.h>
#if defined(_WIN32)
#include <wx/msw/registry.h>
#endif

#include <curl/curl.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <set>

#include "ActionRecord.hpp"
#include "ActionRecordSettings.hpp"
#include "ColourSchemeSettings.hpp"
#include "CommonMenus.hpp"
#include "LocaleSettings.hpp"
#include "OpenTrace.hpp"
#include "Preferences.hpp"
#include "TraceViewerApp.hpp"
#include "TraceViewerFrame.hpp"
#include "WelcomeFrame.hpp"


/// \brief Get the topic for raising the primary instance.
static constexpr char const *getIPCTopicRaise() { return "RAISE"; }

/// \brief Get the topic for opening files in the primary instance.
static constexpr char const *getIPCTopicOpen()  { return "OPEN"; }

/// \brief Get the service name to use for IPC.
///
static wxString getIPCService() {
  auto &StdPaths = wxStandardPaths::Get();

  wxFileName ServicePath;
  ServicePath.AssignDir(StdPaths.GetUserLocalDataDir());

  if (!wxDirExists(ServicePath.GetFullPath()))
    if (!wxMkdir(ServicePath.GetFullPath()))
      return wxEmptyString;

  ServicePath.SetFullName("instanceipc");
  return ServicePath.GetFullPath();
}

//------------------------------------------------------------------------------
// ServerConnection
//------------------------------------------------------------------------------

/// \brief Connection from a non-primary instance.
///
class ServerConnection final : public wxConnection
{
public:
  /// \brief Destructor. This will disconnect the connection.
  ///
  virtual ~ServerConnection()
  {
    Disconnect();
  }

  /// \brief Receive exec commands from the non-primary instance.
  /// We simply forward the appropriate information to the \c TraceViewerApp.
  ///
  virtual bool OnExec(wxString const &Topic, wxString const &Data)
  {
    auto &App = wxGetApp();

    if (Topic == getIPCTopicRaise()) {
      App.Raise();
    }
    else if (Topic == getIPCTopicOpen()) {
      App.MacOpenFile(Data);
    }

    return true;
  }
};

//------------------------------------------------------------------------------
// SingleInstanceServer
//------------------------------------------------------------------------------

/// \brief Receives connections from non-primary instances.
///
class SingleInstanceServer final : private wxServer
{
  /// \brief Constructor.
  ///
  SingleInstanceServer() = default;

public:
  /// \brief Create a new server, ready to receive connections.
  ///
  static std::unique_ptr<SingleInstanceServer> create()
  {
    auto const Service = getIPCService();
    if (Service.empty())
      return nullptr;

    std::unique_ptr<SingleInstanceServer> Ptr (new SingleInstanceServer());
    if (!Ptr->Create(Service))
      return nullptr;

    return Ptr;
  }

  /// \brief Create a \c ServerConnection for a new connection.
  ///
  virtual wxConnectionBase *OnAcceptConnection(wxString const &Topic) override
  {
    return new ServerConnection();
  }
};


//------------------------------------------------------------------------------
// ClientConnection
//------------------------------------------------------------------------------

/// \brief Connection for sending information to the primary/single instance.
///
class ClientConnection final : public wxConnection
{
public:
  /// \brief Destructor. Will disconnect the connection.
  ///
  virtual ~ClientConnection() override
  {
    Disconnect();
  }
};


//------------------------------------------------------------------------------
// SingleInstanceClient
//------------------------------------------------------------------------------

/// \brief Client for sending information to the primary/single instance.
///
class SingleInstanceClient : private wxClient
{
  /// Current connection to the single instance.
  std::unique_ptr<ClientConnection> Connection;

  /// \brief Create a new connection object to use.
  ///
  wxConnectionBase *OnMakeConnection()
  {
    return new ClientConnection();
  }

public:
  /// \brief Constructor.
  ///
  SingleInstanceClient() = default;

  /// \brief Destructor.
  ///
  virtual ~SingleInstanceClient() final = default;

  /// \brief Establish a connection to the single instance with the given topic.
  ///
  bool Connect(wxString const &Topic);

  /// \brief Terminate the current connection.
  ///
  void Disconnect();

  /// \brief Check if a connection exists.
  ///
  bool IsConnected() const { return Connection != nullptr; }

  /// \brief Get the current connection.
  /// pre: IsConnected() == true.
  ///
  ClientConnection &GetConnection() { return *Connection; }
};

bool SingleInstanceClient::Connect(wxString const &Topic)
{
  Disconnect();

  auto const Host = "localhost";
  auto const Service = getIPCService();
  if (Service.empty())
    return false;

  Connection.reset(static_cast<ClientConnection *>
                              (MakeConnection(Host, Service, Topic)));

  return Connection != nullptr;
}

void SingleInstanceClient::Disconnect()
{
  Connection.reset();
}


//------------------------------------------------------------------------------
// setupWebControl
//------------------------------------------------------------------------------

#if defined(_WIN32)
int getIEVersion()
{
  wxRegKey IEKey(wxRegKey::HKLM, "Software\\Microsoft\\Internet Explorer");
  wxString Value;

  if (IEKey.QueryValue("svcVersion", Value, /* raw */ true)) {
    auto const StdValue = Value.ToStdString();
    return std::stoi(StdValue);
  }

  return 0;
}

int convertIEVersionToEmulationValue(int IEVersion)
{
  if (IEVersion >= 11) return 11000;
  switch (IEVersion) {
    case 10: return 10000;
    case 9:  return 9000;
    case 8:  return 8000;
    default: return 7000;
  }
}

void setWebBrowserEmulationMode()
{
  wxRegKey EmulationKey(wxRegKey::HKCU,
    "Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\"
    "FEATURE_BROWSER_EMULATION");

  auto const IEVersion = getIEVersion();
  auto const EmulationValue = convertIEVersionToEmulationValue(IEVersion);
  llvm::errs() << "ie version " << IEVersion
               << "; emulation mode " << EmulationValue << "\n";
  EmulationKey.SetValue("seec-view.exe", EmulationValue);
}
#endif


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

// Define the event table for TraceViewerApp.
BEGIN_EVENT_TABLE(TraceViewerApp, wxApp)
  EVT_MENU(wxID_OPEN, TraceViewerApp::OnCommandOpen)
  EVT_MENU(wxID_EXIT, TraceViewerApp::OnCommandExit)
  EVT_MENU(wxID_PREFERENCES, TraceViewerApp::OnCommandPreferences)
END_EVENT_TABLE()


//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

IMPLEMENT_APP(TraceViewerApp)


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

void TraceViewerApp::deferToExistingInstance()
{
  SingleInstanceClient Client;

  if (CLFiles.empty()) {
    // If the user has simply tried to open the viewer, then tell the existing
    // viewer to show itself.
    if (!Client.Connect(getIPCTopicRaise())) {
      wxLogDebug("Couldn't communicate with existing instance.");
      return;
    }

    Client.GetConnection().Execute(wxEmptyString);
  }
  else {
    // If the user is attempting to open trace files, then send the names to
    // the existing viewer so that it can open each of them.
    if (!Client.Connect(getIPCTopicOpen())) {
      wxLogDebug("Couldn't communicate with existing instance.");
      return;
    }

    auto &Conn = Client.GetConnection();

    for (auto const &File : CLFiles) {
      wxFileName Path(File);
      Path.MakeAbsolute();
      Conn.Execute(Path.GetFullPath());
    }
  }
}

void TraceViewerApp::OpenFile(wxString const &FileName) {
  // Attempt to read the trace, which should either return the newly read trace
  // (in Maybe slot 0), or an error message (in Maybe slot 1).
  auto NewTrace = OpenTrace::FromFilePath(FileName);
  assert(NewTrace.assigned());

  if (NewTrace.assigned<std::unique_ptr<OpenTrace>>()) {
    // The trace was read successfully, so create a new viewer to display it.
    auto TraceViewer =
      new TraceViewerFrame(nullptr,
                           NewTrace.move<std::unique_ptr<OpenTrace>>(),
                           wxID_ANY,
                           wxFileNameFromPath(FileName));
    
    TopLevelWindows.insert(TraceViewer);
    TraceViewer->Show(true);

    // Hide the Welcome frame (on Mac OS X), or destroy it (all others).
#ifdef __WXMAC__
    if (Welcome)
      Welcome->Show(false);
#else
    if (Welcome) {
      Welcome->Close(true);
      Welcome = nullptr;
    }
#endif
  }
  else if (NewTrace.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto const Message = NewTrace.get<seec::Error>
                                     ().getMessage(Status, getLocale());
    
    
    
    // Display the error that occured.
    auto ErrorDialog = new wxMessageDialog(nullptr, seec::towxString(Message));
    ErrorDialog->ShowModal();
    ErrorDialog->Destroy();
  }
}

TraceViewerApp::TraceViewerApp()
: wxApp(),
  SingleInstanceChecker(),
  Server(),
  Welcome(nullptr),
  TopLevelWindows(),
  LogWindow(nullptr),
  ICUResources(),
  Augmentations(),
  CLFiles(),
  CURL(curl_global_init(CURL_GLOBAL_DEFAULT) == 0),
  RecordingSubmitter()
{}

TraceViewerApp::~TraceViewerApp()
{
  // This function is not thread safe and should not be called when any other
  // thread is running. While no other threads exist during the construction
  // of TraceViewerApp, some exist during its destruction. We assume that they
  // are not running and it is safe to call this.
  curl_global_cleanup();
}

bool TraceViewerApp::OnInit() {
  // Find the path to the executable.
  auto &StdPaths = wxStandardPaths::Get();
  auto const ExecutablePath = StdPaths.GetExecutablePath().ToStdString();

  // Set the app name to "seec" so that we share configuration with other
  // SeeC applications (and the runtime library).
  this->SetAppName("seec");
  this->SetAppDisplayName("SeeC");

  // Load ICU resources for TraceViewer. Do this before calling wxApp's default
  // behaviour, so that OnInitCmdLine and OnCmdLineParsed have access to the
  // localized resources.
  auto const ResourcePath = seec::getResourceDirectory(ExecutablePath);
  ICUResources.reset(new seec::ResourceLoader(ResourcePath));
  
  std::array<char const *, 5> ResourceList {
    {"SeeCClang", "ClangEPV", "Trace", "TraceViewer", "RuntimeErrors"}
  };
  
  if (!ICUResources->loadResources(ResourceList))
    HandleFatalError("Couldn't load resources!");
  
  Augmentations = seec::makeUnique<seec::AugmentationCollection>();

  // Call default behaviour.
  if (!wxApp::OnInit())
    return false;

  // Ensure that no other trace viewers are open. If another trace viewer has
  // been open, then send information over to it before we terminate (e.g. any
  // files that the user has requested to open).
  SingleInstanceChecker.reset(new wxSingleInstanceChecker());
  if (SingleInstanceChecker->CreateDefault()) {
    if (SingleInstanceChecker->IsAnotherRunning()) {
      deferToExistingInstance();
      return false;
    }
  }
  else {
    // TODO: Notify the user.
    wxLogDebug("Couldn't check for existing instance.");
  }

  // Setup server to receive information from other instances (see above).
  Server = SingleInstanceServer::create();

  // Setup our configuration file location.
  if (!seec::setupCommonConfig()) {
    HandleFatalError("Failed to setup configuration.");
  }
  
  // Set ICU's default Locale according to the user's preferences.
  UErrorCode Status = U_ZERO_ERROR;
  icu::Locale::setDefault(getLocale(), Status);

  // Load resource augmentations from the resource directory.
  Augmentations->loadFromResources(ResourcePath);
  Augmentations->loadFromUserLocalDataDir();

  // Setup the colour scheme.
  ColourScheme = seec::makeUnique<ColourSchemeSettings>();
  ColourScheme->loadUserScheme();


  // Setup WebBrowser emulation version for windows.
#if defined(_WIN32)
  setWebBrowserEmulationMode();
#endif

#ifdef SEEC_SHOW_DEBUG
  // Setup the debugging log window.
  LogWindow = new wxLogWindow(nullptr, "Log");
#endif
  
  // Initialize the wxImage image handlers.
  wxInitAllImageHandlers();
  
  // Enable wxWidgets virtual file system access to the ICU bundles.
  wxFileSystem::AddHandler(new seec::ICUBundleFSHandler());
  
  // Get the GUIText from the TraceViewer ICU resources.
  Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     getLocale(),
                                     Status,
                                     "GUIText");
  if (U_FAILURE(Status))
    HandleFatalError("Couldn't load resource bundle TraceViewer->GUIText!");

  // Setup OS-X behaviour.
#ifdef __WXMAC__
  wxApp::SetExitOnFrameDelete(false);

  // Setup common menus.
  auto menuFile = new wxMenu();
  menuFile->Append(wxID_OPEN);
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

  auto menuBar = new wxMenuBar();
  menuBar->Append(menuFile,
                  seec::getwxStringExOrEmpty(TextTable, "Menu_File"));
  append(menuBar, createRecordingMenu(*this));

  wxMenuBar::MacSetCommonMenuBar(menuBar);
#endif

  // Setup the welcome frame.
  Welcome = new WelcomeFrame(nullptr,
                             wxID_ANY,
                             seec::getwxStringExOrEmpty(TextTable,
                                                        "Welcome_Title"),
                             wxDefaultPosition,
                             wxDefaultSize);
  Welcome->Show(true);

  // Setup the action recording submitter.
#ifdef SEEC_USER_ACTION_RECORDING
  RecordingSubmitter.reset(new ActionRecordingSubmitter());
#endif

  // On Mac OpenFile is called automatically. On all other platforms, manually
  // open any files that the user passed on the command line.
#ifndef __WXMAC__
  for (auto const &File : CLFiles) {
    OpenFile(File);
  }
#endif

  return true;
}

void TraceViewerApp::OnInitCmdLine(wxCmdLineParser &Parser) {
  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     getLocale(),
                                     Status,
                                     "GUIText");
  if (U_FAILURE(Status))
    HandleFatalError("Couldn't load resource bundle TraceViewer->GUIText!");
  
  Parser.AddSwitch(wxT("h"), wxT("help"),
                   seec::getwxStringExOrEmpty(TextTable, "CmdLine_Help"),
                   wxCMD_LINE_OPTION_HELP);
  
  Parser.AddParam(seec::getwxStringExOrEmpty(TextTable, "CmdLine_Files"),
                  wxCMD_LINE_VAL_STRING,
                  wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL);
}

bool TraceViewerApp::OnCmdLineParsed(wxCmdLineParser &Parser) {
  if (Parser.Found(wxT("h"))) {
    
  }
  
  for (unsigned i = 0; i < Parser.GetParamCount(); ++i) {
    CLFiles.emplace_back(Parser.GetParam(i));
  }
  
  return true;
}

void TraceViewerApp::MacNewFile() {
  // TODO
}

void TraceViewerApp::MacOpenFiles(wxArrayString const &FileNames) {
  // TODO: In the future we could check if the files are source files, in which
  // case we might compile them for the user (and possibly automatically
  // generate a trace file).
  for (wxString const &FileName : FileNames) {
    OpenFile(FileName);
  }
}

void TraceViewerApp::MacOpenFile(wxString const &FileName) {
  OpenFile(FileName);
}

void TraceViewerApp::MacReopenApp() {  
  if (TopLevelWindows.size() == 0) {
    // Re-open welcome frame, if it exists.
    if (Welcome)
      Welcome->Show(true);
  }
}

void TraceViewerApp::OnCommandOpen(wxCommandEvent &WXUNUSED(Event)) {
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     getLocale(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Create the open file dialog.
  wxFileDialog *OpenDialog
    = new wxFileDialog(nullptr,
                       seec::getwxStringExOrDie(TextTable,
                                                "OpenTrace_Title"),
                       wxEmptyString,
                       wxEmptyString,
                       seec::getwxStringExOrDie(TextTable,
                                                "OpenTrace_FileType"),
                       wxFD_OPEN,
                       wxDefaultPosition);

  // Destroy the dialog when we leave this scope.
  auto DestroyDialog = seec::scopeExit([=](){OpenDialog->Destroy();});

  // Show the dialog and exit if the user didn't select a file.
  if (OpenDialog->ShowModal() != wxID_OK)
    return;

  OpenFile(OpenDialog->GetPath());
}

void TraceViewerApp::OnCommandExit(wxCommandEvent &WXUNUSED(Event)) {
#ifdef __WXMAC__
  wxApp::SetExitOnFrameDelete(true);
#endif

  bool WindowsClosed = false;

#ifdef SEEC_SHOW_DEBUG
  if (LogWindow) {
    if (auto Frame = LogWindow->GetFrame()) {
      Frame->Close(true);
      LogWindow = nullptr;
      WindowsClosed = true;
    }
  }
#endif

  if (Welcome) {
    Welcome->Close(true);
    Welcome = nullptr;
    WindowsClosed = true;
  }

  for (auto Window : TopLevelWindows) {
    Window->Close(true);
    WindowsClosed = true;
  }
  TopLevelWindows.clear();

#ifdef __WXMAC__
  // On Mac OS X, there may be no top-level windows when the the exit command
  // is raised (i.e. if the user closed the welcome frame and all trace frames
  // before attempting to quit the program). In this case we must exit manually:
  if (!WindowsClosed)
    ExitMainLoop();
#endif
}

void TraceViewerApp::OnCommandPreferences(wxCommandEvent &Event)
{
  showPreferenceDialog();
}

void TraceViewerApp::Raise()
{
  if (!TopLevelWindows.empty()) {
    for (auto const Window : TopLevelWindows) {
      Window->Raise();
    }
  }
  else if(Welcome) {
    Welcome->Show();
    Welcome->Raise();
  }
}

void TraceViewerApp::HandleFatalError(wxString Description) {
  // Show an error dialog for the user.
  auto ErrorDialog = new wxMessageDialog(NULL,
                                         Description,
                                         "Fatal error!",
                                         wxOK,
                                         wxDefaultPosition);
  ErrorDialog->ShowModal();

  std::exit(EXIT_FAILURE);
}

ActionRecordingSubmitter *TraceViewerApp::getActionRecordingSubmitter() const
{
  return RecordingSubmitter.get();
}
