//===- lib/Clang/GraphLayout.cpp ------------------------------------------===//
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


#include "GraphExpansion.hpp"

#include "seec/Clang/GraphLayout.hpp"
#include "seec/Clang/MappedAllocaState.hpp"
#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/MakeUnique.hpp"

#include <future>


namespace seec {

namespace cm {

namespace graph {


//===----------------------------------------------------------------------===//
// Helper functions
//===----------------------------------------------------------------------===//

std::string getStandardPortFor(Value const &V)
{
  std::string Port;
  
  {
    llvm::raw_string_ostream PortStream {Port};
    PortStream << "value_at_" << reinterpret_cast<uintptr_t>(&V);
  }
  
  return Port;
}

static std::string EscapeForHTML(llvm::StringRef String)
{
  std::string Escaped;
  Escaped.reserve(String.size());
  
  for (auto const Char : String) {
    if (std::isalnum(Char))
      Escaped.push_back(Char);
    else {
      Escaped.push_back('&');
      Escaped.push_back('#');
      
      if (Char > 99) {
        Escaped.push_back('0' + ( Char        / 100));
        Escaped.push_back('0' + ((Char % 100) / 10 ));
        Escaped.push_back('0' + ( Char % 10        ));
      }
      else if (Char > 9) {
        Escaped.push_back('0' + (Char / 10));
        Escaped.push_back('0' + (Char % 10));
      }
      
      Escaped.push_back(';');
    }
  }
  
  return Escaped;
}


//===----------------------------------------------------------------------===//
// Value types
//===----------------------------------------------------------------------===//


class NodeInfo {
  std::string ID;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  NodeInfo(std::string WithID,
           MemoryArea WithArea,
           ValuePortMap WithPorts)
  : ID(std::move(WithID)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getID() const { return ID; }
  
  MemoryArea const &getArea() const { return Area; }
  
  seec::Maybe<ValuePort> getPortForValue(Value const &Val) const {
    return Ports.getPortForValue(Val);
  }
};

/// \brief Represents an edge that has been laid out.
///
class LayoutOfPointer {
  EdgeEndType TailType;
  
  EdgeEndType HeadType;
  
public:
  LayoutOfPointer(EdgeEndType WithTailType,
                  EdgeEndType WithHeadType)
  : TailType(WithTailType),
    HeadType(WithHeadType)
  {}
  
  EdgeEndType getTailType() const { return TailType; }
  
