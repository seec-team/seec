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
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/ICU/Resources.hpp"
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
    "/usr/local/bin",
    // TODO: This is a temporary setting for the lab machines. Make a setting
    //       that can be overriden at compile time.
#if defined(__APPLE__)
    "/cslinux/adhoc/seec/bin"
#else
    "/cslinux/adhoc/seec/linux/bin"
#endif
  };
  
  llvm::SmallString<256> DotPath;
  
  for (auto const SearchPath : seec::range(SearchPaths)) {
    DotPath = SearchPath;
    llvm::sys::path::append(DotPath, DotName);
    
    if (!llvm::sys::fs::exists(DotPath.str()))
      continue;
    
    if (!llvm::sys::fs::can_execute(DotPath.str()))
      continue;
    
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
  if (!Lock || !TaskProcess)
    return std::string();

  std::lock_guard<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
  auto const Layout = LayoutHandler->doLayout(*TaskProcess);
  auto const GraphString = Layout.getDotString();

  auto const TimeMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (Layout.getTimeTaken());

  wxLogDebug("State graph generated in %" PRIu64 " ms.",
            static_cast<uint64_t>(TimeMS.count()));

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
    if (GraphString.empty())
      continue;

    // The remainder of the graph generation does not use the state, so we can
    // release access to the task information.
    Lock.unlock();

    auto const GVStart = std::chrono::steady_clock::now();

    // Write the graph to a temporary file.
    llvm::SmallString<256> GraphPath;

    {
      int GraphFD;
      auto const GraphErr =
        llvm::sys::fs::createUniqueFile("seecgraph-%%%%%%%%.dot",
                                        GraphFD,
                                        GraphPath);

      if (GraphErr != llvm::errc::success) {
        wxLogDebug("Couldn't create temporary dot file.");
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
      llvm::sys::fs::createUniqueFile("seecgraph-%%%%%%%%.svg", SVGPath);

    if (SVGErr != llvm::errc::success) {
      wxLogDebug("Couldn't create temporary svg file.");
      continue;
    }

    auto const RemoveSVG = seec::scopeExit([&] () {
                              bool Existed = false;
                              llvm::sys::fs::remove(SVGPath.str(), Existed);
                            });

    // Run dot using the temporary input/output files.
    char const *Args[] = {
      "dot",
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

    auto const GVEnd = std::chrono::steady_clock::now();
    auto const GVMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (GVEnd - GVStart);
    wxLogDebug("Graphviz completed in %" PRIu64 " ms",
              static_cast<uint64_t>(GVMS.count()));

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

StateGraphViewerPanel::StateGraphViewerPanel()
: wxPanel(),
  Notifier(nullptr),
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
  WebView(nullptr),
  LayoutHandler(),
  LayoutHandlerMutex(),
  CallbackFS(nullptr),
  MouseOver()
{}

StateGraphViewerPanel::StateGraphViewerPanel(wxWindow *Parent,
                                             ContextNotifier &WithNotifier,
                                             wxWindowID ID,
                                             wxPoint const &Position,
                                             wxSize const &Size)
: StateGraphViewerPanel()
{
  Create(Parent, WithNotifier, ID, Position, Size);
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
                                   wxWindowID ID,
                                   wxPoint const &Position,
                                   wxSize const &Size)
{
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;
  
  Notifier = &WithNotifier;
  
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
  
  Bind(wxEVT_CONTEXT_MENU,
       &StateGraphViewerPanel::OnContextMenu, this);
  Bind(SEEC_EV_MOUSE_OVER_DISPLAYABLE,
       &StateGraphViewerPanel::OnMouseOverDisplayable, this);

  wxFileSystem::AddHandler(CallbackFS);
  
  // Get our resources from ICU.
  UErrorCode Status = U_ZERO_ERROR;
  auto Resources = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
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
      switch (Ev.getKind()) {
        case ContextEventKind::HighlightValue:
        {
          // Send this value to the webpage.
          auto const &HighlightEv = llvm::cast<ConEvHighlightValue>(Ev);
          
          // We don't need to lock the highlight's access, as it must already
          // be locked by whoever raised the highlight event.
          if (CurrentAccess && CurrentAccess != HighlightEv.getAccess()) {
            wxLogDebug("Highlight state does not match graph's state.");
          }
          
          wxString Script("HighlightValue(");
          Script << reinterpret_cast<uintptr_t>(HighlightEv.getValue())
                 << ");";
          WebView->RunScript(Script);
          
          break;
        }
        default:
          break;
      }
    });
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
  MouseOver = Ev.getDisplayableShared();
}

static void BindMenuItem(wxMenuItem *Item,
                         std::function<void (wxEvent &)> Handler)
{
  if (!Item)
    return;
  
  auto const Menu = Item->GetMenu();
  if (!Menu)
    return;
  
  Menu->Bind(wxEVT_MENU, Handler, Item->GetId());
}

void StateGraphViewerPanel::OnContextMenu(wxContextMenuEvent &Ev)
{
  if (!MouseOver)
    return;
  
  UErrorCode Status = U_ZERO_ERROR;
  auto const TextTable = seec::getResource("TraceViewer",
                                           Locale::getDefault(),
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
    
    // Contextual movement based on the Value's memory.
    if (ValuePtr->isInMemory()) {
      auto const Size = ValuePtr->getTypeSizeInChars().getQuantity();
      auto const Area = seec::MemoryArea(ValuePtr->getAddress(), Size);
      
      BindMenuItem(
        CM.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "CMValueRewindAllocation")),
        [=] (wxEvent &Ev) -> void {
          raiseMovementEvent(*this, this->CurrentAccess,
            [=] (seec::cm::ProcessState &State) -> bool {
              return seec::cm::moveToAllocation(State, *ValuePtr);
            });
        });
      
      BindMenuItem(
        CM.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "CMValueRewindModification")),
        [=] (wxEvent &Ev) -> void {
          raiseMovementEvent(*this, this->CurrentAccess,
            [=] (seec::cm::ProcessState &State) -> bool {
              return seec::cm::moveBackwardUntilMemoryChanges(State, Area);
            });
        });
      
      BindMenuItem(
        CM.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "CMValueForwardModification")),
        [=] (wxEvent &Ev) -> void {
          raiseMovementEvent(*this, this->CurrentAccess,
            [=] (seec::cm::ProcessState &State) -> bool {
              return seec::cm::moveForwardUntilMemoryChanges(State, Area);
            });
        });
      
      BindMenuItem(
        CM.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "CMValueForwardDeallocation")),
        [=] (wxEvent &Ev) -> void {
          raiseMovementEvent(*this, this->CurrentAccess,
            [=] (seec::cm::ProcessState &State) -> bool {
              return seec::cm::moveToDeallocation(State, *ValuePtr);
            });
        });
    }
    
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
        auto const Name = LazyName->get(Status, Locale());
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
          [=] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveToFunctionEntry(*FnPtr);
          });
      });
    
    BindMenuItem(
      CM.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable,
                                           "CMFunctionForwardExit")),
      [=] (wxEvent &Ev) -> void {
        raiseMovementEvent(*this, this->CurrentAccess,
          [=] (seec::cm::ProcessState &State) -> bool {
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
        auto const Name = LazyName->get(Status, Locale());
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

  if (WebView && !PathToDot.empty())
    WebView->RunScript(wxString{"ClearState();"});

  // Send the rendering task to the worker thread.
  std::unique_lock<std::mutex> Lock{TaskMutex};
  TaskAccess = CurrentAccess;
  TaskProcess = CurrentProcess;
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
}

void StateGraphViewerPanel::clear()
{
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
