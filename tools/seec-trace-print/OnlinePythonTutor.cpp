//===- tools/seec-trace-print/OnlinePythonTutor.cpp -----------------------===//
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

#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Printing.hpp"

#include "clang/AST/Decl.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace seec;
using namespace seec::cm;
using namespace seec::util;

llvm::StringRef GetSingularMainFileContents(ProcessTrace const &Trace)
{
  auto const &CompileInfoMap = Trace.getMapping().getCompileInfoMap();
  if (CompileInfoMap.size() != 1)
    return llvm::StringRef();

  auto const FI = CompileInfoMap.cbegin()->second->getMainFileInfo();
  if (!FI)
    return llvm::StringRef();

  return FI->getContents().getBuffer();
}

class OPTPrinter {
  llvm::raw_ostream &Out;

  IndentationGuide Indent;

  ProcessTrace const &Trace;

  ProcessState Process;

  llvm::DenseMap<seec::trace::offset_uint, uint32_t> FrameIDMap;

  unsigned PreviousLine;

  OPTPrinter(llvm::raw_ostream &ToStream, ProcessTrace const &FromTrace)
  : Out(ToStream),
    Indent("  ", 1),
    Trace(FromTrace),
    Process(FromTrace),
    FrameIDMap(),
    PreviousLine(1)
  {}

  uint32_t getFrameID(FunctionState const &Function);

  void printValue(Value const &V);

  void printGlobal(GlobalVariable const &GV, std::string &NameOut);

  void printGlobals();

  void printParameter(ParamState const &Param, std::string &NameOut);

  void printLocal(LocalState const &Local, std::string &NameOut);

  void printFunction(FunctionState const &Function, bool IsActive);

  void printThread(ThreadState const &Thread);

  bool printAndMoveState();

  bool printAllStates();

public:
  static bool print(llvm::raw_ostream &Out, ProcessTrace const &Trace)
  {
    OPTPrinter Printer(Out, Trace);
    return Printer.printAllStates();
  }
};

uint32_t OPTPrinter::getFrameID(FunctionState const &Function)
{
  auto const &Trace = Function.getUnmappedState().getTrace();
  auto const Result = FrameIDMap.insert(std::make_pair(Trace.getEventStart(),
                                                       FrameIDMap.size() + 1));
  return Result.first->second;
}

void OPTPrinter::printValue(Value const &V)
{
  switch (V.getKind()) {
    case Value::Kind::Basic:
      writeJSONStringLiteral(V.getValueAsStringFull(), Out);
      break;

    case Value::Kind::Scalar:
      if (V.isCompletelyInitialized())
        Out << V.getValueAsStringFull();
      else
        writeJSONStringLiteral(V.getValueAsStringFull(), Out);
      break;

    case Value::Kind::Array:
      writeJSONStringLiteral("<cannot render correctly>", Out);
      break;

    case Value::Kind::Record:
      writeJSONStringLiteral("<cannot render correctly>", Out);
      break;

    case Value::Kind::Pointer:
      // TODO: Render as REF when heap rendering is working.
      writeJSONStringLiteral(V.getValueAsStringFull(), Out);
      break;

    case Value::Kind::PointerToFILE:
      writeJSONStringLiteral(V.getValueAsStringFull(), Out);
      break;
  }
}

void OPTPrinter::printGlobal(GlobalVariable const &GV, std::string &NameOut)
{
  NameOut = GV.getClangValueDecl()->getNameAsString();

  Out << Indent.getString();
  writeJSONStringLiteral(NameOut, Out);
  Out << ": ";

  if (auto const V = GV.getValue())
    printValue(*V);
  else
    Out << "\"<no value>\"";
}