  EdgeEndType getHeadType() const { return HeadType; }
};


/// \brief Represents the layout of a seec::cm::AllocaState.
///
class LayoutOfAlloca {
  std::string DotString;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  LayoutOfAlloca(std::string WithDotString,
                 MemoryArea WithArea,
                 ValuePortMap WithPorts)
  : DotString(std::move(WithDotString)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Area) const &getArea() const { return Area; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


/// \brief Represents the layout of a seec::cm::FunctionState.
///
class LayoutOfFunction {
  std::string ID;
  
  std::string DotString;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  LayoutOfFunction(std::string WithID,
                   std::string WithDotString,
                   MemoryArea WithArea,
                   ValuePortMap WithPorts)
  : ID(std::move(WithID)),
    DotString(std::move(WithDotString)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getID() const { return ID; }
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Area) const &getArea() const { return Area; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


/// \brief Represents the layout of a seec::cm::ThreadState.
///
class LayoutOfThread {
  std::string DotString;
  
  std::vector<NodeInfo> Nodes;
  
public:
  LayoutOfThread(std::string WithDotString,
                 std::vector<NodeInfo> WithNodes)
  : DotString(std::move(WithDotString)),
    Nodes(std::move(WithNodes))
  {}
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Nodes) const &getNodes() const { return Nodes; }
};


/// \brief Represents the layout of a seec::cm::GlobalVariable.
///
class LayoutOfGlobalVariable {
  std::string ID;
  
  std::string DotString;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  LayoutOfGlobalVariable(std::string WithID,
                         std::string WithDotString,
                         MemoryArea WithArea,
                         ValuePortMap WithPorts)
  : ID(std::move(WithID)),
    DotString(std::move(WithDotString)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getID() const { return ID; }
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Area) const &getArea() const { return Area; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


//===----------------------------------------------------------------------===//
// LEVStandard
//===----------------------------------------------------------------------===//

/// \brief Value layout engine "Standard".
///
class LEVStandard final : public LayoutEngineForValue {
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout", "LEVStandard", "Name"});
  }
  
  virtual bool canLayoutImpl(Value const &V) const override {
    return true;
  }
  
  virtual LayoutOfValue
  doLayoutImpl(Value const &V) const override;
  
public:
  LEVStandard(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler}
  {}
};

LayoutOfValue
LEVStandard::doLayoutImpl(Value const &V) const
{
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  
  switch (V.getKind()) {
    case Value::Kind::Basic:
    {
      auto const IsInit = V.isCompletelyInitialized();
      
      Stream << "<TD PORT=\""
             << getStandardPortFor(V)
             << "\">";
      
      if (IsInit)
        Stream << EscapeForHTML(V.getValueAsStringFull());
      
      Stream << "</TD>";
      
      break;
    }
    
    case Value::Kind::Array:
    {
      auto const &Array = static_cast<ValueOfArray const &>(V);
      unsigned const ChildCount = Array.getChildCount();
      
      Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"><TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
      
      for (unsigned i = 0; i < ChildCount; ++i) {
        auto const ChildValue = Array.getChildAt(i);
        if (!ChildValue)
          continue;
        
        Stream << "<TR><TD>&#91;" << i << "&#93;</TD>";
        
        auto const MaybeLayout = this->getHandler().doLayout(*ChildValue);
        if (!MaybeLayout.assigned<LayoutOfValue>()) {
          Stream << "<TD></TD></TR>";
          continue;
        }
        
        auto const &Layout = MaybeLayout.get<LayoutOfValue>();
        
        Stream << Layout.getDotString()
               << "</TR>";
        
        Ports.addAllFrom(Layout.getPorts());
      }
      
      Stream << "</TABLE></TD>";
      
      break;
    }
    
    case Value::Kind::Record:
    {
      auto const &Record = static_cast<ValueOfRecord const &>(V);
      unsigned const ChildCount = Record.getChildCount();
      
      Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"><TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
      
      for (unsigned i = 0; i < ChildCount; ++i) {
        auto const ChildValue = Record.getChildAt(i);
        if (!ChildValue)
          continue;
        
        Stream << "<TR><TD>";
        
        auto const ChildField = Record.getChildField(i);
        if (ChildField)
          Stream << ChildField->getName();
        else
          Stream << "unknown field"; // TODO: Localize
        
        Stream << "</TD>";
        
        auto const MaybeLayout = this->getHandler().doLayout(*ChildValue);
        if (!MaybeLayout.assigned<LayoutOfValue>()) {
          Stream << "<TD></TD></TR>";
          continue;
        }
        
        auto const &Layout = MaybeLayout.get<LayoutOfValue>();
        
        Stream << Layout.getDotString()
               << "</TR>";
        
        Ports.addAllFrom(Layout.getPorts());
      }
      
      Stream << "</TABLE></TD>";
      
      break;
    }
    
    case Value::Kind::Pointer:
    {
      auto const &Ptr = static_cast<ValueOfPointer const &>(V);
      
      if (!Ptr.isCompletelyInitialized()) {
        // An uninitialized pointer.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\">?</TD>";
      }
      else if (!Ptr.getRawValue()) {
        // A NULL pointer.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\">NULL</TD>";
      }
      else if (Ptr.getDereferenceIndexLimit() == 0) {
        // An invalid pointer (as far as we're concerned).
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\">!</TD>";
      }
      else {
        // A valid pointer with at least one dereference.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"> </TD>";
      }
      
      break;
    }
  }
  
  Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  Stream.flush();
  return LayoutOfValue{std::move(DotString), std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// LEAStandard
//===----------------------------------------------------------------------===//

/// \brief Area layout engine "Standard".
///
class LEAStandard final : public LayoutEngineForArea {
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout", "LEAStandard", "Name"});
  }
  
  virtual bool
  canLayoutImpl(seec::MemoryArea const &Area,
                seec::cm::ValueOfPointer const &Reference) const override
  {
    return true;
  }
  
  virtual LayoutOfArea
  doLayoutImpl(seec::MemoryArea const &Area,
               seec::cm::ValueOfPointer const &Reference) const override;
  
public:
  LEAStandard(LayoutHandler const &InHandler)
  : LayoutEngineForArea{InHandler}
  {}
};

LayoutOfArea
LEAStandard::doLayoutImpl(seec::MemoryArea const &Area,
                          seec::cm::ValueOfPointer const &Reference) const
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "area_at_" << Area.start();
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
  
  auto const &Handler = this->getHandler();
  auto const Limit = Reference.getDereferenceIndexLimit();
  
  if (Limit == 1) {
    auto const Pointee = Reference.getDereferenced(0);
    if (Pointee) {
      auto const MaybeLayout = Handler.doLayout(*Pointee);
      if (MaybeLayout.assigned<LayoutOfValue>()) {
        auto const &Layout = MaybeLayout.get<LayoutOfValue>();
        DotStream << "<TR>" << Layout.getDotString() << "</TR>";
        Ports.addAllFrom(Layout.getPorts());
      }
    }
  }
  else {
    for (unsigned i = 0; i < Limit; ++i) {
      auto const Pointee = Reference.getDereferenced(i);
      if (Pointee) {
        auto const MaybeLayout = Handler.doLayout(*Pointee);
        if (MaybeLayout.assigned<LayoutOfValue>()) {
          auto const &Layout = MaybeLayout.get<LayoutOfValue>();
          DotStream << "<TR><TD>&#91;" << i << "&#93;</TD>"
                    << Layout.getDotString()
                    << "</TR>";
          Ports.addAllFrom(Layout.getPorts());
        }
      }
    }
  }
  
  DotStream << "</TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfArea{std::move(IDString),
                      std::move(DotString),
                      std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// Layout creation
//===----------------------------------------------------------------------===//

static
LayoutOfAlloca
doLayout(LayoutHandler const &Handler,
         seec::cm::AllocaState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Attempt to get the value.
  auto const Value = State.getValue();
  if (!Value)
    return LayoutOfAlloca{std::string{}, MemoryArea{}, ValuePortMap{}};
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
  DotStream << "<TR><TD>"
            << State.getDecl()->getNameAsString()
            << "</TD>";
  
  // Attempt to layout the value.
  auto MaybeLayout = Handler.doLayout(*Value);
  if (MaybeLayout.assigned<LayoutOfValue>()) {
    auto const &Layout = MaybeLayout.get<LayoutOfValue>();
    DotStream << Layout.getDotString();
    Ports.addAllFrom(Layout.getPorts());
  }
  else {
    DotStream << "<TD>no layout</TD>"; // TODO: Localise
  }
  
  DotStream << "</TR>";
  DotStream.flush();
  
  if (Value->isInMemory())
    Area = MemoryArea(Value->getAddress(),
                      Value->getTypeSizeInChars().getQuantity());
  
  return LayoutOfAlloca{std::move(DotString),
                        std::move(Area),
                        std::move(Ports)};
}

static
LayoutOfFunction
doLayout(LayoutHandler const &Handler,
         seec::cm::FunctionState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "function_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">"
            << "<TR><TD COLSPAN=\"2\">"
            << State.getNameAsString()
            << "</TD></TR>";
  
  for (auto const &Parameter : State.getParameters()) {
    auto const Layout = doLayout(Handler, Parameter, Expansion);
    
    DotStream << Layout.getDotString();
    
    Ports.addAllFrom(Layout.getPorts());
    
    auto const &AllocaArea = Layout.getArea();
    if (AllocaArea.end() > Area.end())
      Area.setEnd(AllocaArea.end());
    if (Area.start() == 0 || AllocaArea.start() < Area.start())
      Area.setStart(AllocaArea.start());
  }
  
  for (auto const &Local : State.getLocals()) {
    auto const Layout = doLayout(Handler, Local, Expansion);
    
    DotStream << Layout.getDotString();
    
    Ports.addAllFrom(Layout.getPorts());
    
    auto const &AllocaArea = Layout.getArea();
    if (AllocaArea.end() > Area.end())
      Area.setEnd(AllocaArea.end());
    if (Area.start() == 0 || AllocaArea.start() < Area.start())
      Area.setStart(AllocaArea.start());
  }
  
  DotStream << "</TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfFunction{std::move(IDString),
                          std::move(DotString),
                          std::move(Area),
                          std::move(Ports)};
}

static
LayoutOfThread
doLayout(LayoutHandler const &Handler,
         seec::cm::ThreadState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Generate the identifier for this subgraph.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "thread_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  std::vector<NodeInfo> FunctionNodes;
  
  DotStream << "subgraph " << IDString << " {\n";
  
  // Layout all functions.
  std::vector<LayoutOfFunction> FunctionLayouts;
  
  for (auto const &FunctionState : State.getCallStack()) {
    FunctionLayouts.emplace_back(doLayout(Handler, FunctionState, Expansion));
    
    auto const &Layout = FunctionLayouts.back();
    
    DotStream << Layout.getDotString();
    
    FunctionNodes.emplace_back(Layout.getID(),
                               Layout.getArea(),
                               Layout.getPorts());
  }
  
  // Make all function nodes take an equal rank.
  DotStream << "{ rank=same; ";
  
  for (auto const &FunctionLayout : FunctionLayouts)
    DotStream << FunctionLayout.getID() << "; ";
  
  DotStream << "};\n}\n";
  DotStream.flush();
  
  return LayoutOfThread{std::move(DotString), std::move(FunctionNodes)};
}

static
LayoutOfGlobalVariable
doLayout(LayoutHandler const &Handler,
         seec::cm::GlobalVariable const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "global_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">"
            << "<TR><TD>"
            << State.getClangValueDecl()->getName()
            << "</TD>";
  
  MemoryArea Area;
  
  auto const Value = State.getValue();
  if (Value) {
    auto const MaybeLayout = Handler.doLayout(*Value);
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      auto const &Layout = MaybeLayout.get<LayoutOfValue>();
      DotStream << Layout.getDotString();
      Ports.addAllFrom(Layout.getPorts());
    }
    
    if (Value->isInMemory()) {
      Area = MemoryArea(Value->getAddress(),
                        Value->getTypeSizeInChars().getQuantity());
    }
  }
  
  DotStream << "</TR></TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfGlobalVariable{std::move(IDString),
                                std::move(DotString),
                                std::move(Area),
                                std::move(Ports)};
}

static
std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>
doLayout(LayoutHandler const &Handler,
         MemoryArea const &Area,
         Expansion const &Expansion)
{
  auto const Refs = Expansion.getReferencesOfArea(Area.start(), Area.end());
  
  if (Refs.empty())
    return std::make_pair(seec::Maybe<LayoutOfArea>(), Area);
  
  // TODO: Get the reference-picking algorithm from
  //       GraphGenerator::generate(ReferenceArea const &Area).
  
  return std::make_pair(Handler.doLayout(Area, *Refs[0]), Area);
}

static void renderEdges(llvm::raw_string_ostream &DotStream,
                        std::vector<NodeInfo> const &AllNodeInfo,
                        Expansion const &Expansion)
{
  // Layout all pointers.
  for (auto const &Pointer : Expansion.getAllPointers()) {
    if (!Pointer->isInMemory())
      continue;
    
    if (Pointer->getDereferenceIndexLimit() == 0)
      continue;
    
    auto const TailAddress = Pointer->getAddress();
    auto const TailIt =
      std::find_if(AllNodeInfo.begin(),
                   AllNodeInfo.end(),
                   [=] (NodeInfo const &NI) { return NI.getArea()
                                                       .contains(TailAddress);
                                            });
    
    if (TailIt == AllNodeInfo.end()) {
      llvm::errs() << "pointer: tail not found.\n";
      continue;
    }
    
    auto const HeadAddress = Pointer->getRawValue();
    auto const HeadIt =
      std::find_if(AllNodeInfo.begin(),
                   AllNodeInfo.end(),
                   [=] (NodeInfo const &NI) { return NI.getArea()
                                                       .contains(HeadAddress);
                                            });
    
    if (HeadIt == AllNodeInfo.end()) {
      llvm::errs() << "pointer: head not found.\n";
      continue;
    }
    
    auto const MaybeTailPort = TailIt->getPortForValue(*Pointer);
    
    auto const Pointee = Pointer->getDereferenced(0);
    auto const MaybeHeadPort = HeadIt->getPortForValue(*Pointee);
    
    // Accumulate all attributes.
    std::string EdgeAttributes;
    bool IsPunned = false;
    
    // Write the tail.
    DotStream << TailIt->getID();
    
    if (MaybeTailPort.assigned<ValuePort>()) {
      // Tail port was explicitly defined during layout.
      auto const &TailPort = MaybeTailPort.get<ValuePort>();
      
      if (!TailPort.getCustomPort().empty())
        DotStream << ':' << TailPort.getCustomPort();
      else if (TailPort.getEdgeEnd() == EdgeEndType::Standard)
        DotStream << ':'
                  << getStandardPortFor(*Pointer)
                  << ":c";
      
      EdgeAttributes += "tailclip=false;";
    }
    else {
      // The tail port wasn't found, we must consider it punned.
      llvm::errs() << "tail considered punned.\n";
      
      EdgeAttributes += "dir=both;arrowtail=odot;";
      
      IsPunned = true;
    }
    
    // Write the arrow.
    DotStream << " -> ";
    
    // Write the head.
    DotStream << HeadIt->getID();
    
    if (MaybeHeadPort.assigned<ValuePort>()) {
      // Tail port was explicitly defined during layout.
      auto const &HeadPort = MaybeHeadPort.get<ValuePort>();
      
      if (!HeadPort.getCustomPort().empty())
        DotStream << ':' << HeadPort.getCustomPort();
      else if (HeadPort.getEdgeEnd() == EdgeEndType::Standard)
        DotStream << ':'
                  << getStandardPortFor(*Pointee)
                  << ":nw";
    }
    else {
      // The tail port wasn't found, we must consider it punned.
      llvm::errs() << "head considered punned.\n";
      
      EdgeAttributes += "arrowhead=odot;";
      
      IsPunned = true;
    }
    
    if (IsPunned)
      EdgeAttributes += "style=dashed;";
    
    // Write attributes.
    if (!EdgeAttributes.empty()) {
      EdgeAttributes.pop_back();
      DotStream << " [" << EdgeAttributes << "]";
    }
    
    DotStream << ";\n";
  }
}

static
LayoutOfProcess
doLayout(LayoutHandler const &Handler,
         seec::cm::ProcessState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Create tasks to generate global variable layouts.
  std::vector<std::future<LayoutOfGlobalVariable>> GlobalVariableLayouts;
  
  auto const Globals = State.getGlobalVariables();
  for (auto It = Globals.begin(), End = Globals.end(); It != End; ++It) {
    GlobalVariableLayouts.emplace_back(
      std::async( [&, It] () {
                    return doLayout(Handler, *It, Expansion);
                  } ));
  }
  
  // Create tasks to generate thread layouts.
  auto const ThreadCount = State.getThreadCount();
  
  std::vector<std::future<LayoutOfThread>> ThreadLayouts;
  ThreadLayouts.reserve(ThreadCount);
  
  for (std::size_t i = 0; i < ThreadCount; ++i) {
    ThreadLayouts.emplace_back(
      std::async( [&, i] () {
                    return doLayout(Handler, State.getThread(i), Expansion);
                  } ));
  }
  
  // Create tasks to generate malloc area layouts.
  std::vector<std::future<std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>>>
    MallocLayouts;
  
  for (auto const &Malloc : State.getDynamicMemoryAllocations()) {
    auto const Area = seec::MemoryArea(Malloc.getAddress(), Malloc.getSize());
    
    MallocLayouts.emplace_back(
      std::async([&, Area] () { return doLayout(Handler, Area, Expansion); } ));
  }
  
  // Create tasks to generate known memory area layouts.
  std::vector<std::future<std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>>>
    KnownAreaLayouts;
  
  for (auto const &Known : State.getUnmappedProcessState().getKnownMemory()) {
    auto const Area = seec::MemoryArea(Known.Begin,
                                       Known.End - Known.Begin,
                                       Known.Value);
    
    KnownAreaLayouts.emplace_back(
      std::async([&, Area] () { return doLayout(Handler, Area, Expansion); } ));
  }
  
  // Retrieve results and combine layouts.
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  std::vector<NodeInfo> AllNodeInfo;
  
  DotStream << "digraph Process {\n"
            << "node [shape=plaintext];\n"
            << "rankdir=LR;\n";
  
  for (auto &GlobalFuture : GlobalVariableLayouts) {
    auto const Layout = GlobalFuture.get();

    DotStream << Layout.getDotString();
    
    AllNodeInfo.emplace_back(Layout.getID(),
                             Layout.getArea(),
                             Layout.getPorts());
  }
  
  for (auto &ThreadFuture : ThreadLayouts) {
    auto const Layout = ThreadFuture.get();
    
    DotStream << Layout.getDotString();
    
    AllNodeInfo.insert(AllNodeInfo.end(),
                       Layout.getNodes().begin(),
                       Layout.getNodes().end());
  }
  
  for (auto &MallocFuture : MallocLayouts) {
    auto const Result = MallocFuture.get();
    
    auto const &MaybeLayout = Result.first;
    if (!MaybeLayout.assigned<LayoutOfArea>())
      continue;
    
    auto const &Layout = MaybeLayout.get<LayoutOfArea>();
    
    DotStream << Layout.getDotString();
    
    AllNodeInfo.emplace_back(Layout.getID(),
                             Result.second,
                             Layout.getPorts());
  }
  
  for (auto &KnownAreaFuture : KnownAreaLayouts) {
    auto const Result = KnownAreaFuture.get();
    
    auto const &MaybeLayout = Result.first;
    if (!MaybeLayout.assigned<LayoutOfArea>())
      continue;
    
    auto const &Layout = MaybeLayout.get<LayoutOfArea>();
    
    DotStream << Layout.getDotString();
    
    AllNodeInfo.emplace_back(Layout.getID(),
                             Result.second,
                             Layout.getPorts());
  }
  
  // Render all of the pointers.
  renderEdges(DotStream, AllNodeInfo, Expansion);
  
  DotStream << "}\n"; // Close the digraph.
  DotStream.flush();
  
  return LayoutOfProcess{std::move(DotString)};
}


//===----------------------------------------------------------------------===//
// LayoutHandler - Layout Engine Handling
//===----------------------------------------------------------------------===//

void LayoutHandler::addBuiltinLayoutEngines() {
  // LayoutEngineForValue:
  addLayoutEngine(seec::makeUnique<LEVStandard>(*this));
  
  // LayoutEngineForArea:
  addLayoutEngine(seec::makeUnique<LEAStandard>(*this));
}

void
LayoutHandler::addLayoutEngine(std::unique_ptr<LayoutEngineForValue> Engine) {
  ValueEngines.emplace_back(std::move(Engine));
}

void
LayoutHandler::addLayoutEngine(std::unique_ptr<LayoutEngineForArea> Engine) {
  AreaEngines.emplace_back(std::move(Engine));
}


//===----------------------------------------------------------------------===//
// LayoutHandler - Layout Creation
//===----------------------------------------------------------------------===//

seec::Maybe<LayoutOfValue>
LayoutHandler::doLayout(seec::cm::Value const &State) const
{
  for (auto const &EnginePtr : ValueEngines)
    if (EnginePtr->canLayout(State))
      return EnginePtr->doLayout(State);
  
  return seec::Maybe<LayoutOfValue>();
}

seec::Maybe<LayoutOfArea>
LayoutHandler::doLayout(seec::MemoryArea const &Area,
                        seec::cm::ValueOfPointer const &Reference) const
{
  for (auto const &EnginePtr : AreaEngines)
    if (EnginePtr->canLayout(Area, Reference))
      return EnginePtr->doLayout(Area, Reference);
  
  return seec::Maybe<LayoutOfArea>();
}

LayoutOfProcess
LayoutHandler::doLayout(seec::cm::ProcessState const &State) const
{
  return seec::cm::graph::doLayout(*this, State, Expansion::from(State));
}


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec
