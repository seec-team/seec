#include "seec/Trace/ThreadState.hpp"

#include "FunctionStateViewer.hpp"
#include "ThreadStateViewer.hpp"


//------------------------------------------------------------------------------
// ThreadStateViewerPanel
//------------------------------------------------------------------------------

ThreadStateViewerPanel::~ThreadStateViewerPanel() {}

bool ThreadStateViewerPanel::Create(wxWindow *Parent,
                                    OpenTrace const &TheTrace,
                                    wxWindowID ID,
                                    wxPoint const &Position,
                                    wxSize const &Size) {
  if (!wxScrolledWindow::Create(Parent, ID, Position, Size))
    return false;

  Sizer = new wxBoxSizer(wxVERTICAL);
  SetSizer(Sizer);
  
  Trace = &TheTrace;
  
  return true;
}

void ThreadStateViewerPanel::showState(seec::trace::ThreadState const &State) {
  auto &CallStack = State.getCallStack();
  
  if (CallStack.size() > FunctionViewers.size()) {
    // Add new function viewers.
    for (std::size_t i = FunctionViewers.size(); i < CallStack.size(); ++i) {
      auto FunctionViewer = new FunctionStateViewerPanel(this, *Trace);
      
      FunctionViewers.push_back(FunctionViewer);
      
      Sizer->Add(FunctionViewer, wxSizerFlags().Proportion(0).Expand());
    }
    
    Sizer->Layout();
  }
  else if (CallStack.size() < FunctionViewers.size()) {
    // Delete excess function viewers.
    for (std::size_t i = FunctionViewers.size(); i > CallStack.size(); --i) {
      auto FunctionViewer = FunctionViewers.back();
      
      Sizer->Detach(FunctionViewer);
      FunctionViewer->Destroy();
      
      FunctionViewers.pop_back();
    }
    
    Sizer->Layout();
  }
  
  // Set the state of all function viewers.
  for (std::size_t i = 0; i < CallStack.size(); ++i) {
    FunctionViewers[i]->showState(CallStack[i]);
  }
}



#if 0

  if (ThreadStateModels.size() < Threads.size()) {
    // Get the GUIText from the TraceViewer ICU resources.
    UErrorCode Status = U_ZERO_ERROR;
    auto TextTable = seec::getResource("TraceViewer",
                                       Locale::getDefault(),
                                       Status,
                                       "GUIText");
    assert(U_SUCCESS(Status));
  
    for (std::size_t i = ThreadStateModels.size(); i < Threads.size(); ++i) {
      // Create a new model and view for this thread.
      auto Model = new ThreadStateTreeModel();
      
      auto View = new wxDataViewCtrl(this, wxID_ANY);
      View->AssociateModel(Model);
      
      Model->DecRef();
      
      // Column 0 of the state tree (call stack).
      auto Renderer0 = new wxDataViewTextRenderer("string",
                                                  wxDATAVIEW_CELL_INERT);
      auto ColumnTitle = seec::getwxStringExOrEmpty(TextTable,
                                                    "CallTree_Column0Title");
      auto Column0 = new wxDataViewColumn(std::move(ColumnTitle),
                                          Renderer0,
                                          0,
                                          200,
                                          wxALIGN_LEFT,
                                          wxDATAVIEW_COL_RESIZABLE);
      View->AppendColumn(Column0);
      
      // Add the view to the thread book.
      int64_t ThreadID = Threads[i]->getTrace().getThreadID();
      auto ThreadStr = TextTable.getStringEx("ThreadNumber", Status);
      auto Formatted = seec::format(ThreadStr, Status, ThreadID);
      
      ThreadBook->AddPage(View, seec::towxString(Formatted));
      
      // Keep track of the existing models and views.
      ThreadStateModels.push_back(Model);
      ThreadStateViews.push_back(View);
    }
  }

//------------------------------------------------------------------------------
// ThreadStateTreeModel
//------------------------------------------------------------------------------

enum class StateNodeType {
  Function
};

/// \brief Base class for a node in a thread's state tree.
///
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

/// \brief Node for a single function call in a thread's state tree.
///
class StateNodeFunction : public StateNodeBase {
  std::size_t Depth;

public:
  /// \param CallDepth Zero-based depth into the call stack.
  StateNodeFunction(StateNodeBase *Parent, std::size_t CallDepth)
  : StateNodeBase(StateNodeType::Function, Parent),
    Depth(CallDepth)
  {}

  virtual ~StateNodeFunction();

  std::size_t getCallDepth() const { return Depth; }
  
  static bool classof(StateNodeFunction const *Node) { return true; }

  static bool classof(StateNodeBase const *Node) {
    return Node->getType() == StateNodeType::Function;
  }
};

StateNodeFunction::~StateNodeFunction() = default;

/// \brief Tree data model for a single ThreadState.
///
class ThreadStateTreeModel : public wxDataViewModel
{
  /// Information about the trace that this state belongs to.
  OpenTrace *Trace;

  /// The thread state is the root of the tree.
  seec::trace::ThreadState *Root;

  /// Nodes for each function state.
  std::vector<std::unique_ptr<StateNodeFunction>> FunctionNodes;
  
public:
  ThreadStateTreeModel()
  : Trace(nullptr),
    Root(nullptr),
    FunctionNodes()
  {}

  virtual ~ThreadStateTreeModel() {}

