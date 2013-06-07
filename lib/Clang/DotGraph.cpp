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
#include <functional>
#include <memory>
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


/// \brief Write the standard TD attributes for a Value.
///
static void writeAttributesForValue(llvm::raw_ostream &Stream,
                                    seec::cm::Value const &Value)
{
  auto const ValueAddr = reinterpret_cast<uintptr_t>(&Value);
  
  Stream << " PORT=\"" << getPortFor(Value) << "\""
         << " HREF=\"value:" << ValueAddr << "\"";
}


/// \brief Represents a graph node, which may contain Value objects.
///
class Node {
  std::string Identifier;
  
public:
  explicit Node(std::string WithIdentifier)
  : Identifier(std::move(WithIdentifier))
  {}
  
  std::string const &getIdentifier() const { return Identifier; }
};


/// \brief Represents a completed edge.
///
class PointerResolved {
public:
  enum class EdgeKind {
    Normal,
    TypePunned
  };

private:
  std::reference_wrapper<Node const> StartNode;
  
  std::string StartPort;
  
  std::reference_wrapper<Node const> EndNode;
  
  std::string EndPort;
  
  EdgeKind Kind;
  
public:
  PointerResolved(Node const &WithStartNode,
                  std::string WithStartPort,
                  Node const &WithEndNode,
                  std::string WithEndPort,
                  EdgeKind WithKind)
  : StartNode(WithStartNode),
    StartPort(std::move(WithStartPort)),
    EndNode(WithEndNode),
    EndPort(std::move(WithEndPort)),
    Kind(WithKind)
  {}
  
  void write(llvm::raw_ostream &Stream) const {
    Stream << StartNode.get().getIdentifier();
    
    if (!StartPort.empty())
      Stream << ":" << StartPort;
    
    Stream << ":c -> ";
    
    Stream << EndNode.get().getIdentifier();
    
    if (!EndPort.empty())
      Stream << ":" << EndPort;
    
    Stream << ":nw;";
  }
};


/// \brief Represents a pointer that is waiting to be dereferenced.
///
class PointerToExpand {
  std::shared_ptr<seec::cm::ValueOfPointer const> Value;
  
  std::reference_wrapper<Node const> ContainerNode;
  
public:
  PointerToExpand(std::shared_ptr<seec::cm::ValueOfPointer const> WithValue,
                  Node const &InContainerNode)
  : Value(std::move(WithValue)),
    ContainerNode(InContainerNode)
  {}
  
  std::shared_ptr<seec::cm::ValueOfPointer const> const &getValue() const {
    return Value;
  }
  
  Node const &getContainerNode() const { return ContainerNode; }
  
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
  
  /// \brief Copy constructor.
  ///
  ReferenceArea(ReferenceArea const &Other)
  : Area(Other.Area),
    References(Other.References)
  {}
  
  /// \brief Move constructor.
  ///
  ReferenceArea(ReferenceArea &&Other)
  : Area(std::move(Other.Area)),
    References(std::move(Other.References))
  {}
  
  /// \brief Copy assignment.
  ///
  ReferenceArea &operator=(ReferenceArea const &RHS) = default;
  
  /// \brief Move assignment.
  ///
  ReferenceArea &operator=(ReferenceArea &&RHS) {
    Area = std::move(RHS.Area);
    References = std::move(RHS.References);
    return *this;
  }
  
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
  
  /// List of all nodes.
  std::vector<std::unique_ptr<Node>> Nodes;
  
  /// Lookup from Value to containing node.
  std::map<std::shared_ptr<seec::cm::Value const>,
           std::reference_wrapper<Node const>>
    ValuesCompleted;
  
