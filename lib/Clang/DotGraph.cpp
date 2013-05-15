//===- lib/Clang/DotGraph.cpp ---------------------------------------------===//
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


#include "seec/Clang/DotGraph.hpp"
#include "seec/Clang/MappedAllocaState.hpp"
#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/Printing.hpp"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <set>
#include <string>


namespace seec {

namespace cm {


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


/// \brief Generate a port identifier for the given Value.
///
static std::string getPortFor(seec::cm::Value const &Value)
{
  std::string Port = "valueat";
  
  {
    // Use the Value's address rather than the runtime address, because it also
    // specifies the type of the value (this should allow us to handle, e.g.
    // pointers to different members of a union that occupy the same address).
    llvm::raw_string_ostream Stream {Port};
    Stream << reinterpret_cast<uintptr_t>(&Value);
  }
  
  return Port;
}


/// \brief Represents a completed edge.
///
class PointerResolved {
public:
  enum class EdgeKind {
    Normal,
    TypePunned
  };

private:
  std::string Start;
  
  std::string End;
  
  EdgeKind Kind;
  
public:
  PointerResolved(std::string WithStart,
                  std::string WithEnd,
                  EdgeKind WithKind)
  : Start(std::move(WithStart)),
    End(std::move(WithEnd)),
    Kind(WithKind)
  {}
  
  PointerResolved(std::string WithStartNode,
                  std::string WithStartPort,
                  std::string WithEndNode,
                  std::string WithEndPort,
                  EdgeKind WithKind)
  : Start(WithStartNode + ":" + WithStartPort),
    End(WithEndNode + ":" + WithEndPort),
    Kind(WithKind)
  {}
  
  std::string const &getStart() const { return Start; }
  
  std::string const &getEnd() const { return End; }
  
  EdgeKind getKind() const { return Kind; }
};


/// \brief Represents a pointer that is waiting to be dereferenced.
///
class PointerToExpand {
  std::shared_ptr<seec::cm::ValueOfPointer const> Value;
  
  std::string ContainerNode;
  
public:
  PointerToExpand(std::shared_ptr<seec::cm::ValueOfPointer const> WithValue,
                  std::string InContainerNode)
  : Value(std::move(WithValue)),
    ContainerNode(std::move(InContainerNode))
  {}
  
  std::shared_ptr<seec::cm::ValueOfPointer const> const &getValue() const {
    return Value;
  }
  
  std::string const &getContainerNode() const { return ContainerNode; }
  
  /// \brief Allow comparison so that we can put PointerToExpand in a set.
  ///
  /// Comparison redirects to a comparison of the internal Value members. There
  /// should never be two PointerToExpand nodes with the same Value but
  /// different ContainerNode, based on the method of graph generation we use.
  ///
  bool operator<(PointerToExpand const &RHS) const {
    return Value < RHS.Value;
  }
};


/// \brief Represents an area of known or allocated memory.
///
class ReferenceArea {
  /// The area of memory.
  seec::MemoryArea Area;
  
  /// Encountered references.
  std::set<PointerToExpand> References;
  
public:
  /// \brief Constructor.
  ///
  ReferenceArea(seec::MemoryArea ForArea)
  : Area(ForArea),
    References()
  {}
  
  /// \brief Check if this area is referenced by a pointer.
  ///
  /// pre: Ptr.isCompletelyInitialized() == true
  ///
  bool isReferencedBy(seec::cm::ValueOfPointer const &Ptr) const {
    return Area.contains(Ptr.getRawValue());
  }
  
  /// \brief Add a pointer that references this area.
  ///
  void addReference(PointerToExpand Ptr) {
    References.insert(std::move(Ptr));
  }
  
  /// \brief Get the memory area.
  ///
  decltype(Area) const &getArea() const { return Area; }
  
  /// \brief Get all references to this area.
  ///
  decltype(References) const &getReferences() const { return References; }
};


/// \brief Create graphs of SeeC-Clang Mapped process states.
///
class GraphGenerator {
  /// The graph in dot format.
  std::string DotGraph;
  
  /// Stream for writing to the graph.
  llvm::raw_string_ostream Stream;
  
  /// Handles indentation for the graph.
  seec::util::IndentationGuide Indent;
  
  /// Values that have completed layout.
  std::set<std::shared_ptr<seec::cm::Value const>> ValuesCompleted;
  
  /// Pointers that are waiting to be expanded. The key is the pointee address.
  std::map<uintptr_t, PointerToExpand> PointersToExpand;
  
  /// Pointers that have been expanded and just need to be inserted as edges.
  std::vector<PointerResolved> PointersToLayout;
  
