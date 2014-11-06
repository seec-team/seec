//===- tools/seec-trace-view/StateGraphViewer.cpp -------------------------===//
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

#include "seec/Clang/GraphLayout.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeFunction.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Range.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/CallbackFSHandler.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif

#if !wxUSE_WEBVIEW_WEBKIT && !wxUSE_WEBVIEW_IE
  #error "wxWebView backend required!"
#endif

#include <wx/stdpaths.h>
#include <wx/uri.h>
#include <wx/webview.h>
#include <wx/webviewfshandler.h>
#include <wx/wfstream.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"

#include <chrono>
#include <memory>
#include <string>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "CommonMenus.hpp"
#include "LocaleSettings.hpp"
#include "NotifyContext.hpp"
#include "ProcessMoveEvent.hpp"
#include "StateAccessToken.hpp"
#include "StateGraphViewer.hpp"


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

static std::string FindDotExecutable()
{
  auto const DotName = "dot";

  auto const LLVMSearch = llvm::sys::FindProgramByName(DotName);
  if (!LLVMSearch.empty())
    return LLVMSearch;
  
  char const *SearchPaths[] = {
    "/usr/bin",
    "/usr/local/bin"
  };
  
  llvm::SmallString<256> DotPath;
  
  for (auto const SearchPath : seec::range(SearchPaths)) {
    DotPath = SearchPath;
    llvm::sys::path::append(DotPath, DotName);
    
    if (!llvm::sys::fs::exists(DotPath.str())) {
      wxLogDebug("dot does not exist at %s", wxString(DotPath.str()));
      continue;
    }
    
    if (!llvm::sys::fs::can_execute(DotPath.str())) {
      wxLogDebug("dot not executable at %s", wxString(DotPath.str()));
      continue;
    }
    
    return DotPath.str().str();
  }
  
  return std::string{};
}


//------------------------------------------------------------------------------
// Displayable
//------------------------------------------------------------------------------

Displayable::~Displayable() = default;


//------------------------------------------------------------------------------
// GraphRenderedEvent
//------------------------------------------------------------------------------

/// \brief Used to send a rendered graph back to the GUI thread.
///
class GraphRenderedEvent : public wxEvent
{
  std::shared_ptr<wxString const> SetStateScript;
  
public:
  wxDECLARE_CLASS(GraphRenderedEvent);
  
  /// \brief Constructor.
  ///
  GraphRenderedEvent(wxEventType EventType,
                     int WinID,
                     std::shared_ptr<wxString const> WithSetStateScript)
  : wxEvent(WinID, EventType),
    SetStateScript(std::move(WithSetStateScript))
  {}
  
  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new GraphRenderedEvent(*this);
  }
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the SetState() script.
  ///
  wxString const &getSetStateScript() const { return *SetStateScript; }
  
  /// @} (Accessors.)
};

IMPLEMENT_CLASS(GraphRenderedEvent, wxEvent)

wxDEFINE_EVENT(SEEC_EV_GRAPH_RENDERED, GraphRenderedEvent);


//------------------------------------------------------------------------------
// MouseOverDisplayableEvent
//------------------------------------------------------------------------------

/// \brief Used to notify the GUI thread that the mouse has moved over an item.
///
class MouseOverDisplayableEvent : public wxEvent
{
  std::shared_ptr<Displayable const> TheDisplayable;
  
public:
  wxDECLARE_CLASS(MouseOverDisplayableEvent);
  
  /// \brief Constructor.
  ///
  MouseOverDisplayableEvent(wxEventType EventType,
                            int WinID,
                            std::shared_ptr<Displayable const> WithDisplayable)
  : wxEvent(WinID, EventType),
    TheDisplayable(std::move(WithDisplayable))
  {}
  
  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new MouseOverDisplayableEvent(*this);
  }
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the Displayable.
  ///
  decltype(TheDisplayable) const &getDisplayableShared() const {
    return TheDisplayable;
  }
  
  /// @} (Accessors.)
};

IMPLEMENT_CLASS(MouseOverDisplayableEvent, wxEvent)

wxDEFINE_EVENT(SEEC_EV_MOUSE_OVER_DISPLAYABLE, MouseOverDisplayableEvent);


//------------------------------------------------------------------------------
// StateGraphViewerPanel
//------------------------------------------------------------------------------

