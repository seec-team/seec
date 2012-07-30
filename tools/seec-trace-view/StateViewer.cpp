#include "MallocViewer.hpp"
#include "StateViewer.hpp"
#include "OpenTrace.hpp"

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/dataview.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

//------------------------------------------------------------------------------
// StateTreeModel
//------------------------------------------------------------------------------

enum class StateNodeType {
  Process,
  Thread,
  Function,
  Alloca
};

class StateNodeBase {
  StateNodeType ThisType;

  StateNodeBase *Parent;

protected:
  StateNodeBase(StateNodeType Type, StateNodeBase *ParentNode = nullptr)
  : ThisType(Type),
    Parent(ParentNode)
  {}

public:
  virtual ~StateNodeBase();

  StateNodeType getType() const { return ThisType; }

  StateNodeBase *getParent() const { return Parent; }

  static bool classof(StateNodeBase const *Node) { return true; }
};

StateNodeBase::~StateNodeBase() = default;

class StateNodeProcess : public StateNodeBase {
  seec::trace::ProcessState &ThisState;

public:
  StateNodeProcess(seec::trace::ProcessState &State)
  : StateNodeBase(StateNodeType::Process),
    ThisState(State)
  {}

  virtual ~StateNodeProcess();

  seec::trace::ProcessState &getState() const { return ThisState; }

  static bool classof(StateNodeProcess const *Node) { return true; }

  static bool classof(StateNodeBase const *Node) {
    return Node->getType() == StateNodeType::Process;
  }
};

StateNodeProcess::~StateNodeProcess() = default;

class StateNodeThread : public StateNodeBase {
  seec::trace::ThreadState &ThisState;

public:
  StateNodeThread(StateNodeProcess *Parent,
                  seec::trace::ThreadState &State)
  : StateNodeBase(StateNodeType::Thread, Parent),
    ThisState(State)
  {}

  virtual ~StateNodeThread();

  seec::trace::ThreadState &getState() const { return ThisState; }

  static bool classof(StateNodeThread const *Node) { return true; }

  static bool classof(StateNodeBase const *Node) {
    return Node->getType() == StateNodeType::Thread;
  }
};

StateNodeThread::~StateNodeThread() = default;

class StateNodeFunction : public StateNodeBase {
  seec::trace::FunctionState const &ThisState;

public:
  StateNodeFunction(StateNodeBase *Parent,
                    seec::trace::FunctionState const &State)
  : StateNodeBase(StateNodeType::Function, Parent),
    ThisState(State)
  {}

  virtual ~StateNodeFunction();

  seec::trace::FunctionState const &getState() const { return ThisState; }

  static bool classof(StateNodeFunction const *Node) { return true; }

  static bool classof(StateNodeBase const *Node) {
    return Node->getType() == StateNodeType::Function;
  }
};

StateNodeFunction::~StateNodeFunction() = default;

class StateTreeModel : public wxDataViewModel
{
  /// Information about the trace that this state belongs to.
  OpenTrace *Trace;

  /// The process state is the root of the state tree.
  seec::trace::ProcessState *Root;

  /// Root node for the process state.
  std::unique_ptr<StateNodeProcess> RootNode;

  /// Nodes for each thread state.
  std::map<seec::trace::ThreadState *,
           std::unique_ptr<StateNodeThread>> ThreadNodes;

  /// Nodes for each thread's function states.
  std::map<seec::trace::ThreadState *,
           std::vector<std::unique_ptr<StateNodeFunction>>> FunctionNodes;

public:
  StateTreeModel()
  : Trace(nullptr),
    Root(nullptr),
    RootNode(),
    ThreadNodes(),
    FunctionNodes()
  {}

  void updateFunction(seec::trace::FunctionState &State) {

  }

  void updateThread(seec::trace::ThreadState &State) {
    auto ThreadNode = ThreadNodes[&State].get();

    // Make sure that we have the correct number of function nodes.
    auto &FuncNodes = FunctionNodes[&State];
    auto &CallStack = State.getCallStack();

    if (FuncNodes.size() > CallStack.size()) {
      // Delete excess function nodes.
      while (FuncNodes.size() > CallStack.size()) {
        auto &Node = FuncNodes.back();

        ItemDeleted(wxDataViewItem(reinterpret_cast<void *>(Node->getParent())),
                    wxDataViewItem(reinterpret_cast<void *>(Node.get())));

        FuncNodes.pop_back();
      }
    }

    auto ExistingSize = FuncNodes.size();

    if (FuncNodes.size() < CallStack.size()) {
      // Add new function nodes.
      StateNodeBase *Parent
        = FuncNodes.empty()
        ? static_cast<StateNodeBase *>(ThreadNode)
        : static_cast<StateNodeBase *>(FuncNodes.back().get());

      for (std::size_t i = FuncNodes.size(); i < CallStack.size(); ++i) {
        FuncNodes.emplace_back(new StateNodeFunction(Parent, CallStack[i]));

        auto Added = FuncNodes.back().get();

        ItemAdded(wxDataViewItem(reinterpret_cast<void *>(Parent)),
                  wxDataViewItem(reinterpret_cast<void *>(Added)));

        Parent = Added;
      }
    }

    // Update existing functions' nodes.
    for (auto i = 0u; i < ExistingSize; ++i) {
      ItemChanged(wxDataViewItem(reinterpret_cast<void *>(FuncNodes[i].get())));
    }

    // Update the thread item (it may have new children).
    ItemChanged(wxDataViewItem(reinterpret_cast<void *>(ThreadNode)));
  }

