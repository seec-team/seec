//===- tools/seec-trace-view/StateEvaluationTree.cpp ----------------------===//
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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Clang/SubRangeRecorder.hpp"
#include "seec/ClangEPV/ClangEPV.hpp"
#include "seec/Util/MakeFunction.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "llvm/Support/raw_ostream.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/bitmap.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "ColourSchemeSettings.hpp"
#include "CommonMenus.hpp"
#include "NotifyContext.hpp"
#include "RuntimeValueLookup.hpp"
#include "StateAccessToken.hpp"
#include "StateEvaluationTree.hpp"
#include "StmtTooltip.hpp"
#include "TraceViewerApp.hpp"
#include "ValueFormat.hpp"

#include <stack>
#include <string>


void CentreOnPoint(wxScrollHelperBase &Scrolled,
                   wxSize const &TargetSize,
                   wxPoint const &Point)
{
  // Calculate the offset required to centre on Point.
  auto const OffsetH = std::max(0, Point.x - (TargetSize.GetWidth()  / 2));
  auto const OffsetV = std::max(0, Point.y - (TargetSize.GetHeight() / 2));
  
  // Calculate the offset in "scroll units".
  int PixelsPerUnitH = 0, PixelsPerUnitV = 0;
  Scrolled.GetScrollPixelsPerUnit(&PixelsPerUnitH, &PixelsPerUnitV);
  
  Scrolled.Scroll(OffsetH / PixelsPerUnitH, OffsetV / PixelsPerUnitV);
}

void CentreOnArea(wxScrollHelperBase &Scrolled,
                  wxSize const &TargetSize,
                  wxRect const &Area)
{
  CentreOnPoint(Scrolled,
                TargetSize,
                wxPoint(Area.GetX() + (Area.GetWidth()  / 2),
                        Area.GetY() + (Area.GetHeight() / 2)));
}

//------------------------------------------------------------------------------
// StateEvaluationTree
//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(StateEvaluationTreePanel, wxScrolled<wxPanel>)
  EVT_PAINT(StateEvaluationTreePanel::OnPaint)
  EVT_MOTION(StateEvaluationTreePanel::OnMouseMoved)
  EVT_LEAVE_WINDOW(StateEvaluationTreePanel::OnMouseLeftWindow)
  EVT_RIGHT_DOWN(StateEvaluationTreePanel::OnMouseRightDown)
  EVT_RIGHT_UP(StateEvaluationTreePanel::OnMouseRightUp)
END_EVENT_TABLE()


StateEvaluationTreePanel::DisplaySettings::DisplaySettings()
: PageBorderHorizontal(1.0),
  PageBorderVertical(1.0),
  NodeBorderVertical(0.5),
  CodeFontSize(12),
  PenWidth(1),
  m_ColourScheme(nullptr)
{}

void StateEvaluationTreePanel::setupColourScheme(ColourScheme const &Scheme)
{
  CodeFont = Scheme.getDefault().GetFont();
  Settings.CodeFontSize = Scheme.getDefault().GetFont().GetPointSize();
  Settings.m_ColourScheme = &Scheme;
}

void StateEvaluationTreePanel::drawIndicatorAtArea(wxDC &DC,
                                                   IndicatorStyle const &Style,
                                                   wxCoord X, wxCoord Y,
                                                   wxCoord W, wxCoord H)
{
  auto const Kind = Style.GetKind();
  auto const FG   = Style.GetForeground();
  
  wxPen const PrevPen = DC.GetPen();
  wxBrush const PrevBrush = DC.GetBrush();
  
  switch (Kind) {
    case IndicatorStyle::EKind::Plain:
      DC.SetPen(wxPen{FG, Settings.PenWidth});
      DC.DrawLine(X, Y+H, X+W, Y+H);
      break;
    case IndicatorStyle::EKind::Box:
      DC.SetPen(wxPen{FG, Settings.PenWidth});
      DC.DrawRectangle(X, Y, W, H);
      break;
    case IndicatorStyle::EKind::StraightBox:
      // Many DCs don't support alpha at all, so manually calculate an alpha
      // against the background colour.
      auto const BG = PrevBrush.GetColour();
      double const Alpha        = (double)Style.GetAlpha() / 255;
      double const OutlineAlpha = (double)Style.GetOutlineAlpha() / 255;
      DC.SetPen(wxPen{
        wxColour(wxColour::AlphaBlend(FG.Red(), BG.Red(), OutlineAlpha),
                 wxColour::AlphaBlend(FG.Green(), BG.Green(), OutlineAlpha),
                 wxColour::AlphaBlend(FG.Blue(), BG.Blue(), OutlineAlpha)),
        Settings.PenWidth});
      DC.SetBrush(wxBrush{
        wxColour(wxColour::AlphaBlend(FG.Red(), BG.Red(), Alpha),
                 wxColour::AlphaBlend(FG.Green(), BG.Green(), Alpha),
                 wxColour::AlphaBlend(FG.Blue(), BG.Blue(), Alpha))});
      DC.DrawRectangle(X, Y, W, H);
      break;
  }
  
  DC.SetPen(PrevPen);
  DC.SetBrush(PrevBrush);
}