std::string StateGraphViewerPanel::workerGenerateDot()
{
  // Lock the current state while we read from it.
  auto Lock = TaskAccess->getAccess();
  if (!Lock || !TaskProcess || !ContinueGraphGeneration)
    return std::string();

  std::lock_guard<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
  auto const Layout = LayoutHandler->doLayout(*TaskProcess,
                                              ContinueGraphGeneration);
  auto const GraphString = Layout.getDotString();

  return GraphString;
}

void StateGraphViewerPanel::workerTaskLoop()
{
  while (true)
  {
    std::unique_lock<std::mutex> Lock{TaskMutex};

    // Wait until the main thread gives us a task.
    TaskCV.wait(Lock);

    // This indicates that we should end the worker thread because the panel is
    // being destroyed.
    if (!TaskAccess && !TaskProcess)
      return;

    // Create a graph of the process state in dot format.
    auto const GraphString = workerGenerateDot();
    if (GraphString.empty()) {
      wxLogDebug("GraphString.empty()");
      continue;
    }

    // The remainder of the graph generation does not use the state, so we can
    // release access to the task information.
    Lock.unlock();

    // Write the graph to a temporary file.
    llvm::SmallString<256> GraphPath;

    {
      int GraphFD;
      auto const GraphErr =
        llvm::sys::fs::createTemporaryFile("seecgraph", "dot",
                                           GraphFD,
                                           GraphPath);

      if (GraphErr != llvm::errc::success) {
        wxLogDebug("Couldn't create temporary dot file: %s",
                   wxString(GraphErr.message()));
        continue;
      }

      llvm::raw_fd_ostream GraphStream(GraphFD, true);
      GraphStream << GraphString;
    }

    // Remove the temporary file when we exit this function.
    auto const RemoveGraph = seec::scopeExit([&] () {
                                bool Existed = false;
                                llvm::sys::fs::remove(GraphPath.str(), Existed);
                              });

    // Create a temporary filename for the dot result.
    llvm::SmallString<256> SVGPath;
    auto const SVGErr =
      llvm::sys::fs::createTemporaryFile("seecgraph", "svg", SVGPath);

    if (SVGErr != llvm::errc::success) {
      wxLogDebug("Couldn't create temporary svg file: %s",
                 wxString(SVGErr.message()));
      continue;
    }

    auto const RemoveSVG = seec::scopeExit([&] () {
                              bool Existed = false;
                              llvm::sys::fs::remove(SVGPath.str(), Existed);
                            });

    // Run dot using the temporary input/output files.
    char const *Args[] = {
      "dot",
      "-Gfontnames=svg",
#if defined(__APPLE__)
      "-Nfontname=\"Times-Roman\"",
#endif
      "-o",
      SVGPath.c_str(),
      "-Tsvg",
      GraphPath.c_str(),
      nullptr
    };

    char const *Environment[] = {
      PathToGraphvizLibraries.c_str(),
      PathToGraphvizPlugins.c_str(),
      nullptr
    };

    std::string ErrorMsg;

    bool ExecFailed = false;

    auto const Result = llvm::sys::ExecuteAndWait(PathToDot,
                                                  Args,
                                                  Environment,
                                                  /* redirects */ nullptr,
                                                  /* wait */ 0,
                                                  /* mem */ 0,
                                                  &ErrorMsg,
                                                  &ExecFailed);

    if (!ErrorMsg.empty()) {
      wxLogDebug("Dot failed: %s", ErrorMsg);
      continue;
    }

    if (Result) {
      wxLogDebug("Dot returned non-zero.");
      continue;
    }

    // Read the dot-generated SVG from the temporary file.
    llvm::OwningPtr<llvm::MemoryBuffer> SVGData;

    auto const ReadErr = llvm::MemoryBuffer::getFile(SVGPath.str(), SVGData);
    if (ReadErr != llvm::errc::success) {
      wxLogDebug("Couldn't read temporary svg file.");
      continue;
    }

    // Remove all non-print characters from the SVG and prepare it to be sent to
    // the WebView via javascript.
    auto SharedScript = std::make_shared<wxString>();
    auto &Script = *SharedScript;

    Script.reserve(SVGData->getBufferSize() + 256);
    Script << "SetState(\"";

    for (auto It = SVGData->getBufferStart(), End = SVGData->getBufferEnd();
        It != End;
        ++It)
    {
      auto const Ch = *It;

      if (std::isprint(Ch)) {
        if (Ch == '\\' || Ch == '"')
          Script << '\\';
        Script << Ch;
      }
    }

    Script << "\");";

    auto EvPtr = seec::makeUnique<GraphRenderedEvent>(SEEC_EV_GRAPH_RENDERED,
                                                      this->GetId(),
                                                      std::move(SharedScript));

    EvPtr->SetEventObject(this);

    wxQueueEvent(this->GetEventHandler(), EvPtr.release());
  }
}