  void updateThread() {
    auto &CallStack = Root->getCallStack();
    
    if (FunctionNodes.size() > CallStack.size()) {
      // Delete excess function nodes.
      while (FunctionNodes.size() > CallStack.size()) {
        auto Node = FunctionNodes.back().get();
        auto Parent = Node->getParent();
        
        FunctionNodes.pop_back();
        
        ItemDeleted(wxDataViewItem(reinterpret_cast<void *>(Parent)),
                    wxDataViewItem(reinterpret_cast<void *>(Node)));
      }
    }
    else if (FunctionNodes.size() < CallStack.size()) {
      // Add new function nodes.
      StateNodeFunction *Parent = FunctionNodes.empty()
                                  ? nullptr : FunctionNodes.back().get();
      
      for (std::size_t i = FunctionNodes.size(); i < CallStack.size(); ++i) {
        FunctionNodes.emplace_back(new StateNodeFunction(Parent, i));
        
        auto NewNode = FunctionNodes.back().get();
      
        ItemAdded(wxDataViewItem(reinterpret_cast<void *>(Parent)),
                  wxDataViewItem(reinterpret_cast<void *>(NewNode)));
      
        Parent = NewNode;
      }
    }
  }

  /// Set a new thread state and notify any associated controls.
  void setRoot(OpenTrace &NewTrace,
               seec::trace::ThreadState &NewRoot) {
    if (Root == &NewRoot) {
      updateThread();
      return;
    }

    // Destroy old nodes.
    FunctionNodes.clear();
    
    // Notify the controllers of changes.
    Cleared();

    // Setup the new information.
    Trace = &NewTrace;
    Root = &NewRoot;
    
    // Create function nodes (if any).
    StateNodeBase *LastAdded = nullptr;
    std::size_t CallDepth = Root->getCallStack().size();
    
    for (std::size_t i = 0; i < CallDepth; ++i) {
      FunctionNodes.emplace_back(new StateNodeFunction(LastAdded, i));
      
      auto NewNode = FunctionNodes.back().get();
      
      ItemAdded(wxDataViewItem(reinterpret_cast<void *>(LastAdded)),
                wxDataViewItem(reinterpret_cast<void *>(NewNode)));
      
      LastAdded = NewNode;
    }
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
    return wxString("string");
  }

  virtual void GetValue(wxVariant &Variant,
                        wxDataViewItem const &Item,
                        unsigned int Column) const {
    // If there's no tree associated with this model, return an empty value.
    if (!Root) {
      Variant = wxEmptyString;
      return;
    }

    wxASSERT(Item.IsOk());

    auto NodeBase = reinterpret_cast<StateNodeBase *>(Item.GetID());
    
    // Get the GUIText from the TraceViewer ICU resources.
    UErrorCode Status = U_ZERO_ERROR;
    auto TextTable = seec::getResource("TraceViewer",
                                       Locale::getDefault(),
                                       Status,
                                       "GUIText");
    assert(U_SUCCESS(Status));

    if (auto FunctionNode = llvm::dyn_cast<StateNodeFunction>(NodeBase)) {
      auto &FunctionState = Root->getCallStack()[FunctionNode->getCallDepth()];
      
      switch (Column) {
        case 0:
        {
          auto Index = FunctionState.getTrace().getIndex();
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
          Variant = wxEmptyString;
          return;
      }
    }

    wxLogError("ThreadStateTreeModel::GetValue() with unknown item type.");
    Variant = wxEmptyString;
  }

  virtual bool SetValue(wxVariant const &WXUNUSED(Variant),
                        wxDataViewItem const &WXUNUSED(Item),
                        unsigned int WXUNUSED(Column)) {
    return false;
  }

  virtual wxDataViewItem GetParent(wxDataViewItem const &Item) const {
    auto NodeBase = reinterpret_cast<StateNodeBase *>(Item.GetID());
    if (!NodeBase)
      return wxDataViewItem(nullptr);

    return wxDataViewItem(reinterpret_cast<void *>(NodeBase->getParent()));
  }

  virtual bool IsContainer(wxDataViewItem const &Item) const {
    auto NodeBase = reinterpret_cast<StateNodeBase *>(Item.GetID());
    if (!NodeBase)
      return true;

    if (llvm::isa<StateNodeFunction>(NodeBase))
      return true;

    return false;
  }

  virtual unsigned int GetChildren(wxDataViewItem const &Parent,
                                   wxDataViewItemArray &Array) const {
    if (!Root) {
      return 0;
    }
    
    auto NodeBase = reinterpret_cast<StateNodeBase *>(Parent.GetID());
    if (!NodeBase) {
      if (!FunctionNodes.empty()) {
        auto NodePtr = FunctionNodes.front().get();
        Array.Add(wxDataViewItem(reinterpret_cast<void *>(NodePtr)));
        return 1;
      }
      
      return 0;
    }

    if (auto NodeFunc = llvm::dyn_cast<StateNodeFunction>(NodeBase)) {
      auto ChildIndex = NodeFunc->getCallDepth() + 1;
      
      if (ChildIndex < FunctionNodes.size()) {
        auto Node = FunctionNodes[ChildIndex].get();
        Array.Add(wxDataViewItem(reinterpret_cast<void *>(Node)));
        return 1;
      }
      
      return 0;
    }
    
    return 0;
  }
};


#endif