  /// Areas that are not yet referenced (and thus need expanding, layout).
  std::vector<ReferenceArea> AreasToReference;
  
  /// \brief Helper that writes a single line, with indentation, to the graph.
  ///
  void writeln(llvm::Twine Text) {
    Stream << Indent.getString() << Text << "\n";
  }
  
  void generate(std::shared_ptr<seec::cm::Value const> Value,
                std::string const &InNode);
  
  void generate(std::shared_ptr<seec::cm::ValueOfPointer const> Value,
                std::string const &InNode);
  
  void generate(seec::cm::AllocaState const &State,
                std::string const &InNode);
  
  /// \brief Expand and layout a function.
  /// \return The node identifier for the function.
  ///
  std::string generate(seec::cm::FunctionState const &State);
  
  void generate(seec::cm::ThreadState const &State);
  
  void generate(seec::cm::GlobalVariable const &State);
  
  void generate(ReferenceArea const &Area);
  
  // No copying or moving.
  GraphGenerator(GraphGenerator const &) = delete;
  GraphGenerator &operator=(GraphGenerator const &) = delete;
  GraphGenerator(GraphGenerator &&) = delete;
  GraphGenerator &operator=(GraphGenerator &&) = delete;
  
public:
  /// \brief Default constructor.
  ///
  GraphGenerator()
  : DotGraph{},
    Stream{DotGraph},
    Indent{" "},
    ValuesCompleted{},
    PointersToExpand{},
    PointersToLayout{},
    AreasToReference{}
  {}
  
  void generate(seec::cm::ProcessState const &State);
  