void StateEvaluationTreePanel::drawNode(wxDC &DC,
                                        ColourScheme const &Scheme,
                                        NodeInfo const &Node,
                                        NodeDecoration const Decoration)
{
  auto const CharWidth  = DC.GetCharWidth();
  auto const CharHeight = DC.GetCharHeight();
  
  wxCoord const PageBorderV = CharHeight * Settings.PageBorderVertical;
  
  // Determine the indicator (if any).
  IndicatorStyle const *Indicator = nullptr;
  switch (Decoration) {
    case NodeDecoration::None:
      break;
    case NodeDecoration::Active:
      Indicator = &(Scheme.getActiveCode());
      break;
    case NodeDecoration::Highlighted:
      Indicator = &(Scheme.getHighlightCode());
      break;
  }
  
  // Set the background colour.
  DC.SetPen(wxPen{Scheme.getDefault().GetForeground(), Settings.PenWidth});
  DC.SetBrush(wxBrush{Scheme.getDefault().GetBackground()});
  DC.SetTextForeground(Scheme.getDefault().GetForeground());
  
  // Also highlight this node's area in the pretty-printed Stmt.
  if (Indicator) {
    drawIndicatorAtArea(DC, *Indicator,
                        Node.XStart, PageBorderV,
                        Node.XEnd - Node.XStart, CharHeight);
  }
  
  // Draw the background.
  wxPen const PrevPen = DC.GetPen();
  DC.SetPen(wxPen{DC.GetBrush().GetColour()});
  DC.DrawRectangle(Node.XStart, Node.YStart,
                   Node.XEnd - Node.XStart, Node.YEnd - Node.YStart);
  DC.SetPen(PrevPen);

  // Draw the line over the node.
  DC.DrawLine(Node.XStart, Node.YStart, Node.XEnd + 1, Node.YStart);
  
  // Draw the base indicator on the node (if any).
  if (Indicator) {
    drawIndicatorAtArea(DC, *Indicator,
                        Node.XStart, Node.YStart,
                        Node.XEnd - Node.XStart, Node.YEnd - Node.YStart);
  }
  
  // Draw the error indicator if the node has an error.
  if (Node.Error == NodeError::Error) {
    drawIndicatorAtArea(DC, Scheme.getErrorCode(),
                        Node.XStart, Node.YStart,
                        Node.XEnd - Node.XStart, Node.YEnd - Node.YStart);
  }

  // Draw the node's value string.
  if (Node.Value) {
    auto const ValText = Node.ValueStringShort;
    auto const TextWidth = CharWidth * ValText.size();
    auto const NodeWidth = CharWidth * Node.RangeLength;
    auto const Offset = (NodeWidth - TextWidth) / 2;
    DC.DrawText(ValText, Node.XStart + Offset, Node.YStart);
  }
}