void OPTPrinter::printGlobals()
{
  std::vector<std::string> OrderedNames;

  // globals: dict
  Out << Indent.getString() << "\"globals\": {\n";
  Indent.indent();

  bool PreviousPrinted = false;

  for (auto const &GV : Process.getGlobalVariables()) {
    if (!GV || (GV->isInSystemHeader() && !GV->isReferenced()))
      continue;

    if (PreviousPrinted)
      Out << ",\n";
    else
      PreviousPrinted = true;

    OrderedNames.emplace_back();
    printGlobal(*GV, OrderedNames.back());
  }

  Indent.unindent();
  Out << "\n" << Indent.getString() << "},\n";

  // ordered_globals: [string]
  Out << Indent.getString() << "\"ordered_globals\": [";
  for (std::size_t i = 0; i < OrderedNames.size(); ++i) {
    if (i != 0)
      Out << ", ";
    writeJSONStringLiteral(OrderedNames[i], Out);
  }
  Out << "],\n";
}

void OPTPrinter::printParameter(ParamState const &Param, std::string &NameOut)
{
  NameOut = Param.getDecl()->getNameAsString();

  Out << Indent.getString();
  writeJSONStringLiteral(NameOut, Out);
  Out << ": ";

  if (auto const V = Param.getValue())
    printValue(*V);
  else
    Out << "\"<no value>\"";
}

void OPTPrinter::printLocal(LocalState const &Local, std::string &NameOut)
{
  NameOut = Local.getDecl()->getNameAsString();

  Out << Indent.getString();
  writeJSONStringLiteral(NameOut, Out);
  Out << ": ";

  if (auto const V = Local.getValue())
    printValue(*V);
  else
    Out << "\"<no value>\"";
}

void OPTPrinter::printFunction(FunctionState const &Function, bool IsActive)
{
  Out << Indent.getString() << "{\n";
  Indent.indent();

  // func_name
  auto const FnName = Function.getNameAsString();
  Out << Indent.getString() << "\"func_name\": ";
  writeJSONStringLiteral(FnName, Out);
  Out << ",\n";

  // frame_id = unique key for this function call
  // unique_hash = func_name + frame_id
  auto const FrameID = getFrameID(Function);
  Out << Indent.getString() << "\"frame_id\": " << FrameID << ",\n";
  Out << Indent.getString() << "\"unique_hash\": ";
  writeJSONStringLiteral(FnName + std::to_string(FrameID), Out);
  Out << ",\n";

  // encoded_locals = dict
  std::vector<std::string> OrderedVarnames;

  Out << Indent.getString() << "\"encoded_locals\": {\n";
  Indent.indent();

  {
    bool PreviousPrinted = false;

    for (auto const &Param : Function.getParameters()) {
      if (PreviousPrinted)
        Out << ",\n";
      else
        PreviousPrinted = true;

      OrderedVarnames.emplace_back();
      printParameter(Param, OrderedVarnames.back());
    }

    for (auto const &Local : Function.getLocals()) {
      if (PreviousPrinted)
        Out << ",\n";
      else
        PreviousPrinted = true;

      OrderedVarnames.emplace_back();
      printLocal(Local, OrderedVarnames.back());
    }
  }

  Indent.unindent();
  Out << "\n" << Indent.getString() << "},\n";

  // ordered_varnames = [string]
  Out << Indent.getString() << "\"ordered_varnames\": [";
  for (std::size_t i = 0; i < OrderedVarnames.size(); ++i) {
    if (i != 0)
      Out << ", ";
    writeJSONStringLiteral(OrderedVarnames[i], Out);
  }
  Out << "],\n";

  // is_highlighted = ?
  Out << Indent.getString();
  if (IsActive)
    Out << "\"is_highlighted\": true,\n";
  else
    Out << "\"is_highlighted\": false,\n";

  // These are for Closures and Zombie Frames, so we don't need them.
  Out << Indent.getString() << "\"is_parent\": false,\n";
  Out << Indent.getString() << "\"is_zombie\": false,\n";
  Out << Indent.getString() << "\"parent_frame_id_list\": []\n";

  Indent.unindent();
  Out << Indent.getString() << "}";
}

