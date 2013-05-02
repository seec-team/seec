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
  
  bool hasPort(seec::cm::Value const &ForValue) {
    return Ports.count(reinterpret_cast<uintptr_t>(&ForValue));
  }
};


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


void WritePort(seec::cm::Value const &Value,
               llvm::raw_ostream &Stream,
               Node &InNode)
{
  Stream << " PORT=\"" << getPortFor(Value) << "\"";
  InNode.addPort(Value);
}


void WriteDot(std::shared_ptr<seec::cm::Value const> Value,
              llvm::raw_ostream &Stream,
              Node &InNode)
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
          WriteDot(ChildValue, Stream, InNode);
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
          WriteDot(ChildValue, Stream, InNode);
          Stream << "</TD>";
          Stream << "</TR>";
        }
        
        Stream << "</TABLE>";
      }
      break;
    
    case seec::cm::Value::Kind::Pointer:
      {
        Stream << "&nbsp;";
        
        /*
        auto const Pointer = llvm::cast<seec::cm::ValueOfPointer>(Value.get());
        unsigned const DerefLimit = Pointer->getDereferenceIndexLimit();
        
        for (unsigned i = 0; i < DerefLimit; ++i) {
          auto const Pointee = Pointer->getDereferenced(i);
          
          Edges.emplace("", getPortFor(*Value), "", getPortFor(*Pointee));
          
          Pointees.insert(Pointer->getDereferenced(i));
        }
        */
      }
      break;
  }
}


void WriteDot(seec::cm::AllocaState const &State,
              llvm::raw_ostream &Stream,
              Node &InNode)
{
  auto const Value = State.getValue();
  
  Stream << "<TR><TD";
  WritePort(*Value, Stream, InNode);
  Stream << ">"
         << State.getDecl()->getNameAsString()
         << "</TD><TD ALIGN=\"LEFT\">";
  WriteDot(State.getValue(), Stream, InNode);
  Stream << "</TD></TR>";
}

void WriteDot(seec::cm::FunctionState const &State,
              llvm::raw_ostream &Stream,
              std::vector<Node> &FunctionNodes)
{
  std::string NodeIdentifier;
  
  {
    llvm::raw_string_ostream NodeIdentifierStream {NodeIdentifier};
    NodeIdentifierStream << "function" << reinterpret_cast<uintptr_t>(&State);
  }
  
  Stream << NodeIdentifier
         << " [ label = < <TABLE BORDER=\"0\" CELLSPACING=\"0\">";
  
  Stream << "<TR><TD COLSPAN=\"2\">"
         << State.getNameAsString()
         << "</TD></TR>";
  
  FunctionNodes.emplace_back(std::move(NodeIdentifier));
  
  for (auto const &Parameter : State.getParameters())
    WriteDot(Parameter, Stream, FunctionNodes.back());
  
  for (auto const &Local : State.getLocals())
    WriteDot(Local, Stream, FunctionNodes.back());
  
  Stream << "</TABLE> > ];\n";
}

void WriteDot(seec::cm::ThreadState const &State,
              llvm::raw_ostream &Stream,
              std::vector<Node> &AllNodes)
{
  auto const ID = State.getUnmappedState().getTrace().getThreadID();
  std::vector<Node> FunctionNodes;
  
  Stream << "subgraph thread" << ID << " {\n";
  Stream << "node [shape = record];\n";
  
  for (auto const &FunctionState : State.getCallStack()) {
    WriteDot(FunctionState, Stream, FunctionNodes);
  }
  
  Stream << "{ rank=same; ";
  for (auto const &FunctionNode : FunctionNodes)
    Stream << FunctionNode.getIdentifier() << "; ";
  Stream << "};\n";
  
  Stream << "}\n";
  
  // Add the function nodes to the global list of nodes.
  AllNodes.insert(AllNodes.end(), FunctionNodes.begin(), FunctionNodes.end());
}

void writeDot(seec::cm::GlobalVariable const &State,
              llvm::raw_ostream &Stream,
              std::vector<Node> &AllNodes)
{
  std::string NodeIdentifier = "global";
  {
    llvm::raw_string_ostream Stream { NodeIdentifier };
    Stream << State.getAddress();
  }
  
  Stream << NodeIdentifier
         << " [ label = < <TABLE BORDER=\"0\" CELLSPACING=\"0\">"
         << "<TR><TD>"
         << State.getClangValueDecl()->getName()
         << "</TD>";
  
  AllNodes.emplace_back(std::move(NodeIdentifier));
  
  auto const Value = State.getValue();
  
  Stream << "<TD";
  if (Value)
    WritePort(*Value, Stream, AllNodes.back());
  Stream << ">";
  WriteDot(Value, Stream, AllNodes.back());
  Stream << "</TD></TR></TABLE> > ];";
}

void writeDotGraph(seec::cm::ProcessState const &State,
                   llvm::raw_ostream &Stream)
{ 
  Stream << "digraph Process {\n";
  Stream << "node [shape = record];\n";
  Stream << "rankdir=LR;\n";
  
  std::vector<Node> AllNodes;
  
  // Write all threads.
  for (std::size_t i = 0; i < State.getThreadCount(); ++i) {
    WriteDot(State.getThread(i), Stream, AllNodes);
  }
  
  // Write global variables.
  for (auto const &Global : State.getGlobalVariables()) {
    writeDot(Global, Stream, AllNodes);
  }
  
  // Write all referenced, but non-present nodes.
  
  // Write edges.
  
  Stream << "}\n";
}


} // namespace cm

} // namespace seec