void StateEvaluationTreePanel::render(wxDC &dc)
{
  if (Settings.m_ColourScheme == nullptr)
    return;
  
  PrepareDC(dc);
  auto const &Scheme = *(Settings.m_ColourScheme);
  
  dc.SetBackground(wxBrush{Scheme.getDefault().GetBackground()});
  dc.Clear();
  
  if (Statement.empty())
    return;
  
  auto const ActiveStmt = ActiveFn->getActiveStmt();
  if (!ActiveStmt)
    return;
  
  dc.SetFont(CodeFont);
  
  // Draw the sub-Stmts' nodes.
  for (auto const &Node : Nodes) {
    // Don't draw hovered nodes, they will be drawn later.
    if (&*HoverNodeIt == &Node || &*ReplayHoverNodeIt == &Node)
      continue;

    auto const DoHighlight =
      (HighlightedStmt && Node.Statement == HighlightedStmt)
      || (HighlightedValue && Node.Value.get() == HighlightedValue);

    if (DoHighlight)
      drawNode(dc, Scheme, Node, NodeDecoration::Highlighted);
    else if (Node.Statement == ActiveStmt)
      drawNode(dc, Scheme, Node, NodeDecoration::Active);
    else
      drawNode(dc, Scheme, Node, NodeDecoration::None);
  }
  
  // Redraw the hovered nodes, so that they outrank active node highlighting.
  if (HoverNodeIt != Nodes.end())
    drawNode(dc, Scheme, *HoverNodeIt, NodeDecoration::Highlighted);
  if (ReplayHoverNodeIt != Nodes.end())
    drawNode(dc, Scheme, *ReplayHoverNodeIt, NodeDecoration::Highlighted);
  
  // Draw the pretty-printed Stmt's string.
  dc.SetTextForeground(Scheme.getDefault().GetForeground());
  wxCoord const PageBorderH = dc.GetCharWidth() * Settings.PageBorderHorizontal;
  wxCoord const PageBorderV = dc.GetCharHeight() * Settings.PageBorderVertical;
  dc.DrawText(Statement, PageBorderH, PageBorderV);
}

void StateEvaluationTreePanel::recalculateNodePositions()
{
  wxClientDC dc(this);

  // Calculate the new size of the display.
  dc.SetFont(CodeFont);
  auto const StatementExtent = dc.GetTextExtent(Statement);
  auto const CharWidth = dc.GetCharWidth();
  auto const CharHeight = dc.GetCharHeight();

  wxCoord const PageBorderH = CharWidth * Settings.PageBorderHorizontal;
  wxCoord const PageBorderV = CharHeight * Settings.PageBorderVertical;
  wxCoord const NodeBorderV = CharHeight * Settings.NodeBorderVertical;

  auto const TotalWidth = StatementExtent.GetWidth() + (2 * PageBorderH);

  // Depth is zero-based, so there are (MaxDepth+1) lines for sub-nodes, plus
  // one line for the pretty-printed top-level node.
  auto const TotalHeight = ((MaxDepth + 2) * CharHeight)
                           + ((MaxDepth + 1) * NodeBorderV)
                           + (2 * PageBorderV);

  CurrentSize.Set(TotalWidth, TotalHeight);
  SetVirtualSize(TotalWidth, TotalHeight);

  // Calculate the position of each node in the display.
  for (auto &Node : Nodes) {
    auto const WidthPrior =
      dc.GetTextExtent(Statement.substr(0, Node.RangeStart))
        .GetWidth();

    auto const Width =
      dc.GetTextExtent(Statement.substr(Node.RangeStart, Node.RangeLength))
        .GetWidth();

    auto const XStart = PageBorderH + WidthPrior;
    auto const XEnd   = XStart + Width;
    auto const YStart = TotalHeight - PageBorderV - CharHeight
                        - (Node.Depth * (CharHeight + NodeBorderV));

    Node.XStart = XStart;
    Node.XEnd   = XEnd;
    Node.YStart = YStart;
    Node.YEnd   = YStart + CharHeight;
  }
}

void StateEvaluationTreePanel::redraw()
{
  wxClientDC dc(this);
  render(dc);
}

void StateEvaluationTreePanel::centreOnNode(NodeInfo const &Node)
{
  CentreOnArea(*this,
               GetClientSize(),
               wxRect(Node.XStart,
                      Node.YStart,
                      Node.XEnd - Node.XStart,
                      Node.YEnd - Node.YStart));
}