void OPTPrinter::printThread(ThreadState const &Thread)
{
  auto const &Stack = Thread.getCallStack();

  // func_name
  // stack_to_render
  if (!Stack.empty()) {
    // Write the active function's name.
    auto const &Active = Stack.back().get();
    Out << Indent.getString() << "\"func_name\": ";
    writeJSONStringLiteral(Active.getNameAsString(), Out);
    Out << ",\n";

    // Write the stack.
    Out << Indent.getString() << "\"stack_to_render\": [\n";
    Indent.indent();

    bool PreviousPrinted = false;
    for (auto const &Fn : Stack) {
      if (PreviousPrinted)
        Out << ",\n";
      else
        PreviousPrinted = true;

      printFunction(Fn, &Fn.get() == &Active);
    }

    Indent.unindent();
    Out << "\n" << Indent.getString() << "],\n";
  }
  else {
    Out << Indent.getString() << "\"func_name\": \"<none>\",\n";
    Out << Indent.getString() << "\"stack_to_render\": [],\n";
  }

  // event: string
  Out << Indent.getString() << "\"event\": ";
  if (!Thread.isAtEnd())
    Out << "\"step_line\"";
  else
    Out << "\"return\"";
  Out << ",\n";
}

/// \brief Get the start line in the outermost file.
///
static unsigned getLineOutermost(clang::SourceLocation Start,
                                 clang::ASTContext const &AST)
{
  auto const &SourceManager = AST.getSourceManager();

  while (Start.isMacroID())
    Start = SourceManager.getExpansionLoc(Start);

  return SourceManager.getSpellingLineNumber(Start);
}

bool OPTPrinter::printAndMoveState()
{
  Out << Indent.getString() << "{\n";
  Indent.indent();

  printGlobals();
  printThread(Process.getThread(0));

  // stdout
  if (auto const Stream = Process.getStreamStdout()) {
    Out << Indent.getString() << "\"stdout\": ";
    writeJSONStringLiteral(Stream->getWritten(), Out);
    Out << ",\n";
  }
  else {
    Out << Indent.getString() << "\"stdout\": \"\",\n";
  }

  // TODO: heap
  Out << Indent.getString() << "\"heap\": {},\n";

  // Move now so that we can get the "next" line number.
  auto const Moved = moveForward(Process.getThread(0));

  // line: int
  auto const &Thread = Process.getThread(0);
  auto const &Stack = Thread.getCallStack();

  if (!Stack.empty()) {
    auto const &ActiveFn = Stack.back().get();
    auto const &AST = ActiveFn.getMappedAST()->getASTUnit().getASTContext();

    if (auto const ActiveStmt = ActiveFn.getActiveStmt())
      PreviousLine = getLineOutermost(ActiveStmt->getLocStart(), AST);
    else
      PreviousLine = getLineOutermost(ActiveFn.getFunctionDecl()->getLocStart(),
                                      AST);
  }

  Out << Indent.getString() << "\"line\": " << PreviousLine << "\n";

  Indent.unindent();
  Out << Indent.getString() << "}";

  return Moved;
}

bool OPTPrinter::printAllStates()
{
  // OnlinePythonTutor output only supports single-threaded programs.
  if (Process.getThreadCount() != 1)
    return false;

  Out << Indent.getString() << "{\n";
  Indent.indent();

  // Write the source code.
  auto const SourceCode = GetSingularMainFileContents(Trace);
  if (SourceCode.empty())
    return false;

  Out << Indent.getString() << "\"code\": ";
  writeJSONStringLiteral(SourceCode, Out);
  Out << ",\n";

  Out << Indent.getString() << "\"trace\": [\n";
  Indent.indent();

  while (printAndMoveState()) {
    Out << ",\n";
  }
  Out << "\n";

  Indent.unindent();
  Out << Indent.getString() << "]\n";

  Indent.unindent();
  Out << Indent.getString() << "}\n";

  return true;
}

void PrintOnlinePythonTutor(ProcessTrace const &Trace)
{
  OPTPrinter::print(llvm::outs(), Trace);
}