void StateGraphViewerPanel::highlightValue(seec::cm::Value const *Value)
{
  wxString Script("HighlightValue(");
  Script << reinterpret_cast<uintptr_t>(Value);

  if (Value) {
    if (auto const Ptr = llvm::dyn_cast<seec::cm::ValueOfPointer>(Value)) {
      if (Ptr->getDereferenceIndexLimit()) {
        auto const Pointee = Ptr->getDereferenced(0);
        Script << ", " << reinterpret_cast<uintptr_t>(Pointee.get());
      }
    }
  }

  Script << ");";

  WebView->RunScript(Script);
}

void StateGraphViewerPanel::handleContextEvent(ContextEvent const &Ev)
{
  if (auto const HighlightEv = llvm::dyn_cast<ConEvHighlightValue>(&Ev)) {
    // We don't need to lock the highlight's access, as it must already
    // be locked by whoever raised the highlight event.
    if (CurrentAccess && CurrentAccess != HighlightEv->getAccess())
      return;

    highlightValue(HighlightEv->getValue());
  }
}

void StateGraphViewerPanel::replayMouseOverValue(uintptr_t Address,
                                                 std::string &TypeString)
{
  // Remove previous highlight. TODO: This is temporary because the recordings
  // don't receive "MouseOverNone" events correctly on OS X.
  highlightValue(nullptr);

  // Access the current state so that we can find the Value.
  auto Lock = CurrentAccess->getAccess();
  if (!Lock || !CurrentProcess)
    return;

  auto const Store = CurrentProcess->getCurrentValueStore();
  if (auto const V = Store->findFromAddressAndType(Address, TypeString)) {
    highlightValue(V.get());
  }
}

StateGraphViewerPanel::StateGraphViewerPanel()
: wxPanel(),
  Notifier(nullptr),
  Recording(nullptr),
  PathToDot(),
  PathToGraphvizLibraries(),
  PathToGraphvizPlugins(),
  CurrentAccess(),
  CurrentProcess(nullptr),
  WorkerThread(),
  TaskMutex(),
  TaskCV(),
  TaskAccess(),
  TaskProcess(nullptr),
  ContinueGraphGeneration(false),
  WebView(nullptr),
  LayoutHandler(),
  LayoutHandlerMutex(),
  CallbackFS(nullptr),
  MouseOver()
{}

StateGraphViewerPanel::StateGraphViewerPanel(wxWindow *Parent,
                                             ContextNotifier &WithNotifier,
                                             ActionRecord &WithRecording,
                                             ActionReplayFrame &WithReplay,
                                             wxWindowID ID,
                                             wxPoint const &Position,
                                             wxSize const &Size)
: StateGraphViewerPanel()
{
  Create(Parent, WithNotifier, WithRecording, WithReplay, ID, Position, Size);
}

StateGraphViewerPanel::~StateGraphViewerPanel()
{
  // Shutdown our worker thread (it will terminate when it receives the
  // notification and there is no corresponding task).
  std::unique_lock<std::mutex> Lock{TaskMutex};
  TaskAccess.reset();
  TaskProcess = nullptr;
  Lock.unlock();
  TaskCV.notify_one();
  WorkerThread.join();

  wxFileSystem::RemoveHandler(CallbackFS);
}