bool StateEvaluationTreePanel::setHoverNode(decltype(Nodes)::iterator const It)
{
  if (It == HoverNodeIt)
    return false;
  
  if (HoverTimer.IsRunning())
    HoverTimer.Stop();
  
  auto const PreviousHadValue = HoverNodeIt != Nodes.end()
                                && HoverNodeIt->Value.get() != nullptr;
  HoverNodeIt = It;
  
  if (Recording) {
    auto const NodeIndex = std::distance(Nodes.cbegin(), HoverNodeIt);
    auto const NodeStmt = It != Nodes.end() ? It->Statement   : nullptr;
    auto const Value    = It != Nodes.end() ? It->Value.get() : nullptr;

    std::vector<std::unique_ptr<IAttributeReadOnly>> Attrs;
    Attrs.emplace_back(new_attribute("node", NodeIndex));
    Attrs.emplace_back(new_attribute("stmt", NodeStmt));

    if (Value)
      addAttributesForValue(Attrs, *Value);

    Recording->recordEventV("StateEvaluationTree.NodeMouseOver", Attrs);
  }
  
  if (HoverNodeIt != Nodes.end())
    HoverTimer.Start(1000, wxTIMER_ONE_SHOT);
  
  if (Notifier) {
    auto const TheStmt = HoverNodeIt != Nodes.end() ? HoverNodeIt->Statement
                                                    : nullptr;
    Notifier->createNotify<ConEvHighlightStmt>(TheStmt);

    if (auto Access = CurrentAccess->getAccess()) {
      if (HoverNodeIt == Nodes.end()) {
        if (PreviousHadValue)
          Notifier->createNotify<ConEvHighlightValue>(nullptr, CurrentAccess);
      }
      else if (auto const TheValue = HoverNodeIt->Value.get()) {
        Notifier->createNotify<ConEvHighlightValue>(TheValue, CurrentAccess);
      }
    }
  }
  
  return true;
}

void StateEvaluationTreePanel::showHoverTooltip(NodeInfo const &Node)
{
  auto Access = CurrentAccess->getAccess();
  if (!Access)
    return;

  // TODO: This should appear on the node rather than the mouse.
  int const XStart = Node.XStart;
  int const YStart = Node.YStart;

  int const Width  = Node.XEnd - XStart;
  int const Height = Node.YEnd - YStart;

  auto const ClientStart = CalcScrolledPosition(wxPoint(XStart, YStart));
  auto const ScreenStart = ClientToScreen(ClientStart);

  wxRect NodeBounds{ScreenStart, wxSize{Width, Height}};

  // Determine a good maximum width for the tip window.
  auto const WindowSize = GetSize();
  auto const TipWidth = WindowSize.GetWidth();

  makeStmtTooltip(this, *Trace, Node.Statement, *ActiveFn, TipWidth,NodeBounds);
}

bool StateEvaluationTreePanel::treeContainsStmt(clang::Stmt const *S) const
{
  if (!S)
    return false;

  return std::any_of(Nodes.begin(), Nodes.end(),
          [S] (NodeInfo const &Node) { return Node.Statement == S; });
}

bool StateEvaluationTreePanel::treeContainsValue(seec::cm::Value const &V) const
{
  return std::any_of(Nodes.begin(), Nodes.end(),
          [&V] (NodeInfo const &Node) { return Node.Value.get() == &V; });
}

void StateEvaluationTreePanel::notifyContextEvent(ContextEvent const &Ev)
{
  switch (Ev.getKind())
  {
    case ContextEventKind::HighlightDecl:
      break;

    case ContextEventKind::HighlightStmt:
    {
      auto const ContainedPrev = treeContainsStmt(HighlightedStmt);
      auto const &Event = llvm::cast<ConEvHighlightStmt>(Ev);
      HighlightedStmt = Event.getStmt();

      if (ContainedPrev || treeContainsStmt(HighlightedStmt))
        redraw();

      break;
    }

    case ContextEventKind::HighlightValue:
    {
      auto const ContainedPrev = HighlightedValue
                                 && treeContainsValue(*HighlightedValue);
      auto const &Event = llvm::cast<ConEvHighlightValue>(Ev);
      HighlightedValue = Event.getValue();

      if (ContainedPrev
          || (HighlightedValue && treeContainsValue(*HighlightedValue)))
      {
        redraw();
      }

      break;
    }
  }
}

