//===- main.cpp - SeeC Trace Viewer ---------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Clang/MappedAST.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "clang/Basic/Diagnostic.h"
// #include "clang/Basic/SourceManager.h"
#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/LLVMContext.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <unicode/resbund.h>

#include <wx/wx.h>
#include <wx/stdpaths.h>

#include <memory>


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

class TraceViewerApp : public wxApp
{
  /// \name Interface to wxApp.
  /// @{

  virtual bool OnInit();

  /// @}


  /// \name TraceViewer specific.
  /// @{

  std::unique_ptr<seec::ResourceLoader> ICUResources;

  void HandleFatalError(wxString Description);

  /// @}
};


//------------------------------------------------------------------------------
// OpenTrace
//------------------------------------------------------------------------------

class OpenTrace
{
#if 0
  llvm::LLVMContext &Context;

  seec::trace::InputBufferAllocator BufferAllocator;

  std::unique_ptr<seec::trace::ProcessTrace> ProcTrace;

  llvm::Module *Module;

  std::unique_ptr<seec::ModuleIndex> ModuleIndex;

  /// Constructor.
  OpenTrace(llvm::sys::Path DirPath,
            seec::trace::ProcessTrace &&Trace)
  : Context(llvm::getGlobalContext()),
    BufferAllocator(DirPath),
    ProcTrace(Trace),
    Module(nullptr),
    ModuleIndex()
  {

  }
#endif

  OpenTrace() {}

  OpenTrace(OpenTrace const &) = delete;
  OpenTrace &operator=(OpenTrace const &) = delete;

public:
  ~OpenTrace() = default;

  /// Attempt to read a trace at the given FilePath.
  /// \param FilePath the path to the process trace file.
  /// \return a seec::util::Maybe. If the trace was successfully read, then the
  ///         first element will be active and will contain a std::unique_ptr
  ///         holding an OpenTrace. If an error occurred, then the second
  ///         element will be active and will contain a pointer to a
  ///         statically-allocated C-String containing a key that can be used
  ///         to lookup the error in the GUIText table.
  static
  seec::util::Maybe<std::unique_ptr<OpenTrace>,
                    char const *>
  FromFilePath(wxString const &FilePath) {
    typedef seec::util::Maybe<std::unique_ptr<OpenTrace>, char const *> RetTy;

    // Create an InputBufferAllocator for the folder containing the trace file.
    wxStandardPaths StdPaths;
    auto const &ExecutablePath = StdPaths.GetExecutablePath().ToStdString();

    llvm::sys::Path DirPath {FilePath.ToStdString()};
    DirPath.eraseComponent(); // Erase the filename from the path.

    seec::trace::InputBufferAllocator BufferAllocator{DirPath};

    // Read the process trace using the InputBufferAllocator.
    auto MaybeProcTrace = seec::trace::ProcessTrace::readFrom(BufferAllocator);
    if (!MaybeProcTrace.assigned(0)) {
      return RetTy("OpenTrace_Error_LoadProcessTrace");
    }

    auto ProcTrace = std::move(MaybeProcTrace.get<0>());
    assert(ProcTrace);

    // Load the bitcode.
    llvm::LLVMContext &Context = llvm::getGlobalContext();

    llvm::SMDiagnostic ParseError;
    llvm::Module *Mod = llvm::ParseIRFile(ProcTrace->getModuleIdentifier(),
                                          ParseError,
                                          Context);
    if (!Mod) {
      // ParseError.print("seec-trace-view", llvm::errs());
      return RetTy("OpenTrace_Error_ParseIRFile");
    }

    // Index the llvm::Module.
    seec::ModuleIndex ModIndex {*Mod, true};

    // Ignore Clang diagnostics.
    clang::IgnoringDiagConsumer DiagConsumer;

    llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics
      = new clang::DiagnosticsEngine(
          llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs>
                                  (new clang::DiagnosticIDs()),
          &DiagConsumer,
          false);

    Diagnostics->setSuppressSystemWarnings(true);
    Diagnostics->setIgnoreAllWarnings(true);

    // Setup the map to find Decls and Stmts from Instructions
    seec::seec_clang::MappedModule MapMod(*Mod,
                                          ExecutablePath,
                                          Diagnostics);

    return RetTy(std::unique_ptr<OpenTrace>(new OpenTrace()));
  }
};


//------------------------------------------------------------------------------
// TraceViewerFrame
//------------------------------------------------------------------------------