bool StateGraphViewerPanel::Create(wxWindow *Parent,
                                   ContextNotifier &WithNotifier,
                                   ActionRecord &WithRecording,
                                   ActionReplayFrame &WithReplay,
                                   wxWindowID ID,
                                   wxPoint const &Position,
                                   wxSize const &Size)
{
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;
  
  Notifier = &WithNotifier;
  Recording = &WithRecording;
  
  // Enable vfs access to request information about the state.
  auto const ThisAddr = reinterpret_cast<uintptr_t>(this);
  auto const CallbackProto = std::string{"seec"} + std::to_string(ThisAddr);
  
  CallbackFS = new seec::CallbackFSHandler(CallbackProto);
  
  CallbackFS->addCallback("notify_hover",
    std::function<void (std::string const &)>{
      [this] (std::string const &NodeID) -> void {
        this->OnMouseOver(NodeID);
      }
    });
  
  CallbackFS->addCallback("notify_contextmenu",
    std::function<void (std::string const &)>{
      [this] (std::string const &Foo) -> void {
        this->RaiseContextMenu();
      }
    });
  
  CallbackFS->addCallback("log_debug",
    std::function<void (std::string const &)>{
      [] (std::string const &Message) -> void {
        wxLogDebug("%s", wxString{Message});
      }
    });

  Bind(wxEVT_CONTEXT_MENU,
       &StateGraphViewerPanel::OnContextMenu, this);
  Bind(SEEC_EV_MOUSE_OVER_DISPLAYABLE,
       &StateGraphViewerPanel::OnMouseOverDisplayable, this);

  wxFileSystem::AddHandler(CallbackFS);
  
  // Get our resources from ICU.
  UErrorCode Status = U_ZERO_ERROR;
  auto Resources = seec::getResource("TraceViewer",
                                     getLocale(),
                                     Status,
                                     "StateGraphViewer");
  
  if (!U_SUCCESS(Status))
    return false;
  
  auto Sizer = new wxBoxSizer(wxVERTICAL);
  
  // Setup the webview.
  WebView = wxWebView::New(this, wxID_ANY);
  if (!WebView) {
    wxLogDebug("wxWebView::New failed.");
    return false;
  }
  
  WebView->RegisterHandler(wxSharedPtr<wxWebViewHandler>
                                      (new wxWebViewFSHandler("icurb")));
  WebView->RegisterHandler(wxSharedPtr<wxWebViewHandler>
                                      (new wxWebViewFSHandler(CallbackProto)));
  
  Sizer->Add(WebView, wxSizerFlags(1).Expand());
  SetSizerAndFit(Sizer);
  
  // Find the dot executable.
  PathToDot = FindDotExecutable();
  
  if (!PathToDot.empty())
  {
    // Determine the path to Graphviz's libraries, based on the location of dot.
    llvm::SmallString<256> PluginPath (PathToDot);
    
    llvm::sys::path::remove_filename(PluginPath);    // */bin/dot -> */bin
    llvm::sys::path::remove_filename(PluginPath);    // */bin    -> *
    llvm::sys::path::append(PluginPath, "lib");      // *      -> */lib
    
    PathToGraphvizLibraries = "DYLD_LIBRARY_PATH=";
    PathToGraphvizLibraries += PluginPath.str();
    
    llvm::sys::path::append(PluginPath, "graphviz"); // */lib -> */lib/graphviz
    
    PathToGraphvizPlugins = "GVBINDIR=";
    PathToGraphvizPlugins += PluginPath.str();
    
    // Setup the layout handler.
    {
      std::lock_guard<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
      LayoutHandler.reset(new seec::cm::graph::LayoutHandler());
      LayoutHandler->addBuiltinLayoutEngines();
    }
    
    // Load the webpage.
    auto const WebViewURL =
      std::string{"icurb://TraceViewer/StateGraphViewer/WebViewHTML#"}
      + CallbackProto;
    
    WebView->LoadURL(WebViewURL);
    
    // Wire up our event handlers.
    Bind(SEEC_EV_GRAPH_RENDERED, &StateGraphViewerPanel::OnGraphRendered, this);
    
    // Register for context notifications.
    Notifier->callbackAdd([this] (ContextEvent const &Ev) -> void {
      this->handleContextEvent(Ev); });

    WithReplay.RegisterHandler("StateGraphViewer.MouseOverValue",
                               {{"address", "type"}},
      seec::make_function(this, &StateGraphViewerPanel::replayMouseOverValue));
  }
  else {
    // If the user navigates to a link, open it in the default browser.
    WebView->Bind(wxEVT_WEBVIEW_NAVIGATING,
      std::function<void (wxWebViewEvent &Event)>{
        [] (wxWebViewEvent &Event) -> void {
          if (Event.GetURL().StartsWith("http")) {
            wxLaunchDefaultBrowser(Event.GetURL());
            Event.Veto();
          }
          else
            Event.Skip();
        }});
    
    std::string const WebViewURL =
      "icurb://TraceViewer/StateGraphViewer/StateGraphViewerNoGraphviz.html";
    
    // Load the webpage explaining that dot is required.
    WebView->LoadURL(WebViewURL);
  }
  
  // Create the worker thread that will perform our graph generation.
  WorkerThread = std::thread{
    [this] () { this->workerTaskLoop(); }};

  return true;
}