  void updateProcess() {
    // The root process item doesn't change, so we don't need to notify.

    // Update all threads individually.
    for (auto &ThreadStatePtr : Root->getThreadStates()) {
      updateThread(*ThreadStatePtr);
    }
  }

  /// Set a new process state and notify any associated controls.
  void setRoot(OpenTrace &NewTrace,
               seec::trace::ProcessState &NewRoot) {
    if (Root == &NewRoot) {
      updateProcess();
      return;
    }

    // Destroy old nodes.
    ThreadNodes.clear();
    FunctionNodes.clear();

    // Create new Root (Process) node.
    Trace = &NewTrace;
    Root = &NewRoot;
    RootNode.reset(new StateNodeProcess(NewRoot));

    // Create new thread nodes.
    wxDataViewItemArray ThreadItems;

    for (auto &ThreadState : NewRoot.getThreadStates()) {
      auto Node = new StateNodeThread(RootNode.get(), *ThreadState);

      ThreadNodes.insert(
                    std::make_pair(ThreadState.get(),
                                   std::unique_ptr<StateNodeThread>(Node)));

      ThreadItems.Add(wxDataViewItem(reinterpret_cast<void *>(Node)));

      auto CallStack = std::vector<std::unique_ptr<StateNodeFunction>>();
      FunctionNodes.insert(std::make_pair(ThreadState.get(),
                                          std::move(CallStack)));
    }

    // Notify the controllers of changes.
    Cleared();

    // Add the root node.
    ItemAdded(wxDataViewItem(),
              wxDataViewItem(reinterpret_cast<void *>(RootNode.get())));

    // Add all thread nodes.
    ItemsAdded(wxDataViewItem(reinterpret_cast<void *>(RootNode.get())),
               ThreadItems);
  }

  int Compare(wxDataViewItem const &Item1,
              wxDataViewItem const &Item2,
              unsigned int Column,
              bool Ascending) const {
    return &Item1 - &Item2;
  }

  virtual unsigned int GetColumnCount() const {
    return 1;
  }

  virtual wxString GetColumnType(unsigned int Column) const {
    switch (Column) {
      case 0:  return "string";
      default: return "string";
    }
  }

  virtual void GetValue(wxVariant &Variant,
                        wxDataViewItem const &Item,
                        unsigned int Column) const {
    if (!Root)
      return;

    auto NodeBase = reinterpret_cast<StateNodeBase *>(Item.GetID());
    if (!NodeBase)
      return;

    // Get the GUIText from the TraceViewer ICU resources.
    UErrorCode Status = U_ZERO_ERROR;
    auto TextTable = seec::getResource("TraceViewer",
                                       Locale::getDefault(),
                                       Status,
                                       "GUIText");
    assert(U_SUCCESS(Status));

    if (llvm::isa<StateNodeProcess>(NodeBase)) {
      switch (Column) {
        case 0:
          Variant = seec::getwxStringExOrEmpty(TextTable, "CallTree_Process");
          return;
        default:
          return;
      }
    }

    if (auto ThreadNode = llvm::dyn_cast<StateNodeThread>(NodeBase)) {
      switch (Column) {
        case 0:
        {
          int64_t ThreadID = ThreadNode->getState().getTrace().getThreadID();
          auto ThreadStr = TextTable.getStringEx("CallTree_Thread", Status);
          auto Formatted = seec::format(ThreadStr, Status, ThreadID);
          Variant = seec::towxString(Formatted);
          return;
        }
        default:
          return;
      }
    }

    if (auto FunctionNode = llvm::dyn_cast<StateNodeFunction>(NodeBase)) {
      switch (Column) {
        case 0:
        {
          auto Index = FunctionNode->getState().getTrace().getIndex();
          auto Function = Trace->getModuleIndex().getFunction(Index);
          if (!Function) {
            Variant = seec::getwxStringExOrEmpty(TextTable,
                                                 "CallTree_UnknownFunction");
            return;
          }

          auto Decl = Trace->getMappedModule().getDecl(Function);
          if (!Decl) {
            Variant = seec::getwxStringExOrEmpty(TextTable,
                                                 "CallTree_UnknownFunction");
            return;
          }

          auto NamedDecl = llvm::dyn_cast<clang::NamedDecl>(Decl);
          assert(NamedDecl);

          Variant = wxString(NamedDecl->getNameAsString());
          return;
        }
        default:
          return;
      }
    }
  }

  virtual bool SetValue(wxVariant const &Variant,
                        wxDataViewItem const &Item,
                        unsigned int Column) {
    return false;
  }

  virtual wxDataViewItem GetParent(wxDataViewItem const &Item) const {
    if (!Root)
      return wxDataViewItem(nullptr);

    auto NodeBase = reinterpret_cast<StateNodeBase *>(Item.GetID());
    if (!NodeBase)
      return wxDataViewItem(nullptr);

    return wxDataViewItem(reinterpret_cast<void *>(NodeBase->getParent()));
  }

  virtual bool IsContainer(wxDataViewItem const &Item) const {
    if (!Root)
      return false;

    auto NodeBase = reinterpret_cast<StateNodeBase *>(Item.GetID());
    if (!NodeBase)
      return false;

    if (llvm::isa<StateNodeProcess>(NodeBase))
      return true;

    if (llvm::isa<StateNodeThread>(NodeBase))
      return true;

    if (llvm::isa<StateNodeFunction>(NodeBase))
      return true;

    return false;
  }

  virtual unsigned int GetChildren(wxDataViewItem const &Parent,
                                   wxDataViewItemArray &Array) const {
    if (!Root)
      return 0;

    auto NodeBase = reinterpret_cast<StateNodeBase *>(Parent.GetID());
    if (!NodeBase) {
      Array.Add(wxDataViewItem(reinterpret_cast<void *>(RootNode.get())));
      return 1;
    }

    if (llvm::isa<StateNodeProcess>(NodeBase)) {
      unsigned int Count = 0;

      for (auto &Pair : ThreadNodes) {
        Array.Add(wxDataViewItem(reinterpret_cast<void *>(Pair.second.get())));
        ++Count;
      }

      return Count;
    }

    if (auto NodeThread = llvm::dyn_cast<StateNodeThread>(NodeBase)) {
      auto FuncIt = FunctionNodes.find(&(NodeThread->getState()));
      if (FuncIt == FunctionNodes.end())
        return 0;

      auto &Stack = FuncIt->second;
      if (Stack.empty())
        return 0;

      Array.Add(wxDataViewItem(reinterpret_cast<void *>(Stack.front().get())));
      return 1;
    }

    if (auto NodeFunc = llvm::dyn_cast<StateNodeFunction>(NodeBase)) {
      // First, find the thread that the function is in.
      auto InThread = NodeFunc->getParent();
      while (llvm::isa<StateNodeFunction>(InThread))
        InThread = InThread->getParent();

      // Get the call stack for this thread.
      auto NodeThread = llvm::dyn_cast<StateNodeThread>(InThread);
      assert(NodeThread);

      auto FuncIt = FunctionNodes.find(&(NodeThread->getState()));
      if (FuncIt == FunctionNodes.end())
        return 0;

      auto &Stack = FuncIt->second;

      // Find the function in the stack, and return the next function as its
      // child (if there is a next function).
      for (auto It = Stack.begin(), End = Stack.end(); It != End; ++It) {
        if (It->get() == NodeFunc) {
          if (++It != End) {
            Array.Add(wxDataViewItem(reinterpret_cast<void *>(It->get())));
            return 1;
          }

          break;
        }
      }

      return 0;
    }

    return 0;
  }
};


//------------------------------------------------------------------------------
// StateViewerPanel
//------------------------------------------------------------------------------

StateViewerPanel::~StateViewerPanel() {}

bool StateViewerPanel::Create(wxWindow *Parent,
                              wxWindowID ID,
                              wxPoint const &Position,
                              wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Create the state tree (call stack).
  StateTree = new StateTreeModel();
  DataViewCtrl = new wxDataViewCtrl(this, wxID_ANY);

  DataViewCtrl->AssociateModel(StateTree);

  // Column 0 of the state tree (call stack).
  auto Renderer0 = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
  auto ColumnTitle = seec::getwxStringExOrEmpty(TextTable,
                                                "CallTree_Column0Title");
  auto Column0 = new wxDataViewColumn(std::move(ColumnTitle),
                                      Renderer0,
                                      0,
                                      200,
                                      wxALIGN_LEFT,
                                      wxDATAVIEW_COL_RESIZABLE);
  DataViewCtrl->AppendColumn(Column0);

  // Create the notebook that holds other state views.
  StateBook = new wxAuiNotebook(this,
                                wxID_ANY,
                                wxDefaultPosition,
                                wxDefaultSize,
                                wxAUI_NB_TOP
                                | wxAUI_NB_TAB_SPLIT
                                | wxAUI_NB_TAB_MOVE
                                | wxAUI_NB_SCROLL_BUTTONS);

  // Create the MallocViewer and add it to the notebook.
  auto MallocViewer = new MallocViewerPanel(this);
  StateBook->AddPage(MallocViewer,
                     seec::getwxStringExOrEmpty(TextTable, "MallocView_Title"));

  // Use a sizer to layout the state tree and notebook.
  auto TopSizer = new wxGridSizer(1, // Rows
                                  2, // Cols
                                  wxSize(0,0) // Gap
                                  );
  TopSizer->Add(DataViewCtrl, wxSizerFlags().Expand());
  TopSizer->Add(StateBook, wxSizerFlags().Expand());
  SetSizerAndFit(TopSizer);

  return true;
}

void StateViewerPanel::show(OpenTrace &TraceInfo,
                            seec::trace::ProcessState &State) {
  StateTree->setRoot(TraceInfo, State);
}
