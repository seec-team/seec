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
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"

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

#include "NotifyContext.hpp"
#include "StateAccessToken.hpp"
#include "StateEvaluationTree.hpp"

#include <string>


BEGIN_EVENT_TABLE(StateEvaluationTreePanel, wxScrolled<wxPanel>)
  EVT_PAINT(StateEvaluationTreePanel::OnPaint)
  EVT_MOTION(StateEvaluationTreePanel::OnMouseMoved)
  EVT_LEAVE_WINDOW(StateEvaluationTreePanel::OnMouseLeftWindow)
END_EVENT_TABLE()


StateEvaluationTreePanel::DisplaySettings::DisplaySettings()
: PageBorderHorizontal(1.0),
  PageBorderVertical(1.0),
  NodeBorderVertical(0.5),
  CodeFontSize(12),
  NodeBackground(204, 204, 204),
  NodeBorder(102, 102, 102),
  NodeHighlightedBackground(102, 204, 204),
  NodeHighlightedBorder(51, 102, 102)
{}

StateEvaluationTreePanel::StateEvaluationTreePanel()
: Settings(),
  Notifier(nullptr),
  CurrentAccess(),
  CurrentProcess(nullptr),
  CurrentThread(nullptr),
  ActiveFn(nullptr),
  CodeFont(),
  Statement(),
  Nodes(),
  HoverNodeIt(Nodes.end())
{}

StateEvaluationTreePanel::StateEvaluationTreePanel(wxWindow *Parent,
                                                   ContextNotifier &TheNotifier,
                                                   wxWindowID ID,
                                                   wxPoint const &Position,
                                                   wxSize const &Size)
: Settings(),
  Notifier(nullptr),
  CurrentAccess(),
  CurrentProcess(nullptr),
  CurrentThread(nullptr),
  ActiveFn(nullptr),
  CodeFont(),
  Statement(),
  Nodes(),
  HoverNodeIt(Nodes.end())
{
  Create(Parent, TheNotifier, ID, Position, Size);
}

StateEvaluationTreePanel::~StateEvaluationTreePanel() = default;

bool StateEvaluationTreePanel::Create(wxWindow *Parent,
                                      ContextNotifier &WithNotifier,
                                      wxWindowID ID,
                                      wxPoint const &Position,
                                      wxSize const &Size)
{
  if (!wxScrolled<wxPanel>::Create(Parent, ID, Position, Size))
    return false;
  
  Notifier = &WithNotifier;
  
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  CodeFont = wxFont{wxFontInfo(Settings.CodeFontSize)
                    .Family(wxFONTFAMILY_MODERN)
                    .AntiAliased(true)};
  SetScrollRate(10, 10);
  
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

/// \brief Records the range of each Stmt in a pretty-printed Stmt.
///
class SubRangeRecorder : public clang::PrinterHelper
{
  clang::PrintingPolicy &Policy;
  
  std::string Buffer;
  
  llvm::raw_string_ostream BufferOS;
  
  llvm::DenseMap<clang::Stmt *, std::pair<uint64_t, std::size_t>> Ranges;
  
public:
  SubRangeRecorder(clang::PrintingPolicy &WithPolicy)
  : Policy(WithPolicy),
    Buffer(),
    BufferOS(Buffer),
    Ranges()
  {}
  
  virtual bool handledStmt(clang::Stmt *E, llvm::raw_ostream &OS) {
    // Print the Stmt to determine the length of its printed text.
    Buffer.clear();
    E->printPretty(BufferOS, nullptr, Policy);
    BufferOS.flush();
    
    // Record the start and length of the Stmt's printed text.
    Ranges.insert(std::make_pair(E, std::make_pair(OS.tell(), Buffer.size())));
    
    return false;
  }
  
  decltype(Ranges) &getRanges() {
    return Ranges;
  }
  
  decltype(Ranges) const &getRanges() const {
    return Ranges;
  }
};

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
  
  // TODO: get these from the ASTContext.
  clang::LangOptions LangOpts;
  
  clang::PrintingPolicy Policy(LangOpts);
  Policy.Indentation = 0;
  Policy.Bool = true;
  Policy.ConstantArraySizeAsWritten = true;
  
  SubRangeRecorder PrinterHelper(Policy);
  
  TopStmt->printPretty(StmtOS, &PrinterHelper, Policy);
  
  StmtOS.flush();
  
  // Determine the "depth" of each sub-Stmt.
  DepthRecorder DepthRecord;
  DepthRecord.TraverseStmt(const_cast<clang::Stmt *>(TopStmt));
  auto const &Depths = DepthRecord.getDepths();
  auto const MaxDepth = DepthRecord.getMaxDepth();
  
  // Now save all of the calculated information for the render method.
  Statement = PrettyPrintedStmt;
  auto const &Ranges = PrinterHelper.getRanges();
  
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
    
    auto const Depth = DepthIt->second;
    auto const XStart = PageBorderH + (StmtRange.second.first * CharWidth);
    auto const XEnd = XStart + (StmtRange.second.second * CharWidth);
    auto const YStart = TotalHeight - PageBorderV - CharHeight
                        - (Depth * (CharHeight + NodeBorderV));
    
    Nodes.emplace_back(StmtRange.first,
                       ActiveFn->getStmtValue(StmtRange.first),
                       StmtRange.second.first,
                       StmtRange.second.second,
                       Depth,
                       XStart,
                       XEnd,
                       YStart,
                       YStart + CharHeight);
  }
  
  HoverNodeIt = Nodes.end();
  
  // Create a new DC because we've changed the virtual size.
  wxClientDC newdc(this);
  render(newdc);
}