void StateGraphViewerPanel::OnGraphRendered(GraphRenderedEvent const &Ev)
{
  WebView->RunScript(Ev.getSetStateScript());
}

void
StateGraphViewerPanel::
OnMouseOverDisplayable(MouseOverDisplayableEvent const &Ev)
{
  // Remove highlighting from the existing value (if any).
  if (auto const Prev = MouseOver.get()) {
    if (llvm::isa<DisplayableValue>(Prev)) {
      if (auto Access = CurrentAccess->getAccess())
        Notifier->createNotify<ConEvHighlightValue>(nullptr, CurrentAccess);
    }
    else if (llvm::isa<DisplayableDereference>(Prev)) {
      if (auto Access = CurrentAccess->getAccess())
        Notifier->createNotify<ConEvHighlightValue>(nullptr, CurrentAccess);
    }
    else if (llvm::isa<DisplayableFunctionState>(Prev)) {
      Notifier->createNotify<ConEvHighlightDecl>(nullptr);
    }
    else if (llvm::isa<DisplayableLocalState>(Prev)) {
      Notifier->createNotify<ConEvHighlightDecl>(nullptr);
    }
    else if (llvm::isa<DisplayableParamState>(Prev)) {
      Notifier->createNotify<ConEvHighlightDecl>(nullptr);
    }
    else if (llvm::isa<DisplayableGlobalVariable>(Prev)) {
      Notifier->createNotify<ConEvHighlightDecl>(nullptr);
    }
    else if (auto const DA = llvm::dyn_cast<DisplayableReferencedArea>(Prev)) {
      if (auto Access = CurrentAccess->getAccess()) {
        auto const Start = DA->getAreaStart();
        auto const MMalloc = CurrentProcess->getDynamicMemoryAllocation(Start);
        if (MMalloc.assigned<seec::cm::MallocState>()) {
          auto const &Malloc = MMalloc.get<seec::cm::MallocState>();
          if (Malloc.getAllocatorStmt())
            Notifier->createNotify<ConEvHighlightStmt>(nullptr);
        }
      }
    }
  }

  MouseOver = Ev.getDisplayableShared();

  auto const Node = MouseOver.get();

  if (!Node) {
    if (Recording)
      Recording->recordEventL("StateGraphViewer.MouseOverNone");
  }
  else if (auto const DV = llvm::dyn_cast<DisplayableValue>(Node)) {
    if (auto Access = CurrentAccess->getAccess()) {
      Notifier->createNotify<ConEvHighlightValue>(&(DV->getValue()),
                                                  CurrentAccess);
    }

    if (Recording) {
      std::vector<std::unique_ptr<IAttributeReadOnly>> Attrs;
      addAttributesForValue(Attrs, DV->getValue());
      Recording->recordEventV("StateGraphViewer.MouseOverValue", Attrs);
    }
  }
  else if (auto const DD = llvm::dyn_cast<DisplayableDereference>(Node)) {
    if (auto Access = CurrentAccess->getAccess()) {
      Notifier->createNotify<ConEvHighlightValue>(&(DD->getPointer()),
                                                  CurrentAccess);
    }

    if (Recording) {
      std::vector<std::unique_ptr<IAttributeReadOnly>> Attrs;
      addAttributesForValue(Attrs, DD->getPointer());
      Recording->recordEventV("StateGraphViewer.MouseOverDereference", Attrs);
    }
  }
  else if (auto const DF = llvm::dyn_cast<DisplayableFunctionState>(Node)) {
    auto const Decl = DF->getFunctionState().getFunctionDecl();
    Notifier->createNotify<ConEvHighlightDecl>(Decl);

    if (Recording) {
      auto const &FS = DF->getFunctionState();
      Recording->recordEventL("StateGraphViewer.MouseOverFunctionState",
                              make_attribute("function", FS.getNameAsString()));
    }
  }
  else if (auto const DL = llvm::dyn_cast<DisplayableLocalState>(Node)) {
    auto const Decl = DL->getLocalState().getDecl();
    Notifier->createNotify<ConEvHighlightDecl>(Decl);

    if (Recording) {
      // TODO
    }
  }
  else if (auto const DP = llvm::dyn_cast<DisplayableParamState>(Node)) {
    auto const Decl = DP->getParamState().getDecl();
    Notifier->createNotify<ConEvHighlightDecl>(Decl);

    if (Recording) {
      // TODO
    }
  }
  else if (auto const DG = llvm::dyn_cast<DisplayableGlobalVariable>(Node)) {
    auto const Decl = DG->getGlobalVariable().getClangValueDecl();
    Notifier->createNotify<ConEvHighlightDecl>(Decl);

    if (Recording) {
      // TODO
    }
  }
  else if (auto const DA = llvm::dyn_cast<DisplayableReferencedArea>(Node)) {
    if (auto Access = CurrentAccess->getAccess()) {
      auto const Start = DA->getAreaStart();
      auto const MayMalloc = CurrentProcess->getDynamicMemoryAllocation(Start);
      if (MayMalloc.assigned<seec::cm::MallocState>()) {
        auto const &Malloc = MayMalloc.get<seec::cm::MallocState>();
        if (auto const S = Malloc.getAllocatorStmt())
          Notifier->createNotify<ConEvHighlightStmt>(S);
      }
    }

    if (Recording) {
      Recording->recordEventL("StateGraphViewer.MouseOverReferencedArea",
                              make_attribute("start", DA->getAreaStart()),
                              make_attribute("end", DA->getAreaEnd()));
    }
  }
  else {
    wxLogDebug("Mouse over unknown Displayable.");

    if (Recording) {
      Recording->recordEventL("StateGraphViewer.MouseOverUnknown");
    }
  }
}

