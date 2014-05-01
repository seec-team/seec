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

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "llvm/Support/raw_ostream.h"

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "CommonMenus.hpp"
#include "NotifyContext.hpp"
#include "RuntimeValueLookup.hpp"
#include "StateAccessToken.hpp"
#include "StateEvaluationTree.hpp"
#include "ValueFormat.hpp"

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
  NodeBackground(204, 204, 204),
  NodeBorder(102, 102, 102),
  NodeActiveBackground(200, 255, 200),
  NodeActiveBorder(100, 127, 100),
  NodeHighlightedBackground(102, 204, 204),
  NodeHighlightedBorder(51, 102, 102)
{}

void StateEvaluationTreePanel::drawNode(wxDC &DC,
                                        NodeInfo const &Node,
                                        NodeDecoration const Decoration)
{
  auto const CharWidth  = DC.GetCharWidth();
  auto const CharHeight = DC.GetCharHeight();
  
  wxCoord const PageBorderV = CharHeight * Settings.PageBorderVertical;
  
  // Set the background colour.
  switch (Decoration) {
    case NodeDecoration::None:
      DC.SetPen(wxPen{Settings.NodeBorder});
      DC.SetBrush(wxBrush{Settings.NodeBackground});
      break;
    case NodeDecoration::Active:
      DC.SetPen(wxPen{Settings.NodeActiveBorder});
      DC.SetBrush(wxBrush{Settings.NodeActiveBackground});
      break;
    case NodeDecoration::Highlighted:
      DC.SetPen(wxPen{Settings.NodeHighlightedBorder});
      DC.SetBrush(wxBrush{Settings.NodeHighlightedBackground});
      break;
  }
  
  // Also highlight this node's area in the pretty-printed Stmt.
  if (Decoration == NodeDecoration::Active
      || Decoration == NodeDecoration::Highlighted)
  {
    DC.DrawRectangle(Node.XStart, PageBorderV,
                     Node.XEnd - Node.XStart, CharHeight);
  }
  
  // Draw the background.
  DC.DrawRectangle(Node.XStart, Node.YStart,
                   Node.XEnd - Node.XStart, Node.YEnd - Node.YStart);
  
  // Draw the line over the node.
  DC.SetPen(wxPen{*wxBLACK});
  DC.DrawLine(Node.XStart, Node.YStart, Node.XEnd, Node.YStart);
  
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
  PrepareDC(dc);
  
  dc.Clear();
  if (Statement.empty())
    return;
  
  auto const ActiveStmt = ActiveFn->getActiveStmt();
  if (!ActiveStmt)
    return;
  
  dc.SetFont(CodeFont);
  dc.SetTextForeground(*wxBLACK);
  
  // Draw the sub-Stmts' nodes.
  for (auto const &Node : Nodes) {
    if (Node.Statement == ActiveStmt)
      drawNode(dc, Node, NodeDecoration::Active);
    else
      drawNode(dc, Node, NodeDecoration::None);
  }
  
  // Redraw the hovered nodes, so that they outrank active node highlighting.
  if (HoverNodeIt != Nodes.end())
    drawNode(dc, *HoverNodeIt, NodeDecoration::Highlighted);
  if (ReplayHoverNodeIt != Nodes.end())
    drawNode(dc, *ReplayHoverNodeIt, NodeDecoration::Highlighted);
  
  // Draw the pretty-printed Stmt's string.
  wxCoord const PageBorderH = dc.GetCharWidth() * Settings.PageBorderHorizontal;
  wxCoord const PageBorderV = dc.GetCharHeight() * Settings.PageBorderVertical;
  dc.DrawText(Statement, PageBorderH, PageBorderV);
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

bool StateEvaluationTreePanel::setHoverNode(decltype(Nodes)::iterator It)
{
  if (It == HoverNodeIt)
    return false;
  
  if (HoverTimer.IsRunning())
    HoverTimer.Stop();
  
  HoverNodeIt = It;
  
  if (Recording) {
    auto const NodeIndex = std::distance(Nodes.cbegin(), HoverNodeIt);
    auto const NodeStmt = It != Nodes.end() ? It->Statement : nullptr;

    Recording->recordEventL("StateEvaluationTree.NodeMouseOver",
                            make_attribute("node", NodeIndex),
                            make_attribute("stmt", NodeStmt));
  }
  
  if (HoverNodeIt != Nodes.end())
    HoverTimer.Start(1000, wxTIMER_ONE_SHOT);
  
  if (Notifier) {
    auto const TheStmt = HoverNodeIt != Nodes.end() ? HoverNodeIt->Statement
                                                    : nullptr;
    Notifier->createNotify<ConEvHighlightStmt>(TheStmt);
  }
  
  return true;
}

void StateEvaluationTreePanel::showHoverTooltip(NodeInfo const &Node)
{
  auto const Statement = Node.Statement;
  wxString TipString;
  
  // Add the complete value string.
  if (Node.ValueString.size()) {
    TipString += Node.ValueString;
    TipString += "\n";
  }
  
  // Add the type of the value.
  if (auto const E = llvm::dyn_cast<clang::Expr>(Statement)) {
    TipString += E->getType().getAsString();
    TipString += "\n";
  }
  
  // Attempt to get a general explanation of the statement.
  auto const MaybeExplanation =
    seec::clang_epv::explain(Statement,
                             RuntimeValueLookupForFunction{ActiveFn});
  
  if (MaybeExplanation.assigned(0)) {
    auto const &Explanation = MaybeExplanation.get<0>();
    if (TipString.size())
      TipString += "\n";
    TipString += seec::towxString(Explanation->getString());
    TipString += "\n";
  }
  else if (MaybeExplanation.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto const String = MaybeExplanation.get<seec::Error>()
                                        .getMessage(Status, Locale());
    
    if (U_SUCCESS(Status)) {
      wxLogDebug("Error getting explanation: %s", seec::towxString(String));
    }
    else {
      wxLogDebug("Indescribable error getting explanation.");
    }
  }
  
  // Get any runtime errors related to the Stmt.
  for (auto const &RuntimeError : ActiveFn->getRuntimeErrors()) {
    if (RuntimeError.getStmt() != Statement)
      continue;
    
    auto const MaybeDescription = RuntimeError.getDescription();
    if (MaybeDescription.assigned(0)) {
      auto const &Description = MaybeDescription.get<0>();
      if (TipString.size())
        TipString += "\n";
      TipString += seec::towxString(Description->getString());
    }
    else if (MaybeDescription.assigned<seec::Error>()) {
      UErrorCode Status = U_ZERO_ERROR;
      auto const String = MaybeDescription.get<seec::Error>()
                                          .getMessage(Status, Locale());
      
      if (U_SUCCESS(Status)) {
        wxLogDebug("Error getting description: %s", seec::towxString(String));
      }
      else {
        wxLogDebug("Indescribable error getting description.");
      }
    }
  }
  
  // Display the generated tooltip (if any).
  // TODO: This should appear on the node rather than the mouse.
  if (TipString.size()) {
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
    
    new wxTipWindow(this, TipString, TipWidth, nullptr, &NodeBounds);
  }
}

void
StateEvaluationTreePanel::
ReplayNodeMouseOver(decltype(Nodes)::difference_type const NodeIndex,
                    clang::Stmt const *Stmt)
{
  ReplayHoverNodeIt = std::next(Nodes.cbegin(), NodeIndex);
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
  auto const NodeIt = std::next(Nodes.cbegin(), NodeIndex);
  if (NodeIt != Nodes.end()) {
    centreOnNode(*NodeIt);
    showHoverTooltip(*NodeIt);
  }
}

StateEvaluationTreePanel::StateEvaluationTreePanel()
: Settings(),
  Notifier(nullptr),
  Recording(nullptr),
  CurrentAccess(),
  CurrentProcess(nullptr),
  CurrentThread(nullptr),
  ActiveFn(nullptr),
  CodeFont(),
  Statement(),
  Nodes(),
  HoverNodeIt(Nodes.end()),
  ReplayHoverNodeIt(Nodes.end()),
  HoverTimer(),
  ClickUnmoved(false)
{}

StateEvaluationTreePanel::StateEvaluationTreePanel(wxWindow *Parent,
                                                   ContextNotifier &TheNotifier,
                                                   ActionRecord &TheRecording,
                                                   ActionReplayFrame &TheReplay,
                                                   wxWindowID ID,
                                                   wxPoint const &Position,
                                                   wxSize const &Size)
: StateEvaluationTreePanel()
{
  Create(Parent, TheNotifier, TheRecording, TheReplay, ID, Position, Size);
}

StateEvaluationTreePanel::~StateEvaluationTreePanel() = default;

bool StateEvaluationTreePanel::Create(wxWindow *Parent,
                                      ContextNotifier &WithNotifier,
                                      ActionRecord &WithRecording,
                                      ActionReplayFrame &WithReplay,
                                      wxWindowID ID,
                                      wxPoint const &Position,
                                      wxSize const &Size)
{
  if (!wxScrolled<wxPanel>::Create(Parent, ID, Position, Size))
    return false;
  
  Notifier = &WithNotifier;
  Recording = &WithRecording;
  
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  CodeFont = wxFont{wxFontInfo(Settings.CodeFontSize)
                    .Family(wxFONTFAMILY_MODERN)
                    .AntiAliased(true)};
  SetScrollRate(10, 10);
  
  HoverTimer.Bind(wxEVT_TIMER, &StateEvaluationTreePanel::OnHover, this);
  
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

/// \brief Records the depth of each sub-node in a Stmt.
///
class DepthRecorder : public clang::RecursiveASTVisitor<DepthRecorder>
{
  unsigned CurrentDepth;
  
  unsigned MaxDepth;
  
  llvm::DenseMap<clang::Stmt const *, unsigned> Depths;
  
public:
  DepthRecorder()
  : CurrentDepth(0),
    MaxDepth(0),
    Depths()
  {}
  
  bool shouldUseDataRecursionFor(clang::Stmt *S) {
    return false;
  }
  
  bool TraverseStmt(clang::Stmt *S) {
    if (!S)
      return true;
    
    if (CurrentDepth > MaxDepth)
      MaxDepth = CurrentDepth;
    
    Depths[S] = CurrentDepth;
    
    ++CurrentDepth;
    
    clang::RecursiveASTVisitor<DepthRecorder>::TraverseStmt(S);
    
    --CurrentDepth;
    
    return true;
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
  
  wxClientDC dc(this);
  
  // Recalculate the data here.
  if (!CurrentThread) {
    render(dc);
    return;
  }
  
  auto &Stack = CurrentThread->getCallStack();
  if (Stack.empty()) {
    render(dc);
    return;
  }
  
  ActiveFn = &(Stack.back().get());
  auto const MappedAST = ActiveFn->getMappedAST();
  auto const ActiveStmt = ActiveFn->getActiveStmt();
  if (!ActiveStmt) {
    render(dc);
    return;
  }
  
  auto const TopStmt = getEvaluationRoot(ActiveStmt, *MappedAST);
  if (!TopStmt) {
    render(dc);
    return;
  }
  
  std::string PrettyPrintedStmt;
  llvm::raw_string_ostream StmtOS(PrettyPrintedStmt);
  
  auto &ASTUnit = MappedAST->getASTUnit();
  clang::LangOptions LangOpts = ASTUnit.getASTContext().getLangOpts();
  
  clang::PrintingPolicy Policy(LangOpts);
  Policy.Indentation = 0;
  Policy.Bool = true;
  Policy.ConstantArraySizeAsWritten = true;
  
  auto const &Ranges = seec::printStmtAndRecordRanges(StmtOS, TopStmt, Policy);
  
  // Determine the "depth" of each sub-Stmt.
  DepthRecorder DepthRecord;
  DepthRecord.TraverseStmt(const_cast<clang::Stmt *>(TopStmt));
  auto const &Depths = DepthRecord.getDepths();
  auto const MaxDepth = DepthRecord.getMaxDepth();
  
  // Now save all of the calculated information for the render method.
  Statement = PrettyPrintedStmt;
  
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
  
  SetVirtualSize(TotalWidth, TotalHeight);
  
  // Calculate the position of each node in the display.
  for (auto const &StmtRange : Ranges) {
    auto const DepthIt = Depths.find(StmtRange.first);
    if (DepthIt == Depths.end()) {
      wxLogDebug("Couldn't get depth for sub-Stmt.");
      continue;
    }
    
    auto const WidthPrior =
      dc.GetTextExtent(Statement.substr(0, StmtRange.second.getStart()))
        .GetWidth();
    auto const Width =
      dc.GetTextExtent(Statement.substr(StmtRange.second.getStart(),
                                        StmtRange.second.getLength()))
        .GetWidth();

    auto const Depth  = DepthIt->second;
    auto const XStart = PageBorderH + WidthPrior;
    auto const XEnd   = XStart + Width;
    auto const YStart = TotalHeight - PageBorderV - CharHeight
                        - (Depth * (CharHeight + NodeBorderV));
    
    auto Value = ActiveFn->getStmtValue(StmtRange.first);
    auto const ValueString = Value ? getPrettyStringForInline(*Value, Process)
                                   : UnicodeString{};
    auto const ValueStringShort =
      shortenValueString(ValueString, StmtRange.second.getLength());
    
    Nodes.emplace_back(StmtRange.first,
                       std::move(Value),
                       seec::towxString(ValueString),
                       seec::towxString(ValueStringShort),
                       StmtRange.second.getStart(),
                       StmtRange.second.getLength(),
                       Depth,
                       XStart,
                       XEnd,
                       YStart,
                       YStart + CharHeight);
  }
  
  HoverNodeIt = Nodes.end();
  ReplayHoverNodeIt = Nodes.end();
  
  // Create a new DC because we've changed the virtual size.
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
  wxPaintDC dc(this);
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
    if (Recording) {
      auto const NodeIndex = std::distance(Nodes.cbegin(), HoverNodeIt);
      auto const NodeStmt = HoverNodeIt != Nodes.end() ? HoverNodeIt->Statement
                                                       : nullptr;
      
      Recording->recordEventL("StateEvaluationTree.NodeRightClick",
                              make_attribute("node", NodeIndex),
                              make_attribute("stmt", NodeStmt));
    }
    
    auto const MaybeIndex = CurrentProcess->getThreadIndex(*CurrentThread);
    if (!MaybeIndex.assigned<std::size_t>())
      return;
    
    auto const ThreadIndex = MaybeIndex.get<std::size_t>();
    auto const Statement = HoverNodeIt->Statement;
    
    wxMenu CM{};
    addStmtNavigation(*this, CurrentAccess, CM, ThreadIndex, Statement);
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