void
StateEvaluationTreePanel::
ReplayNodeMouseOver(decltype(Nodes)::difference_type const NodeIndex,
                    clang::Stmt const *Stmt)
{
  ReplayHoverNodeIt = std::find_if(Nodes.cbegin(), Nodes.cend(),
                                   [Stmt] (NodeInfo const &N) -> bool {
                                     return N.Statement == Stmt;
                                   });

  if (ReplayHoverNodeIt != Nodes.end())
    centreOnNode(*ReplayHoverNodeIt);
  redraw();
}

void
StateEvaluationTreePanel::
ReplayNodeRightClick(decltype(Nodes)::difference_type const NodeIndex,
                     clang::Stmt const *Stmt)
{
  // TODO: Ensure that the node is visible in the window.
  wxLogDebug("RIGHT CLICK NODE %d", (int)NodeIndex);
}

void
StateEvaluationTreePanel::
ReplayNodeHover(decltype(Nodes)::difference_type const NodeIndex,
                clang::Stmt const *Stmt)
{
  auto const NodeIt = std::find_if(Nodes.cbegin(), Nodes.cend(),
                                   [Stmt] (NodeInfo const &N) -> bool {
                                     return N.Statement == Stmt;
                                   });

  if (NodeIt != Nodes.end()) {
    centreOnNode(*NodeIt);
    showHoverTooltip(*NodeIt);
  }
}

StateEvaluationTreePanel::StateEvaluationTreePanel()
: Settings(),
  Notifier(nullptr),
  m_ColourSchemeSettingsRegistration(),
  Recording(nullptr),
  CurrentAccess(),
  CurrentProcess(nullptr),
  CurrentThread(nullptr),
  ActiveFn(nullptr),
  CurrentSize(1,1),
  CodeFont(),
  Statement(),
  MaxDepth(0),
  Nodes(),
  HoverNodeIt(Nodes.end()),
  ReplayHoverNodeIt(Nodes.end()),
  HoverTimer(),
  ClickUnmoved(false),
  HighlightedStmt(nullptr),
  HighlightedValue(nullptr)
{}

StateEvaluationTreePanel::StateEvaluationTreePanel(wxWindow *Parent,
                                                   OpenTrace &WithTrace,
                                                   ContextNotifier &TheNotifier,
                                                   ActionRecord &TheRecording,
                                                   ActionReplayFrame &TheReplay,
                                                   wxWindowID ID,
                                                   wxPoint const &Position,
                                                   wxSize const &Size)
: StateEvaluationTreePanel()
{
  Create(Parent, WithTrace, TheNotifier, TheRecording, TheReplay, ID, Position,
         Size);
}

StateEvaluationTreePanel::~StateEvaluationTreePanel() = default;

bool StateEvaluationTreePanel::Create(wxWindow *Parent,
                                      OpenTrace &WithTrace,
                                      ContextNotifier &WithNotifier,
                                      ActionRecord &WithRecording,
                                      ActionReplayFrame &WithReplay,
                                      wxWindowID ID,
                                      wxPoint const &Position,
                                      wxSize const &Size)
{
  if (!wxScrolled<wxPanel>::Create(Parent, ID, Position, Size))
    return false;
  
  Trace = &WithTrace;
  Notifier = &WithNotifier;
  Recording = &WithRecording;
  
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  SetScrollRate(10, 10);

  // Setup the current ColourScheme.
  auto &SchemeSettings = wxGetApp().getColourSchemeSettings();
  setupColourScheme(*SchemeSettings.getColourScheme());

  // Handle ColourScheme updates.
  m_ColourSchemeSettingsRegistration =
    SchemeSettings.addListener(
      [this] (ColourSchemeSettings const &Settings) {
        setupColourScheme(*Settings.getColourScheme());
        recalculateNodePositions();
        redraw();
      });

  HoverTimer.Bind(wxEVT_TIMER, &StateEvaluationTreePanel::OnHover, this);
  
  // Receive notifications of context events.
  Notifier->callbackAdd(seec::make_function([this] (ContextEvent const &Ev) {
                                              this->notifyContextEvent(Ev); }));

  WithReplay.RegisterHandler("StateEvaluationTree.NodeMouseOver",
                             {{"node", "stmt"}},
    seec::make_function(this, &StateEvaluationTreePanel::ReplayNodeMouseOver));
  
  WithReplay.RegisterHandler("StateEvaluationTree.NodeRightClick",
                             {{"node", "stmt"}},
    seec::make_function(this, &StateEvaluationTreePanel::ReplayNodeRightClick));
  
  WithReplay.RegisterHandler("StateEvaluationTree.NodeHover",
                             {{"node", "stmt"}},
    seec::make_function(this, &StateEvaluationTreePanel::ReplayNodeHover));
  
  return true;
}