void StateGraphViewerPanel::OnContextMenu(wxContextMenuEvent &Ev)
{
  if (!MouseOver)
    return;
  
  UErrorCode Status = U_ZERO_ERROR;
  auto const TextTable = seec::getResource("TraceViewer",
                                           getLocale(),
                                           Status,
                                           "StateGraphViewer");
  if (U_FAILURE(Status)) {
    wxLogDebug("Couldn't get StateGraphViewer resources.");
    return;
  }
  
  auto const TheNode = MouseOver.get();
  
  if (auto const DV = llvm::dyn_cast<DisplayableValue>(TheNode)) {
    auto const ValuePtr = &(DV->getValue());
    
    wxMenu CM{};
    
    addValueNavigation(*this, CurrentAccess, CM, *ValuePtr, Recording);
    
    // Allow the user to select the Value's layout engine.
    std::unique_lock<std::mutex> LockLayoutHandler(LayoutHandlerMutex);
    auto const Engines = LayoutHandler->listLayoutEnginesSupporting(*ValuePtr);
    LockLayoutHandler.unlock();
    
    if (Engines.size() > 1) {
      auto SM = seec::makeUnique<wxMenu>();
      
      for (auto const E : Engines) {
        auto const LazyName = E->getName();
        if (!LazyName)
          continue;
        
        UErrorCode Status = U_ZERO_ERROR;
        auto const Name = LazyName->get(Status, getLocale());
        if (U_FAILURE(Status))
          continue;
        
        std::string UTF8Name;
        Name.toUTF8String(UTF8Name);
        
        auto const EngineID = reinterpret_cast<uintptr_t>(E);
        
        BindMenuItem(
          SM->Append(wxID_ANY, wxString{UTF8Name}),
          [=] (wxEvent &Ev) -> void {
            {
              std::lock_guard<std::mutex> LLH(this->LayoutHandlerMutex);
              this->LayoutHandler->setLayoutEngine(*ValuePtr, EngineID);
            }
            this->renderGraph();
          });
      }
      
      CM.AppendSubMenu(SM.release(),
                       seec::getwxStringExOrEmpty(TextTable,
                                                  "CMValueDisplayAs"));
    }
    
    PopupMenu(&CM);
  }
  else if (auto const DD = llvm::dyn_cast<DisplayableDereference>(TheNode)) {
    auto const ValOfPtr = &(DD->getPointer());
    
    wxMenu CM{};
    
    BindMenuItem(
      CM.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "CMDereferenceUse")),
      [=] (wxEvent &Ev) -> void {
        {
          std::lock_guard<std::mutex> LLH(this->LayoutHandlerMutex);
          this->LayoutHandler->setAreaReference(*ValOfPtr);
        }
        this->renderGraph();
      });
    
    PopupMenu(&CM);
  }
  else if (auto const DF = llvm::dyn_cast<DisplayableFunctionState>(TheNode)) {
    wxMenu CM{};
    
    auto const FnPtr = &(DF->getFunctionState());
    
    BindMenuItem(
      CM.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable,
                                           "CMFunctionRewindEntry")),
      [=] (wxEvent &Ev) -> void {
        raiseMovementEvent(*this, this->CurrentAccess,
          [=] (seec::cm::ProcessState &State) {
            return seec::cm::moveToFunctionEntry(*FnPtr);
          });
      });
    
    BindMenuItem(
      CM.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable,
                                           "CMFunctionForwardExit")),
      [=] (wxEvent &Ev) -> void {
        raiseMovementEvent(*this, this->CurrentAccess,
          [=] (seec::cm::ProcessState &State) {
            return seec::cm::moveToFunctionFinished(*FnPtr);
          });
      });
    
    PopupMenu(&CM);
  }
  else if (auto const DA = llvm::dyn_cast<DisplayableReferencedArea>(TheNode)) {
    auto const Area = seec::MemoryArea(DA->getAreaStart(), DA->getAreaEnd());
    auto const ValOfPtr = &(DA->getPointer());
    
    wxMenu CM{};
    
    // Allow the user to select the Area's layout engine.
    std::unique_lock<std::mutex> LLH(LayoutHandlerMutex);
    auto const Engines = LayoutHandler->listLayoutEnginesSupporting(Area,
                                                                    *ValOfPtr);
    LLH.unlock();
    
    if (Engines.size() > 1) {
      auto SM = seec::makeUnique<wxMenu>();
      
      for (auto const E : Engines) {
        auto const LazyName = E->getName();
        if (!LazyName)
          continue;
        
        UErrorCode Status = U_ZERO_ERROR;
        auto const Name = LazyName->get(Status, getLocale());
        if (U_FAILURE(Status))
          continue;
        
        std::string UTF8Name;
        Name.toUTF8String(UTF8Name);
        
        auto const EngineID = reinterpret_cast<uintptr_t>(E);
        
        BindMenuItem(
          SM->Append(wxID_ANY, wxString{UTF8Name}),
          [=] (wxEvent &Ev) -> void {
            {
              std::lock_guard<std::mutex> LLH(this->LayoutHandlerMutex);
              this->LayoutHandler->setLayoutEngine(Area, *ValOfPtr, EngineID);
            }
            this->renderGraph();
          });
      }
      
      CM.AppendSubMenu(SM.release(),
                       seec::getwxStringExOrEmpty(TextTable,
                                                  "CMAreaDisplayAs"));
    }
    
    PopupMenu(&CM);
  }
  else {
    wxLogDebug("Unknown Displayable!");
  }
}