class TraceViewerFrame : public wxFrame
{
  // TODO: Store information about the currently-loaded trace.
  std::unique_ptr<OpenTrace> Trace;

public:
  TraceViewerFrame(wxString const &Title,
                   wxPoint const &Position,
                   wxSize const &Size);

#define SEEC_COMMAND_EVENT(EVENT) \
  void On##EVENT(wxCommandEvent &Event);
#include "TraceViewerFrameEvents.def"

  DECLARE_EVENT_TABLE()

  enum CommandEvent {
#define SEEC_COMMAND_EVENT(EVENT) \
    ID_##EVENT,
#include "TraceViewerFrameEvents.def"
  };
};

BEGIN_EVENT_TABLE(TraceViewerFrame, wxFrame)
#define SEEC_COMMAND_EVENT(EVENT) \
  EVT_MENU(ID_##EVENT, TraceViewerFrame::On##EVENT)
#include "TraceViewerFrameEvents.def"
END_EVENT_TABLE()


//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

IMPLEMENT_APP(TraceViewerApp)


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

bool TraceViewerApp::OnInit() {
  // Find the path to the executable.
  wxStandardPaths StdPaths;
  char const *ExecutablePath = StdPaths.GetExecutablePath().c_str();

  // Load ICU resources for TraceViewer.
  ICUResources.reset(new seec::ResourceLoader(llvm::sys::Path{ExecutablePath}));

  if (!ICUResources->loadResource("TraceViewer"))
    HandleFatalError("Couldn't load TraceViewer resources!");

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;

  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  if (U_FAILURE(Status))
    HandleFatalError("Couldn't load resource bundle TraceViewer->GUIText!");

  // Setup the main Frame.
  auto TitleStr = seec::getwxStringEx(TextTable, "FrameTitle", Status);
  if (U_FAILURE(Status))
    HandleFatalError("Couldn't load FrameTitle from GUIText resource bundle!");

  auto Frame = new TraceViewerFrame(TitleStr,
                                    wxPoint(50, 50),
                                    wxSize(450, 340));

  Frame->Show(true);

  SetTopWindow(Frame);

  return true;
}

void TraceViewerApp::HandleFatalError(wxString Description) {
  // TODO: If any frames exist, destroy them?

  // Show an error dialog for the user.
  auto ErrorDialog = new wxMessageDialog(NULL,
                                         Description,
                                         "Fatal error!",
                                         wxOK,
                                         wxDefaultPosition);

  ErrorDialog->ShowModal();

  exit(EXIT_FAILURE);
}


//------------------------------------------------------------------------------
// TraceViewerFrame
//------------------------------------------------------------------------------

TraceViewerFrame::TraceViewerFrame(wxString const &Title,
                                   wxPoint const &Position,
                                   wxSize const &Size)
: wxFrame(NULL, -1, Title, Position, Size)
{
  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;

  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Setup the menu bar.
  wxMenu *menuFile = new wxMenu();
  menuFile->Append(ID_OpenTrace,
                   seec::getwxStringExOrDie(TextTable, "Menu_File_Open"));
  menuFile->AppendSeparator();
  menuFile->Append(ID_Quit,
                   seec::getwxStringExOrDie(TextTable, "Menu_File_Exit"));

  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(menuFile,
                  seec::getwxStringExOrDie(TextTable, "Menu_File"));

  SetMenuBar(menuBar);

  // Setup a status bar.
  CreateStatusBar();
}

void TraceViewerFrame::OnQuit(wxCommandEvent &WXUNUSED(Event)) {
  Close(true);
}

void TraceViewerFrame::OnOpenTrace(wxCommandEvent &WXUNUSED(Event)) {
  UErrorCode Status = U_ZERO_ERROR;

  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  wxFileDialog *OpenDialog
    = new wxFileDialog(this,
                       seec::getwxStringExOrDie(TextTable,
                                                "OpenTrace_Title"),
                       wxEmptyString,
                       wxEmptyString,
                       seec::getwxStringExOrDie(TextTable,
                                                "OpenTrace_FileType"),
                       wxFD_OPEN,
                       wxDefaultPosition);

  if (OpenDialog->ShowModal() == wxID_OK) {
    auto NewTrace = OpenTrace::FromFilePath(OpenDialog->GetPath());
    if (NewTrace.assigned(0)) {
      Trace = std::move(NewTrace.get<0>());
    }
    else if (NewTrace.assigned(1)) {
      auto ErrorDialog
        = new wxMessageDialog(this,
                              seec::getwxStringExOrDie(TextTable,
                                                       NewTrace.get<1>()));
      ErrorDialog->ShowModal();
      ErrorDialog->Destroy();
    }
  }

  OpenDialog->Destroy();
}