  /// Pointers that are waiting to be expanded. The key is the pointee address.
  std::multimap<uintptr_t, PointerToExpand> PointersToExpand;
  
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
                Node const &InNode);
  
  void generate(std::shared_ptr<seec::cm::ValueOfPointer const> Value,
                Node const &InNode);
  
  void generate(seec::cm::AllocaState const &State,
                Node const &InNode);
  
  /// \brief Expand and layout a function.
  /// \return The node identifier for the function.
  ///
  Node const &generate(seec::cm::FunctionState const &State);
  
  void generate(seec::cm::ThreadState const &State);
  
  void generate(seec::cm::GlobalVariable const &State);
  
  void layoutDereferences(PointerToExpand const &Pointer,
                          Node const &InNode);
  
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
                              Node const &InNode)
{
  if (!Value)
    return;
  
  ValuesCompleted.insert(std::make_pair(Value, std::cref(InNode)));
  
  switch (Value->getKind()) {
    case seec::cm::Value::Kind::Basic:
      {
        auto const Initialized = Value->isCompletelyInitialized();
        
        Stream << "<TD";
        writeAttributesForValue(Stream, *Value);
        if (!Initialized)
          Stream << " BGCOLOR=\"grey\"";
        Stream << ">";
        
        if (Initialized)
          Stream << EscapeForHTML(Value->getValueAsStringFull());
        
        Stream << "</TD>";
      }
      break;
    
    case seec::cm::Value::Kind::Array:
      {
        auto const &Array = *llvm::cast<seec::cm::ValueOfArray>(Value.get());
        unsigned const ChildCount = Array.getChildCount();
        
        Stream << "<TD";
        writeAttributesForValue(Stream, *Value);
        Stream << ">"
               << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
        
        for (unsigned i = 0; i < ChildCount; ++i) {
          auto const ChildValue = Array.getChildAt(i);
          
          Stream << "<TR><TD HREF=\"element:"
                 << reinterpret_cast<uintptr_t>(&Array) << "," << i
                 << "\" BGCOLOR=\"white\">&#91;"
                 << i
                 << "&#93;</TD>";
          
          generate(ChildValue, InNode);
          
          Stream << "</TR>";
        }
        
        Stream << "</TABLE></TD>";
      }
      break;
    
    case seec::cm::Value::Kind::Record:
      {
        auto const &Record = *llvm::cast<seec::cm::ValueOfRecord>(Value.get());
        unsigned const ChildCount = Record.getChildCount();
        
        Stream << "<TD";
        writeAttributesForValue(Stream, *Value);
        Stream << ">"
               << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
        
        for (unsigned i = 0; i < ChildCount; ++i) {
          auto const ChildValue = Record.getChildAt(i);
          
          Stream << "<TR><TD HREF=\"member:"
                 << reinterpret_cast<uintptr_t>(&Record) << "," << i
                 << " BGCOLOR=\"white\">";
          
          // Write identifier.
          auto const ChildField = Record.getChildField(i);
          if (ChildField)
            Stream << "." << Record.getChildField(i)->getName();
          else
            Stream << " "; // TODO: Localize
          
          Stream << "</TD>";
          
          // Write value.
          generate(ChildValue, InNode);
          
          Stream << "</TR>";
        }
        
        Stream << "</TABLE></TD>";
      }
      break;
    
    case seec::cm::Value::Kind::Pointer:
      {
        auto const Ptr =
          std::static_pointer_cast<seec::cm::ValueOfPointer const>(Value);
        
        if (!Ptr->isCompletelyInitialized()) {
          // An uninitialized pointer.
          Stream << "<TD";
          writeAttributesForValue(Stream, *Value);
          Stream << " BGCOLOR=\"grey\">?</TD>";
        }
        else if (!Ptr->getRawValue()) {
          // A NULL pointer.
          Stream << "<TD";
          writeAttributesForValue(Stream, *Value);
          Stream << " BGCOLOR=\"white\">NULL</TD>";
        }
        else if (Ptr->getDereferenceIndexLimit() == 0) {
          // Probably an invalid pointer.
          Stream << "<TD";
          writeAttributesForValue(Stream, *Value);
          Stream << " BGCOLOR=\"red\">!</TD>";
        }
        else {
          // A valid pointer with at least one dereference.
          Stream << "<TD";
          writeAttributesForValue(Stream, *Value);
          Stream << " BGCOLOR=\"white\"> </TD>";
          
          PointersToExpand.insert(std::make_pair(Ptr->getRawValue(),
                                                 PointerToExpand{Ptr,
                                                                 InNode}));
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
                         Node const &InNode)
{
  auto const Limit = Value->getDereferenceIndexLimit();
  
  // If there's only one pointee element then we use a simple layout. If there
  // are more then we will use an array-like layout.
  if (Limit == 1) {
    auto const Pointee = Value->getDereferenced(0);
    
    Stream << Indent.getString()
           << "<TR>";
    
    generate(Pointee, InNode);
    
    Stream << "</TR>\n";
  }
  else {
    for (unsigned i = 0; i < Limit; ++i) {
      auto const Pointee = Value->getDereferenced(i);
      
      writeln("<TR>");
      Indent.indent();
      
      Stream << Indent.getString()
             << "<TD HREF=\"dereference:"
             << reinterpret_cast<uintptr_t>(&*Value) << "," << i << "\""
             << " BGCOLOR=\"white\">&#91;"
             << i << "&#93;</TD>\n";
      
      Stream << Indent.getString();
      generate(Pointee, InNode);
      Stream << "\n";
      
      Indent.unindent();
      writeln("</TR>");
    }
  }
}

void GraphGenerator::generate(seec::cm::AllocaState const &State,
                              Node const &InNode)
{
  auto const Value = State.getValue();
  
  writeln("<TR>");
  Indent.indent();
  
  writeln("<TD>" + State.getDecl()->getNameAsString() + "</TD>");
  
  Stream << Indent.getString();
  generate(Value, InNode);
  Stream << "\n";
  
  Indent.unindent();
  writeln("</TR>");
}

Node const &GraphGenerator::generate(seec::cm::FunctionState const &State)
{
  {
    std::string NodeIdentifier;
    
    {
      llvm::raw_string_ostream NodeIdentifierStream {NodeIdentifier};
      NodeIdentifierStream << "function" << reinterpret_cast<uintptr_t>(&State);
    }
  
    writeln(NodeIdentifier + " [ label = <");
    Indent.indent();
  
    Nodes.emplace_back(new Node{std::move(NodeIdentifier)});
  }
  
  auto const &ThisNode = *Nodes.back();
  
  writeln("<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">");
  Indent.indent();
  
  writeln("<TR><TD COLSPAN=\"2\">" + State.getNameAsString() + "</TD></TR>");
  
  for (auto const &Parameter : State.getParameters())
    generate(Parameter, ThisNode);
  
  for (auto const &Local : State.getLocals())
    generate(Local, ThisNode);
  
  Indent.unindent();
  writeln("</TABLE>");
  
  Indent.unindent();
  writeln("> ];");
  
  return ThisNode;
}

void GraphGenerator::generate(seec::cm::ThreadState const &State)
{
  auto const ID = State.getUnmappedState().getTrace().getThreadID();
  
  // Create a subgraph for this thread.
  Stream << Indent.getString()
         << "subgraph thread" << ID << " {\n";
  Indent.indent();
  
  // Layout all functions.
  std::vector<std::reference_wrapper<Node const>> FunctionNodes;
  
  for (auto const &FunctionState : State.getCallStack())
    FunctionNodes.emplace_back(generate(FunctionState));
  
  // Make all function nodes take an equal rank.
  Stream << Indent.getString() << "{ rank=same; ";
  
  for (auto const &FunctionNode : FunctionNodes)
    Stream << FunctionNode.get().getIdentifier() << "; ";
  
  Stream << "};\n";
  
  // Finish the subgraph.
  Indent.unindent();
  writeln("}");
}

void GraphGenerator::generate(seec::cm::GlobalVariable const &State)
{
  {
    std::string NodeIdentifier = "global";
    {
      llvm::raw_string_ostream IdentifierStream { NodeIdentifier };
      IdentifierStream << State.getAddress();
    }
    
    writeln(NodeIdentifier + " [ label = <");
    Indent.indent();
    
    Nodes.emplace_back(new Node{std::move(NodeIdentifier)});
  }
  
  auto const &ThisNode = *Nodes.back();
  
  writeln("<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">");
  Indent.indent();
  
  writeln("<TR>");
  Indent.indent();
  
  writeln("<TD>" + State.getClangValueDecl()->getName() + "</TD>");
  
  auto const Value = State.getValue();
  
  Stream << Indent.getString();
  generate(Value, ThisNode);
  Stream << "\n";
  
  Indent.unindent();
  writeln("</TR>");
  
  Indent.unindent();
  writeln("</TABLE>");
  
  Indent.unindent();
  writeln("> ];");
}

static bool
isChildOfAnyDereference(seec::cm::Value const &Child,
                        seec::cm::ValueOfPointer const &ParentReference)
{
  auto const Limit = ParentReference.getDereferenceIndexLimit();
  
  for (unsigned i = 0; i < Limit; ++i) {
    auto const &Deref = *ParentReference.getDereferenced(i);
    
    if (&Child == &Deref || isContainedChild(Child, Deref))
      return true;
  }
  
  return false;
}

void GraphGenerator::layoutDereferences(PointerToExpand const &Pointer,
                                        Node const &InNode)
{
  generate(Pointer.getValue(), InNode);
  
  PointersToLayout.emplace_back(Pointer.getContainerNode(),
                                getPortFor(*Pointer.getValue()),
                                InNode,
                                "root",
                                PointerResolved::EdgeKind::Normal);
}

void GraphGenerator::generate(ReferenceArea const &Area)
{
  llvm::errs() << "generating bin.\n";
  
  {
    std::string Identifier = "region";
    {
      llvm::raw_string_ostream IdentifierStream { Identifier };
      IdentifierStream << Area.getArea().start();
    }
    
    writeln(Identifier + " [ label = <");
    Indent.indent();
    
    Nodes.emplace_back(new Node{std::move(Identifier)});
  }
  
  auto const &ThisNode = *Nodes.back();
  
  writeln("<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\""
          " PORT=\"root\">");
  Indent.indent();
  
  // Decide which pointer to expand (if any). The preference of type is:
  // (non-char non-void) > char > void.
  std::vector<PointerToExpand> PointersPreferred;
  std::vector<PointerToExpand> PointersChar;
  std::vector<PointerToExpand> PointersVoid;
  std::vector<PointerToExpand> PointersAliased;
  std::vector<PointerToExpand> PointersTypePunned;
  
  for (auto const &Pointer : Area.getReferences()) {
    auto const CanType = Pointer.getValue()->getCanonicalType();
    auto const PtrType = llvm::cast< ::clang::PointerType >(CanType);
    auto const PointeeType = PtrType->getPointeeType();
    
    if (PointeeType->isVoidType())
      PointersVoid.emplace_back(Pointer);
    else if (PointeeType->isCharType())
      PointersChar.emplace_back(Pointer);
    else
      PointersPreferred.emplace_back(Pointer);
  }
  
  llvm::errs() << " pointers preferred: " << PointersPreferred.size() << "\n";
  llvm::errs() << " pointers to char  : " << PointersChar.size() << "\n";
  llvm::errs() << " pointers to void  : " << PointersVoid.size() << "\n";
  
  if (PointersPreferred.size() > 1) {
    // First sort the references in descending order of pointee size.
    std::sort(PointersPreferred.begin(), PointersPreferred.end(),
              [] (PointerToExpand const &LHS, PointerToExpand const &RHS) {
                return LHS.getValue()->getPointeeSize() >
                       RHS.getValue()->getPointeeSize();
              });
    
    for (auto i = PointersPreferred.size() - 1; i != 0; --i) {
      // Determine if PointersPreferred[i]'s zeroth dereference is a child of
      // any of [0,i)'s dereferences.
      auto const &Pointer = *PointersPreferred[i].getValue();
      auto const &Pointee = *Pointer.getDereferenced(0);
      
      auto const ItBegin = PointersPreferred.begin();
      auto const ItEnd = std::next(ItBegin, i);
      
      auto const IsChild =
        std::any_of(ItBegin, ItEnd,
                    [&] (PointerToExpand const &Parent) {
                      return isChildOfAnyDereference(Pointee,
                                                     *Parent.getValue());
                    });
      
      // If it is a child then we can resolve it after the parent is laid out.
      if (IsChild) {
        PointersAliased.emplace_back(std::move(*ItEnd));
        PointersPreferred.erase(ItEnd);
        
        llvm::errs() << " eliminated child.\n";
      }
    }
  }
  
  if (PointersPreferred.size() > 1) {
    // TODO: Attempt to layout as coexisting.
    llvm::errs() << "coexisting layout not yet implemented.\n";
  }
  else if (PointersPreferred.size() == 1) {
    // Layout the single reference.
    layoutDereferences(PointersPreferred.front(), ThisNode);
    PointersPreferred.clear();
  }
  else if (!PointersChar.empty()) {
    // Find the pointer with the lowest raw value, expand it, and let the rest
    // be resolved as aliases.
    auto const It =
      std::min_element(PointersChar.begin(), PointersChar.end(),
                        [] (PointerToExpand const &LHS,
                            PointerToExpand const &RHS)
                        {
                          return LHS.getValue()->getRawValue() <
                                 RHS.getValue()->getRawValue();
                        });
    
    layoutDereferences(*It, ThisNode);
    
    PointersChar.erase(It);
  }
  else if (!PointersVoid.empty()) {
    // TODO.
    
    // void pointers simply refer to an opaque block of memory, so there's no
    // aliasing problem. However, if the void pointers refer to different areas
    // within the block, then we may want to represent that in the layout.
    
    llvm::errs() << "void pointer layout not yet implemented.\n";
    
    PointersVoid.clear();
  }
  
  // We can simply push the unhandled pointers back to the process layout. If
  // they can't be resolved in the next pass, then they'll be discarded.
  for (auto const &Pointer : PointersChar)
    PointersToExpand.insert(std::make_pair(Pointer.getValue()->getRawValue(),
                                           std::move(Pointer)));
  
  for (auto const &Pointer : PointersVoid)
    PointersToExpand.insert(std::make_pair(Pointer.getValue()->getRawValue(),
                                           std::move(Pointer)));
  
  for (auto const &Pointer : PointersAliased)
    PointersToExpand.insert(std::make_pair(Pointer.getValue()->getRawValue(),
                                           std::move(Pointer)));
  
  for (auto const &Pointer : PointersTypePunned)
    PointersToExpand.insert(std::make_pair(Pointer.getValue()->getRawValue(),
                                           std::move(Pointer)));
  
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
  
  // Add Known memory regions.
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
    generate(*Global);
  
  // Now we will expand pointers. We will also expand and layout dereferences
  // when required.
  while (!PointersToExpand.empty()) {
    llvm::errs() << "have " << PointersToExpand.size()
                 << " pointers to expand.\n";
    
    // Assign pointers to known/dynamic regions that own them.
    for (auto &Bin : AreasToReference) {
      auto const &Area = Bin.getArea();
      
      auto const RangeStart = PointersToExpand.lower_bound(Area.start());
      auto const RangeEnd = PointersToExpand.lower_bound(Area.end());
      
      for (auto It = RangeStart; It != RangeEnd; ++It) {
        llvm::errs() << "adding pointer to bin.\n";
        Bin.addReference(std::move(It->second));
      }
      
      PointersToExpand.erase(RangeStart, RangeEnd);
    }
    
    // Create edges for the remaining pointers.
    for (auto const &PtrPair : PointersToExpand) {
      llvm::errs() << "resolving pointer.\n";
      
      // First determine if this pointer refers to an already-expanded Value.
      auto const Pointer = PtrPair.second.getValue();
      auto const Pointee = Pointer->getDereferenced(0);
      
      auto const It = ValuesCompleted.find(Pointee);
      
      if (It != ValuesCompleted.end()) {
        PointersToLayout.emplace_back(PtrPair.second.getContainerNode(),
                                      getPortFor(*Pointer),
                                      It->second.get(),
                                      getPortFor(*Pointee),
                                      PointerResolved::EdgeKind::Normal);
      }
      
      // TODO: Otherwise, we have an aliased reference.
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
    Stream << Indent.getString();
    Edge.write(Stream);
    Stream << "\n";
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