void StateGraphViewerPanel::renderGraph()
{
  if (!WebView || PathToDot.empty())
    return;

  WebView->RunScript(wxString{"ClearState();"});

  // Send the rendering task to the worker thread.
  std::unique_lock<std::mutex> Lock{TaskMutex};
  TaskAccess = CurrentAccess;
  TaskProcess = CurrentProcess;
  ContinueGraphGeneration = true;
  Lock.unlock();
  TaskCV.notify_one();
}

void
StateGraphViewerPanel::show(std::shared_ptr<StateAccessToken> Access,
                            seec::cm::ProcessState const &Process,
                            seec::cm::ThreadState const &Thread)
{
  CurrentAccess = std::move(Access);
  CurrentProcess = &Process;
  MouseOver.reset();
  
  WebView->RunScript(wxString("InvalidateState();"));
  
  if (!WebView || PathToDot.empty())
    return;
  
  renderGraph();

  // Add special highlighting for values associated with the active Stmt.
  //
  if (!Thread.getCallStack().empty()) {
    seec::cm::FunctionState const &Fn = Thread.getCallStack().back();
    if (auto const Stmt = Fn.getActiveStmt()) {
      if (auto const Value = Fn.getStmtValue(Stmt)) {
        wxString Script("MarkActiveStmtValue(");
        Script << reinterpret_cast<uintptr_t>(Value.get()) << ");";

        if (auto const Ptr = llvm::dyn_cast<seec::cm::ValueOfPointer>
                                           (Value.get()))
        {
          if (Ptr->getDereferenceIndexLimit()) {
            if (auto const Pointee = Ptr->getDereferenced(0)) {
              Script << "MarkActiveStmtValue("
                     << reinterpret_cast<uintptr_t>(Pointee.get()) << ");";
            }
          }
        }

        WebView->RunScript(Script);
      }
    }
  }
}

void StateGraphViewerPanel::clear()
{
  // If the graph generation is still running, then terminate it now.
  ContinueGraphGeneration = false;

  // Clear any existing graph from the WebView.
  if (WebView && !PathToDot.empty())
    WebView->RunScript(wxString{"ClearState();"});

  MouseOver.reset();
}