void StateEvaluationTreePanel::clear()
{
  CurrentAccess.reset();
  CurrentProcess = nullptr;
  CurrentThread = nullptr;
  Statement.clear();
  Nodes.clear();
  
  SetVirtualSize(1, 1);
  
  wxClientDC dc(this);
  render(dc);
}

void StateEvaluationTreePanel::render(wxDC &dc)
{
  PrepareDC(dc);
  
  dc.Clear();
  if (Statement.empty()) {
    wxLogDebug("empty stmt");
    return;
  }
  
  auto const CharWidth = dc.GetCharWidth();
  auto const CharHeight = dc.GetCharHeight();
  
  wxCoord const PageBorderH = CharWidth * Settings.PageBorderHorizontal;
  wxCoord const PageBorderV = CharHeight * Settings.PageBorderVertical;
  
  dc.SetFont(CodeFont);
  dc.SetTextForeground(*wxBLACK);
  
  wxPen TreeLinePen{*wxBLACK};
  
  wxPen TreeBackPen{Settings.NodeBorder};
  wxBrush TreeBackBrush{Settings.NodeBackground};
  
  wxPen HighlightedBackPen{Settings.NodeHighlightedBorder};
  wxBrush HighlightedBackBrush{Settings.NodeHighlightedBackground};
  
  // Draw the sub-Stmt's node's backgrounds.
  for (auto const &Node : Nodes) {
    dc.SetPen(TreeBackPen);
    dc.SetBrush(TreeBackBrush);
    dc.DrawRectangle(Node.XStart, Node.YStart,
                     Node.XEnd - Node.XStart, Node.YEnd - Node.YStart);
  }
  
  // Draw the hovered node's background.
  if (HoverNodeIt != Nodes.end()) {
    auto const &Node = *HoverNodeIt;
    dc.SetPen(HighlightedBackPen);
    dc.SetBrush(HighlightedBackBrush);
    dc.DrawRectangle(Node.XStart, Node.YStart,
                     Node.XEnd - Node.XStart, Node.YEnd - Node.YStart);
    
    // Also highlight this node's area in the pretty-printed Stmt.
    dc.DrawRectangle(Node.XStart, PageBorderV,
                     Node.XEnd - Node.XStart, CharHeight);
  }
  
  // Draw the pretty-printed Stmt's string.
  dc.DrawText(Statement, PageBorderH, PageBorderV);
  
  // Draw the individual sub-Stmts' strings.
  for (auto const &Node : Nodes) {
    dc.SetPen(TreeLinePen);
    dc.DrawLine(Node.XStart, Node.YStart, Node.XEnd, Node.YStart);
    
    if (Node.Value) {
      auto ValText = Node.Value->getValueAsStringShort();
      if (ValText.size() > Node.RangeLength) {
        ValText.erase(Node.RangeLength);
      }
      
      auto const TextWidth = CharWidth * ValText.size();
      auto const NodeWidth = CharWidth * Node.RangeLength;
      auto const Offset = (NodeWidth - TextWidth) / 2;
      dc.DrawText(ValText, Node.XStart + Offset, Node.YStart);
    }
  }
}

void StateEvaluationTreePanel::OnPaint(wxPaintEvent &Ev)
{
  wxPaintDC dc(this);
  render(dc);
}

void StateEvaluationTreePanel::OnMouseMoved(wxMouseEvent &Ev)
{
  bool DisplayChanged = false;
  auto const Pos = CalcUnscrolledPosition(Ev.GetPosition());
  
  // TODO: Find if the Pos is over the pretty-printed Stmt.
  
  // Find if the Pos is over a node's rectangle.
  auto const NewHoverNodeIt = std::find_if(Nodes.begin(), Nodes.end(),
    [&Pos] (NodeInfo const &Node) -> bool {
      return Node.XStart <= Pos.x && Pos.x <= Node.XEnd
          && Node.YStart <= Pos.y && Pos.y <= Node.YEnd;
    });
  
  if (NewHoverNodeIt != HoverNodeIt) {
    HoverNodeIt = NewHoverNodeIt;
    DisplayChanged = true;
    
    if (Notifier) {
      auto const TheStmt = HoverNodeIt != Nodes.end() ? HoverNodeIt->Statement
                                                      : nullptr;
      Notifier->createNotify<ConEvHighlightStmt>(TheStmt);
    }
  }
  
  // Redraw the display.
  if (DisplayChanged) {
    wxClientDC dc(this);
    render(dc);
  }
}

void StateEvaluationTreePanel::OnMouseLeftWindow(wxMouseEvent &Ev)
{
  bool DisplayChanged = false;
  
  if (HoverNodeIt != Nodes.end()) {
    HoverNodeIt = Nodes.end();
    DisplayChanged = true;
  }
  
  // Redraw the display.
  if (DisplayChanged) {
    wxClientDC dc(this);
    render(dc);
  }
}
