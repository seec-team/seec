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
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include "llvm/ADT/StringRef.h"

#include <set>
#include <string>


namespace seec {

namespace cm {


std::string EscapeForHTML(llvm::StringRef String)
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


std::string getPortFor(seec::cm::Value const &Value)
{
  std::string Port = "addr";
  {
    // Use the Value's address rather than the runtime address, because it also
    // specifies the type of the value (this should allow us to handle, e.g.
    // pointers to different members of a union that occupy the same address).
    llvm::raw_string_ostream Stream {Port};
    Stream << reinterpret_cast<uintptr_t>(&Value);
  }
  return Port;
}


class Node {
  std::string Identifier;
  
  std::set<uintptr_t> Ports;
  
public:
  Node(std::string WithIdentifier)
  : Identifier(std::move(WithIdentifier))
  {}
  
  std::string const &getIdentifier() const { return Identifier; }
  
  void addPort(seec::cm::Value const &ForValue) {
    Ports.emplace(reinterpret_cast<uintptr_t>(&ForValue));
  }
  
  bool hasPort(seec::cm::Value const &ForValue) const {
    return Ports.count(reinterpret_cast<uintptr_t>(&ForValue));
  }
  
  /// pre: hasPort(ForValue) == true
  ///
  std::string getEdgeEndForPort(seec::cm::Value const &ForValue) const {
    return Identifier + ":" + getPortFor(ForValue);
  }
};


typedef std::pair<std::shared_ptr<seec::cm::Value const>,
                  std::shared_ptr<seec::cm::Value const>> Edge;


void WritePort(seec::cm::Value const &Value,
               llvm::raw_ostream &Stream,
               Node &InNode)
{
  Stream << " PORT=\"" << getPortFor(Value) << "\"";
  InNode.addPort(Value);
}


void WriteDot(std::shared_ptr<seec::cm::Value const> Value,
              llvm::raw_ostream &Stream,
              Node &InNode,
              std::set<Edge> &AllEdges)
{
  if (!Value)
    return;
  
  switch (Value->getKind()) {
    case seec::cm::Value::Kind::Basic:
      {
        Stream << EscapeForHTML(Value->getValueAsStringFull());
      }
      break;
    
    case seec::cm::Value::Kind::Array:
      {
        auto const Array = llvm::cast<seec::cm::ValueOfArray>(Value.get());
        unsigned const ChildCount = Array->getChildCount();
        
        Stream << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
        
        for (unsigned i = 0; i < ChildCount; ++i) {
          auto const ChildValue = Array->getChildAt(i);
          
          Stream << "<TR>";
          Stream << "<TD";
          WritePort(*ChildValue, Stream, InNode);
          Stream << ">&#91;" << i << "&#93;</TD>";
          
          Stream << "<TD>";
          WriteDot(ChildValue, Stream, InNode, AllEdges);
          Stream << "</TD>";
          Stream << "</TR>";
        }
        
        Stream << "</TABLE>";
      }
      break;
    
    case seec::cm::Value::Kind::Record:
      {
        auto const Record = llvm::cast<seec::cm::ValueOfRecord>(Value.get());
        unsigned const ChildCount = Record->getChildCount();
        
        Stream << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
        
        for (unsigned i = 0; i < ChildCount; ++i) {
          auto const ChildValue = Record->getChildAt(i);
          
          Stream << "<TR>";
          Stream << "<TD";
          WritePort(*ChildValue, Stream, InNode);
          Stream << ">";
          auto const ChildField = Record->getChildField(i);
          if (ChildField)
            Stream << "." << Record->getChildField(i)->getName();
          Stream << "</TD>";
          
          Stream << "<TD>";
          WriteDot(ChildValue, Stream, InNode, AllEdges);
          Stream << "</TD>";
          Stream << "</TR>";
        }
        
        Stream << "</TABLE>";
      }
      break;
    
    case seec::cm::Value::Kind::Pointer:
      {
        Stream << "&nbsp;";
        
        auto const Pointer = llvm::cast<seec::cm::ValueOfPointer>(Value.get());
        unsigned const DerefLimit = Pointer->getDereferenceIndexLimit();
        
        llvm::errs() << "writing pointer with limit = " << DerefLimit << "\n";
        
        for (unsigned i = 0; i < DerefLimit; ++i) {
          auto const Pointee = Pointer->getDereferenced(i);
          
          llvm::errs() << "recording edge.\n";
          
          auto const Inserted = AllEdges.emplace(Value, Pointee);
          
          if (!Inserted.second)
            llvm::errs() << "emplace failed.\n";
        }
      }
      break;
  }
}


void WriteDot(seec::cm::AllocaState const &State,
              llvm::raw_ostream &Stream,
              Node &InNode,
              std::set<Edge> &AllEdges)
{
  auto const Value = State.getValue();
  
  Stream << "<TR><TD>"
         << State.getDecl()->getNameAsString()
         << "</TD><TD ALIGN=\"LEFT\"";
  WritePort(*Value, Stream, InNode);
  Stream << ">";
  WriteDot(State.getValue(), Stream, InNode, AllEdges);
  Stream << "</TD></TR>";
}

void WriteDot(seec::cm::FunctionState const &State,
              llvm::raw_ostream &Stream,
              std::vector<Node> &FunctionNodes,
              std::set<Edge> &AllEdges)
{
  std::string NodeIdentifier;
  
  {
    llvm::raw_string_ostream NodeIdentifierStream {NodeIdentifier};
    NodeIdentifierStream << "function" << reinterpret_cast<uintptr_t>(&State);
  }
  
  Stream << NodeIdentifier
         << " [ label = < <TABLE BORDER=\"1\" CELLSPACING=\"0\">";
  
  Stream << "<TR><TD COLSPAN=\"2\">"
         << State.getNameAsString()
         << "</TD></TR>";
  
  FunctionNodes.emplace_back(std::move(NodeIdentifier));
  
  for (auto const &Parameter : State.getParameters())
    WriteDot(Parameter, Stream, FunctionNodes.back(), AllEdges);
  
  for (auto const &Local : State.getLocals())
    WriteDot(Local, Stream, FunctionNodes.back(), AllEdges);
  
  Stream << "</TABLE> > ];\n";
}

/// \brief Write the dot describing a ThreadState.
///
void WriteDot(seec::cm::ThreadState const &State,
              llvm::raw_ostream &Stream,
              std::vector<Node> &AllNodes,
              std::set<Edge> &AllEdges)
{
  auto const ID = State.getUnmappedState().getTrace().getThreadID();
  std::vector<Node> FunctionNodes;
  
  Stream << "subgraph thread" << ID << " {\n";
  Stream << "node [shape=plaintext];\n";
  
  for (auto const &FunctionState : State.getCallStack()) {
    WriteDot(FunctionState, Stream, FunctionNodes, AllEdges);
  }
  
  Stream << "{ rank=same; ";
  for (auto const &FunctionNode : FunctionNodes)
    Stream << FunctionNode.getIdentifier() << "; ";
  Stream << "};\n";
  
  Stream << "}\n";
  
  // Add the function nodes to the global list of nodes.
  AllNodes.insert(AllNodes.end(), FunctionNodes.begin(), FunctionNodes.end());
}

/// \brief Write the dot describing a GlobalVariable.
///
void writeDot(seec::cm::GlobalVariable const &State,
              llvm::raw_ostream &Stream,
              std::vector<Node> &AllNodes,
              std::set<Edge> &AllEdges)
{
  std::string NodeIdentifier = "global";
  {
    llvm::raw_string_ostream Stream { NodeIdentifier };
    Stream << State.getAddress();
  }
  
  Stream << NodeIdentifier
         << " [ label = < <TABLE BORDER=\"1\" CELLSPACING=\"0\">"
         << "<TR><TD>"
         << State.getClangValueDecl()->getName()
         << "</TD>";
  
  AllNodes.emplace_back(std::move(NodeIdentifier));
  
  auto const Value = State.getValue();
  
  Stream << "<TD";
  if (Value)
    WritePort(*Value, Stream, AllNodes.back());
  Stream << ">";
  WriteDot(Value, Stream, AllNodes.back(), AllEdges);
  Stream << "</TD></TR></TABLE> > ];\n";
}

/// \brief Write a graph in dot format describing a complete ProcessState.
///
void writeDotGraph(seec::cm::ProcessState const &State,
                   llvm::raw_ostream &Stream)
{ 
  Stream << "digraph Process {\n";
  Stream << "node [shape=plaintext];\n";
  Stream << "rankdir=LR;\n";
  
  std::vector<Node> AllNodes;
  std::set<Edge> AllEdges;
  
  // Write all threads.
  for (std::size_t i = 0; i < State.getThreadCount(); ++i) {
    WriteDot(State.getThread(i), Stream, AllNodes, AllEdges);
  }
  
  // Write global variables.
  for (auto const &Global : State.getGlobalVariables()) {
    writeDot(Global, Stream, AllNodes, AllEdges);
  }
  
  // Write all referenced, but non-present nodes.
  // One possible method for this is to expand all Value objects, then filter
  // them according to the mallocs they belong to, then draw the mallocs as
  // nodes. Finally we would need to draw Values that existed in Known memory
  // (non-alloca, non-malloc, non-global).
  
  llvm::errs() << "got " << AllEdges.size() << " edges.\n";
  
  // Write edges.
  for (auto const &E : AllEdges) {
    auto const NodeStart =
      std::find_if(AllNodes.begin(),
                   AllNodes.end(),
                   [&E] (Node const &N) { return N.hasPort(*(E.first)); });
    
    auto const NodeEnd =
      std::find_if(AllNodes.begin(),
                   AllNodes.end(),
                   [&E] (Node const &N) { return N.hasPort(*(E.second)); });
    
    if (NodeStart == AllNodes.end() || NodeEnd == AllNodes.end()) {
      llvm::errs() << "couldn't find start and end for edge.\n";
      continue;
    }
    
    Stream << NodeStart->getEdgeEndForPort(*(E.first))
           << ":c -> "
           << NodeEnd->getEdgeEndForPort(*(E.second))
           << ";\n";
  }
  
  Stream << "}\n";
}


} // namespace cm

} // namespace seec