void StateGraphViewerPanel::OnMouseOver(std::string const &NodeID)
{
  auto const Unescaped = wxURI::Unescape(NodeID).ToStdString();
  auto const SpacePos = Unescaped.find(' ');
  auto const NodeType = Unescaped.substr(0, SpacePos);
  
  std::shared_ptr<Displayable const> NodeDisplayable;

  if (NodeType == "value") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const ID = seec::callbackfs::ParseImpl<uintptr_t>::impl(NodeData);
    auto const &Value = *reinterpret_cast<seec::cm::Value const *>(ID);
    NodeDisplayable = std::make_shared<DisplayableValue>(Value);
  }
  else if (NodeType == "dereference") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const ID = seec::callbackfs::ParseImpl<uintptr_t>::impl(NodeData);
    auto const &Ptr = *reinterpret_cast<seec::cm::ValueOfPointer const *>(ID);
    NodeDisplayable = std::make_shared<DisplayableDereference>(Ptr);
  }
  else if (NodeType == "function") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const ID = seec::callbackfs::ParseImpl<uintptr_t>::impl(NodeData);
    auto &Fn = *reinterpret_cast<seec::cm::FunctionState *>(ID);
    NodeDisplayable = std::make_shared<DisplayableFunctionState>(Fn);
  }
  else if (NodeType == "local") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const ID = seec::callbackfs::ParseImpl<uintptr_t>::impl(NodeData);
    auto const &Local = *reinterpret_cast<seec::cm::LocalState const *>(ID);
    NodeDisplayable = std::make_shared<DisplayableLocalState>(Local);
  }
  else if (NodeType == "param") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const ID = seec::callbackfs::ParseImpl<uintptr_t>::impl(NodeData);
    auto const &Param = *reinterpret_cast<seec::cm::ParamState const *>(ID);
    NodeDisplayable = std::make_shared<DisplayableParamState>(Param);
  }
  else if (NodeType == "global") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const ID = seec::callbackfs::ParseImpl<uintptr_t>::impl(NodeData);
    auto const &GV = *reinterpret_cast<seec::cm::GlobalVariable const *>(ID);
    NodeDisplayable = std::make_shared<DisplayableGlobalVariable>(GV);
  }
  else if (NodeType == "area") {
    auto const NodeData = Unescaped.substr(SpacePos + 1);
    auto const Comma1 = NodeData.find(',');
    if (Comma1 == std::string::npos) {
      wxLogDebug("Bad area node data: %s", wxString{NodeData});
      return;
    }
    
    auto const Comma2 = NodeData.find(',', Comma1 + 1);
    if (Comma2 == std::string::npos) {
      wxLogDebug("Bad area node data: %s", wxString{NodeData});
      return;
    }
    
    auto const StrStart = NodeData.substr(0,          Comma1);
    auto const StrEnd   = NodeData.substr(Comma1 + 1, Comma2 - Comma1);
    auto const StrID    = NodeData.substr(Comma2 + 1);
    
    auto const Start = seec::callbackfs::ParseImpl<uint64_t>::impl(StrStart);
    auto const End   = seec::callbackfs::ParseImpl<uint64_t>::impl(StrEnd);
    auto const ID    = seec::callbackfs::ParseImpl<uintptr_t>::impl(StrID);
    auto const &Ptr  = *reinterpret_cast<seec::cm::ValueOfPointer const *>(ID);
    
    NodeDisplayable = std::make_shared<DisplayableReferencedArea>
                                      (Start, End, Ptr);
  }
  else if (NodeType != "null"){
    wxLogDebug("Bad node: %s", wxString{Unescaped});
    return;
  }
  
  // If the node was Displayable, push the event to the GUI thread.
  MouseOverDisplayableEvent Ev {
    SEEC_EV_MOUSE_OVER_DISPLAYABLE,
    this->GetId(),
    std::move(NodeDisplayable)
  };
  
  Ev.SetEventObject(this);
    
  this->GetEventHandler()->AddPendingEvent(Ev);
}

void StateGraphViewerPanel::RaiseContextMenu()
{
  wxContextMenuEvent Ev {
    wxEVT_CONTEXT_MENU,
    this->GetId(),
    wxGetMousePosition()
  };
  
  Ev.SetEventObject(this);
  
  this->GetEventHandler()->AddPendingEvent(Ev);
}