  std::string takeDotGraph();
};

void GraphGenerator::generate(std::shared_ptr<seec::cm::Value const> Value,
                              std::string const &InNode)
{
  if (!Value)
    return;
  
  switch (Value->getKind()) {
    case seec::cm::Value::Kind::Basic:
      Stream << EscapeForHTML(Value->getValueAsStringFull());
      break;
    
    case seec::cm::Value::Kind::Array:
      {
        auto const &Array = *llvm::cast<seec::cm::ValueOfArray>(Value.get());
        unsigned const ChildCount = Array.getChildCount();
        
        Stream << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
        
        for (unsigned i = 0; i < ChildCount; ++i) {
          auto const ChildValue = Array.getChildAt(i);
          
          Stream << "<TR><TD>&#91;"
                 << i
                 << "&#93;</TD><TD PORT=\""
                 << getPortFor(*ChildValue)
                 << "\">";
          
          generate(ChildValue, InNode);
          
          Stream << "</TD></TR>";
        }
        
        Stream << "</TABLE>";
      }
      break;
    
    case seec::cm::Value::Kind::Record:
      {
        auto const &Record = *llvm::cast<seec::cm::ValueOfRecord>(Value.get());
        unsigned const ChildCount = Record.getChildCount();
        
        Stream << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
        
        for (unsigned i = 0; i < ChildCount; ++i) {
          auto const ChildValue = Record.getChildAt(i);
          
          Stream << "<TR><TD>";
          
          // Write identifier.
          auto const ChildField = Record.getChildField(i);
          if (ChildField)
            Stream << "." << Record.getChildField(i)->getName();
          else
            Stream << " "; // TODO: Localize
          
          Stream << "</TD><TD PORT=\""
                 << getPortFor(*ChildValue)
                 << "\">";
          
          // Write value.
          generate(ChildValue, InNode);
          
          Stream << "</TD></TR>";
        }
        
        Stream << "</TABLE>";
      }
      break;
    
    case seec::cm::Value::Kind::Pointer:
      {
        auto const Ptr =
          std::static_pointer_cast<seec::cm::ValueOfPointer const>(Value);
        
        if (Ptr->isCompletelyInitialized() && Ptr->getRawValue()) {
          if (Ptr->getDereferenceIndexLimit() > 0) {
            Stream << " ";
            PointersToExpand.insert(std::make_pair(Ptr->getRawValue(),
                                                   PointerToExpand{Ptr,
                                                                   InNode}));
          }
          else {
            Stream << "???"; // TODO: Localize.
          }
        }
        else {
          Stream << "NULL";
        }
      }
      break;
  }
}

///
/// pre: Value->isCompletelyInitialized() == true
/// pre: Value->getRawValue() != 0
/// pre: Value->getDereferenceIndexLimit() > 0
///
void
GraphGenerator::generate(std::shared_ptr<seec::cm::ValueOfPointer const> Value,
                         std::string const &InNode)
{
  auto const Limit = Value->getDereferenceIndexLimit();
  
  // If there's only one pointee element then we use a simple layout. If there
  // are more then we will use an array-like layout.
  if (Limit == 1) {
    auto const Pointee = Value->getDereferenced(0);
    
    Stream << Indent.getString()
           << "<TR><TD PORT=\""
           << getPortFor(*Pointee)
           << "\">";
    
    generate(Pointee, InNode);
    
    Stream << "</TD></TR>\n";
  }
  else {
    for (unsigned i = 0; i < Limit; ++i) {
      auto const Pointee = Value->getDereferenced(i);
      
      writeln("<TR>");
      Indent.indent();
      
      Stream << Indent.getString() << "<TD>&#91;" << i << "&#93;</TD>\n";
      
      Stream << Indent.getString()
             << "<TD PORT=\""
             << getPortFor(*Pointee)
             << "\">";
      
      generate(Pointee, InNode);
      
      Stream << "</TD>\n";
      
      Indent.unindent();
      writeln("</TR>");
    }
  }
}

void GraphGenerator::generate(seec::cm::AllocaState const &State,
                              std::string const &InNode)
{
  auto const Value = State.getValue();
  
  writeln("<TR>");
  Indent.indent();
  
  writeln("<TD>" + State.getDecl()->getNameAsString() + "</TD>");
  
  Stream << Indent.getString()
         << "<TD ALIGN=\"LEFT\" PORT=\""
         << getPortFor(*Value)
         << "\">";
  generate(Value, InNode);
  Stream << "</TD>\n";
  
  Indent.unindent();
  writeln("</TR>");
}

std::string GraphGenerator::generate(seec::cm::FunctionState const &State)
{
  std::string NodeIdentifier;
  {
    llvm::raw_string_ostream NodeIdentifierStream {NodeIdentifier};
    NodeIdentifierStream << "function" << reinterpret_cast<uintptr_t>(&State);
  }
  
  writeln(NodeIdentifier + " [ label = <");
  Indent.indent();
  
  writeln("<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">");
  Indent.indent();
  
  writeln("<TR><TD COLSPAN=\"2\">" + State.getNameAsString() + "</TD></TR>");
  
  for (auto const &Parameter : State.getParameters())
    generate(Parameter, NodeIdentifier);
  
  for (auto const &Local : State.getLocals())
    generate(Local, NodeIdentifier);
  
  Indent.unindent();
  writeln("</TABLE>");
  
  Indent.unindent();
  writeln("> ];");
  
  return NodeIdentifier;
}

void GraphGenerator::generate(seec::cm::ThreadState const &State)
{
  auto const ID = State.getUnmappedState().getTrace().getThreadID();
  
  // Create a subgraph for this thread.
  Stream << Indent.getString()
         << "subgraph thread" << ID << " {\n";
  Indent.indent();
  
  // Layout all functions.
  std::vector<std::string> FunctionIdentifiers;
  
  for (auto const &FunctionState : State.getCallStack())
    FunctionIdentifiers.emplace_back(generate(FunctionState));
  
  // Make all function nodes take an equal rank.
  Stream << Indent.getString() << "{ rank=same; ";
  
  for (auto const &Identifier : FunctionIdentifiers)
    Stream << Identifier << "; ";
  
  Stream << "};\n";
  
  // Finish the subgraph.
  Indent.unindent();
  writeln("}");
}

void GraphGenerator::generate(seec::cm::GlobalVariable const &State)
{
  std::string NodeIdentifier = "global";
  {
    llvm::raw_string_ostream IdentifierStream { NodeIdentifier };
    IdentifierStream << State.getAddress();
  }
  
  writeln(NodeIdentifier + " [ label = <");
  Indent.indent();
  
  writeln("<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">");
  Indent.indent();
  
  writeln("<TR>");
  Indent.indent();
  
  writeln("<TD>" + State.getClangValueDecl()->getName() + "</TD>");
  
  auto const Value = State.getValue();
  
  Stream << Indent.getString() << "<TD PORT=\"" << getPortFor(*Value) << "\">";
  generate(Value, NodeIdentifier);
  Stream << "</TD>\n";
  
  Indent.unindent();
  writeln("</TR>");
  
  Indent.unindent();
  writeln("</TABLE>");
  
  Indent.unindent();
  writeln("> ];");
}

void GraphGenerator::generate(ReferenceArea const &Area)
{
  llvm::errs() << "performing layout for bin.\n";
  
  std::string Identifier = "region";
  {
    llvm::raw_string_ostream IdentifierStream { Identifier };
    IdentifierStream << Area.getArea().start();
  }
  
  writeln(Identifier + " [ label = <");
  Indent.indent();
  
  writeln("<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\""
          " PORT=\"root\">");
  Indent.indent();
  
  // Decide which pointer to expand (if any). The preference of type is:
  // (non-char non-void) > char > void.
  auto const &References = Area.getReferences();
  
  if (References.size() == 1) {
    // Layout the single reference.
    auto const Pointer = References.begin()->getValue();
    
    generate(Pointer, Identifier);
    
    PointersToLayout.emplace_back(References.begin()->getContainerNode(),
                                  getPortFor(*Pointer),
                                  Identifier,
                                  "root",
                                  PointerResolved::EdgeKind::Normal);
  }
  else {
    // Find all non-char non-void references.
    llvm::errs() << "  multiple reference (not yet supported).\n";
  }
  
  Indent.unindent();
  writeln("</TABLE>");
  
  Indent.unindent();
  writeln("> ];");
}

void GraphGenerator::generate(seec::cm::ProcessState const &State)
{
  // Setup the initial state.
  for (auto const &Malloc : State.getDynamicMemoryAllocations()) {
    AreasToReference.emplace_back(seec::MemoryArea{Malloc.getAddress(),
                                                   Malloc.getSize()});
  }
  
  // TODO: Add Known memory regions.
  for (auto const &Known : State.getUnmappedProcessState().getKnownMemory()) {
    AreasToReference.emplace_back(seec::MemoryArea{Known.Begin,
                                                   Known.End - Known.Begin,
                                                   Known.Value});
  }
  
  // Write the top-level graph information.
  writeln("digraph Process {");
  Indent.indent();
  
  writeln("node [shape=plaintext];");
  writeln("rankdir=LR;");
  
  // Generate all thread states. This will expand and layout allocas, but will
  // not dereference pointers - they will be added to PointersToExpand.
  for (std::size_t i = 0; i < State.getThreadCount(); ++i)
    generate(State.getThread(i));
  
  // Generate all global variable states. This will expand and layout, but will
  // not dereference pointers - they will be added to PointersToExpand.
  for (auto const &Global : State.getGlobalVariables())
    generate(Global);
  
  // Now we will expand pointers. We will also expand and layout dereferences
  // when required.
  while (!PointersToExpand.empty()) {
    // Assign pointers to known/dynamic regions that own them.
    for (auto &Bin : AreasToReference) {
      auto const &Area = Bin.getArea();
      
      auto const RangeStart = PointersToExpand.lower_bound(Area.start());
      auto const RangeEnd = PointersToExpand.lower_bound(Area.end());
      
      for (auto It = RangeStart; It != RangeEnd; ++It)
        Bin.addReference(std::move(It->second));
      
      PointersToExpand.erase(RangeStart, RangeEnd);
    }
    
    // Create edges for the remaining pointers.
    for (auto const &PtrPair : PointersToExpand) {
      // TODO: First determine if this pointer refers to an already-expanded
      //       Value.
      
      // TODO: Otherwise, we have an unmatched reference.
    }
    
    PointersToExpand.clear();
    
    auto const RefdEnd = AreasToReference.end();
    
    // Move all empty reference areas to the beginning (and thus all non-empty
    // areas to the end).
    auto const RefdIt = std::partition(AreasToReference.begin(), RefdEnd,
                                       [] (ReferenceArea const &Bin) {
                                         return Bin.getReferences().empty();
                                       });
    
    // Layout all the referenced areas.
    std::for_each(RefdIt, RefdEnd,
                  [this] (ReferenceArea const &Bin) { this->generate(Bin); });
    
    // Remove all areas that have been laid out.
    AreasToReference.erase(RefdIt, RefdEnd);
  }
  
  // Write all edges.
  writeln("edge[tailclip=false];");
  
  for (auto const &Edge : PointersToLayout) {
    Stream << Indent.getString()
           << Edge.getStart()
           << ":c -> "
           << Edge.getEnd()
           << ":nw;\n";
  }
  
  Indent.unindent();
  writeln("}");
}

std::string GraphGenerator::takeDotGraph()
{
  Stream.flush();
  
  std::string RetGraph = std::move(DotGraph);
  
  DotGraph.clear();
  ValuesCompleted.clear();
  PointersToExpand.clear();
  PointersToLayout.clear();
  AreasToReference.clear();
  
  return RetGraph;
}

/// \brief Write a graph in dot format describing a complete ProcessState.
///
void writeDotGraph(seec::cm::ProcessState const &State,
                   llvm::raw_ostream &Stream)
{
  GraphGenerator G;
  G.generate(State);
  Stream << G.takeDotGraph();
}


} // namespace cm

} // namespace seec