/// \brief Determine if a Stmt is suitable for evaluation tree display.
///
static bool isSuitableEvaluationRoot(clang::Stmt const * const S)
{
  return llvm::isa<clang::Expr>(S);
}

/// \brief Find the "top-level" Stmt suitable for evaluation tree display.
///
static
clang::Stmt const *getEvaluationRoot(clang::Stmt const *S,
                                     seec::seec_clang::MappedAST const &AST)
{
  if (!isSuitableEvaluationRoot(S))
    return nullptr;
  
  while (true) {
    auto const MaybeParent = AST.getParent(S);
    if (!MaybeParent.assigned<clang::Stmt const *>())
      break;
    
    auto const Parent = MaybeParent.get<clang::Stmt const *>();
    if (!Parent || !isSuitableEvaluationRoot(Parent))
      break;
    
    S = Parent;
  }
  
  return S;
}

/// \brief Records the effective depth of each sub-node in a Stmt.
///
class DepthRecorder : public clang::RecursiveASTVisitor<DepthRecorder>
{
  enum class StmtPresence {
    Unknown,
    Unexpanded,
    Visible
  };

  seec::FormattedStmt const &Formatted;

  unsigned CurrentDepth;

  unsigned MaxDepth;

  llvm::DenseMap<clang::Stmt const *, unsigned> Depths;

  std::stack<StmtPresence> Visibilities;

  std::stack<bool> Shown;

  std::stack<clang::Stmt const *> Parents;

  StmtPresence getPresence(clang::Stmt const * const S) const {
    auto const Range = Formatted.getStmtRange(S);
    if (!Range)
      return StmtPresence::Unknown;

    if (Range->isEndHidden() && Range->isStartHidden())
      return StmtPresence::Unexpanded;

    return StmtPresence::Visible;
  }

  /// \param Visibility the true visibility of this node.
  ///
  bool shouldShow(clang::Stmt const * const S,
                  StmtPresence const Visibility) const
  {
    if (auto const Cast = llvm::dyn_cast<clang::ImplicitCastExpr>(S)) {
      // Hide certain implicit casts from students.
      switch (Cast->getCastKind()) {
        case clang::CastKind::CK_FunctionToPointerDecay: return false;
        default: break;
      }
    }

    if (Visibility == StmtPresence::Unknown)
      return false;

    if (Visibility == StmtPresence::Visible || Visibilities.empty())
      return true;

    // If the parent node was visible, but this node is not, then we should
    // show this node anyway (it will represent the entirity of the macro).
    if (Visibilities.top() == StmtPresence::Visible)
      return true;

    // If the parent was not shown, then certainly do not show this node.
    if (!Shown.top())
      return false;

    // If the parent was invisible but shown, and was of a certain type, then
    // show this expression as well:
    if (llvm::isa<clang::ParenExpr>(Parents.top())
        || llvm::isa<clang::ImplicitCastExpr>(Parents.top()))
      return true;

    return false;
  }

public:
  DepthRecorder(seec::FormattedStmt const &WithFormatted)
  : Formatted(WithFormatted),
    CurrentDepth(0),
    MaxDepth(0),
    Depths(),
    Visibilities(),
    Shown(),
    Parents()
  {}

  bool shouldUseDataRecursionFor(clang::Stmt *S) {
    return false;
  }

  bool DoTraverseStmt(clang::Stmt *S) {
    if (auto const E = llvm::dyn_cast<clang::CallExpr>(S)) {
      // If this is a direct function call, don't bother showing the nodes for
      // the DeclRefExpr and function to pointer decay - just show arg nodes.
      if (E->getDirectCallee()) {
        for (auto const &Arg : seec::range(E->arg_begin(), E->arg_end())) {
          if (!TraverseStmt(Arg))
            return false;
        }

        return true;
      }
    }

    return clang::RecursiveASTVisitor<DepthRecorder>::TraverseStmt(S);
  }

  bool TraverseStmt(clang::Stmt *S) {
    if (!S)
      return true;

    auto const Visible = getPresence(S);
    auto const Show = shouldShow(S, Visible);

    Visibilities.push(Visible);
    Shown.push(Show);
    Parents.push(S);

    if (Show) {
      if (CurrentDepth > MaxDepth)
        MaxDepth = CurrentDepth;
      Depths[S] = CurrentDepth;
      ++CurrentDepth;
    }

    auto const Result = DoTraverseStmt(S);

    if (Show)
      --CurrentDepth;

    Visibilities.pop();
    Shown.pop();
    Parents.pop();

    return Result;
  }

  unsigned getMaxDepth() const {
    return MaxDepth;
  }

  decltype(Depths) &getDepths() {
    return Depths;
  }

  decltype(Depths) const &getDepths() const {
    return Depths;
  }
};

void StateEvaluationTreePanel::show(std::shared_ptr<StateAccessToken> Access,
                                    seec::cm::ProcessState const &Process,
                                    seec::cm::ThreadState const &Thread)
{
  CurrentAccess = std::move(Access);
  CurrentProcess = &Process;
  CurrentThread = &Thread;
  ActiveFn = nullptr;
  Statement.clear();
  Nodes.clear();
  HoverNodeIt = Nodes.end();
  ReplayHoverNodeIt = Nodes.end();

  // Recalculate the data here.
  if (!CurrentThread) {
    redraw();
    return;
  }
  
  auto &Stack = CurrentThread->getCallStack();
  if (Stack.empty()) {
    redraw();
    return;
  }
  
  ActiveFn = &(Stack.back().get());
  auto const MappedAST = ActiveFn->getMappedAST();
  auto const &RunErrors = ActiveFn->getRuntimeErrors();
  auto const ActiveStmt = ActiveFn->getActiveStmt();
  if (!ActiveStmt) {
    redraw();
    return;
  }
  
  auto const TopStmt = getEvaluationRoot(ActiveStmt, *MappedAST);
  if (!TopStmt) {
    redraw();
    return;
  }
  
  // Format the Stmt and determine the ranges of sub-Stmts.
  auto const Formatted = seec::formatStmtSource(TopStmt, *MappedAST);
  
  // Determine the "depth" of each sub-Stmt.
  DepthRecorder DepthRecord(Formatted);
  DepthRecord.TraverseStmt(const_cast<clang::Stmt *>(TopStmt));
  auto const &Depths = DepthRecord.getDepths();
  MaxDepth = DepthRecord.getMaxDepth();
  
  // Now save all of the calculated information for the render method.
  Statement = Formatted.getCode();
  
  // Setup each node in the display.
  for (auto const &StmtRange : Formatted.getStmtRanges()) {
    // If the node has been hidden (because it is in an unexpanded macro) then
    // it will have no depth entry - we should simply skip it.
    auto const DepthIt = Depths.find(StmtRange.first);
    if (DepthIt == Depths.end())
      continue;

    auto const Depth  = DepthIt->second;
    
    auto Value = ActiveFn->getStmtValue(StmtRange.first);
    auto const ValueString = Value ? getPrettyStringForInline(*Value,
                                                              Process,
                                                              StmtRange.first)
                                   : UnicodeString{};
    auto const ValueStringShort =
      shortenValueString(ValueString, StmtRange.second.getLength());
    
    auto const HasError =
      std::any_of(RunErrors.begin(), RunErrors.end(),
                  [&] (seec::cm::RuntimeErrorState const &Err) {
                    return Err.getStmt() == StmtRange.first;
                  });

    Nodes.emplace_back(StmtRange.first,
                       std::move(Value),
                       seec::towxString(ValueString),
                       seec::towxString(ValueStringShort),
                       StmtRange.second.getStart(),
                       StmtRange.second.getLength(),
                       Depth,
                       0,
                       0,
                       0,
                       0,
                       HasError ? NodeError::Error : NodeError::None);
  }
  
  HoverNodeIt = Nodes.end();
  ReplayHoverNodeIt = Nodes.end();

  // Calculate the positions of the nodes.
  recalculateNodePositions();
  
  // Draw onto a new DC because we've changed the virtual size.
  redraw();
}

void StateEvaluationTreePanel::clear()
{
  CurrentAccess.reset();
  CurrentProcess = nullptr;
  CurrentThread = nullptr;
  Statement.clear();
  Nodes.clear();
  HoverNodeIt = Nodes.end();
  ReplayHoverNodeIt = Nodes.end();
  HoverTimer.Stop();
  
  SetVirtualSize(1, 1);
  
  redraw();
}

void StateEvaluationTreePanel::OnPaint(wxPaintEvent &Ev)
{
  wxAutoBufferedPaintDC dc(this);
  render(dc);
}

void StateEvaluationTreePanel::OnMouseMoved(wxMouseEvent &Ev)
{
  ClickUnmoved = false;
  auto const Pos = CalcUnscrolledPosition(Ev.GetPosition());
  
  // TODO: Find if the Pos is over the pretty-printed Stmt.
  
  // Find if the Pos is over a node's rectangle.
  auto const NewHoverNodeIt = std::find_if(Nodes.begin(), Nodes.end(),
    [&Pos] (NodeInfo const &Node) -> bool {
      return Node.XStart <= Pos.x && Pos.x <= Node.XEnd
          && Node.YStart <= Pos.y && Pos.y <= Node.YEnd;
    });
  
  if (setHoverNode(NewHoverNodeIt))
    redraw();
}

void StateEvaluationTreePanel::OnMouseLeftWindow(wxMouseEvent &Ev)
{
  ClickUnmoved = false;
  if (setHoverNode(Nodes.end()))
    redraw();
}

void StateEvaluationTreePanel::OnMouseRightDown(wxMouseEvent &Ev)
{
  ClickUnmoved = true;
}

void StateEvaluationTreePanel::OnMouseRightUp(wxMouseEvent &Ev)
{
  if (!ClickUnmoved)
    return;
  
  if (HoverNodeIt != Nodes.end()) {
    auto const Stmt = HoverNodeIt->Statement;
    auto const Value = HoverNodeIt->Value.get();

    if (Recording) {
      auto const NodeIndex = std::distance(Nodes.cbegin(), HoverNodeIt);
      Recording->recordEventL("StateEvaluationTree.NodeRightClick",
                              make_attribute("node", NodeIndex),
                              make_attribute("stmt", Stmt));
    }
    
    auto const MaybeIndex = CurrentProcess->getThreadIndex(*CurrentThread);
    if (!MaybeIndex.assigned<std::size_t>())
      return;
    
    auto const ThreadIndex = MaybeIndex.get<std::size_t>();
    
    wxMenu CM{};
    addStmtNavigation(*this, CurrentAccess, CM, ThreadIndex, Stmt, Recording);
    if (Value) {
      CM.AppendSeparator();
      addValueNavigation(*this, CurrentAccess, CM, *Value, *CurrentProcess,
                         Recording);
    }
    CM.AppendSeparator();
    addStmtAnnotationEdit(CM, this, *Trace, Stmt);
    PopupMenu(&CM);
  }
}

void StateEvaluationTreePanel::OnHover(wxTimerEvent &Ev)
{
  if (HoverNodeIt == Nodes.end())
    return;
  
  if (Recording) {
    auto const NodeIndex = std::distance(Nodes.cbegin(), HoverNodeIt);
    auto const NodeStmt = HoverNodeIt != Nodes.end() ? HoverNodeIt->Statement
                                                     : nullptr;
    
    Recording->recordEventL("StateEvaluationTree.NodeHover",
                            make_attribute("node", NodeIndex),
                            make_attribute("stmt", NodeStmt));
  }
  
  showHoverTooltip(*HoverNodeIt);
}

bool StateEvaluationTreePanel::renderToBMP(wxString const &Filename)
{
  wxBitmap Bitmap(CurrentSize.GetWidth(), CurrentSize.GetHeight());
  wxMemoryDC DC(Bitmap);

  render(DC);
  DC.SelectObject(wxNullBitmap);

  return Bitmap.SaveFile(Filename, wxBITMAP_TYPE_BMP);
}
