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

#include "seec/Clang/GraphExpansion.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/PrintOnlinePythonTutorTrace.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Printing.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"

#include "clang/AST/Decl.h"
#include "clang/Lex/Lexer.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace seec::runtime_errors;
using namespace seec::util;

namespace {

std::string makeAddressString(seec::trace::stateptr_ty const Address)
{
  std::string RetVal;
  {
    llvm::raw_string_ostream RetStream(RetVal);
    RetStream << '"';
    write_hex_padded(RetStream, Address);
    RetStream << '"';
  }
  return RetVal;
}

}

namespace seec {

namespace cm {

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
  OPTSettings const &Settings;

  llvm::raw_ostream &Stream;

  std::string StateString;

  llvm::raw_string_ostream Out;

  IndentationGuide Indent;

  ProcessTrace const &Trace;

  ProcessState Process;

  llvm::DenseMap<seec::trace::offset_uint, uint32_t> FrameIDMap;

  unsigned PreviousLine;

  unsigned PreviousExprColumn;

  unsigned PreviousExprWidth;

  OPTPrinter(OPTSettings const &WithSettings,
             llvm::raw_ostream &ToStream,
             ProcessTrace const &FromTrace)
  : Settings(WithSettings),
    Stream(ToStream),
    StateString(),
    Out(StateString),
    Indent("  ", 1),
    Trace(FromTrace),
    Process(FromTrace),
    FrameIDMap(),
    PreviousLine(1),
    PreviousExprColumn(1),
    PreviousExprWidth(0)
  {}

  uint32_t getFrameID(FunctionState const &Function);

  void printArray(ValueOfArray const &V);

  void printRecord(ValueOfRecord const &V);

  void printPointer(ValueOfPointer const &PV);

  void printValue(Value const &V);

  void printPossibleNullValue(std::shared_ptr<Value const> const &V);

  void printHeapValue(std::shared_ptr<Value const> const &V);

  std::string printGlobal(GlobalVariable const &GV);

  void printGlobals();

  void printParameter(ParamState const &Param, std::string &NameOut);

  void printLocal(LocalState const &Local, std::string &NameOut);

  void printFunction(FunctionState const &Function, bool IsActive);

  void printThread(ThreadState const &Thread);

  void printAreaList(std::shared_ptr<ValueOfPointer const> const &Ref,
                     unsigned const Limit);

  bool printArea(MemoryArea const &Area,
                 seec::cm::graph::Expansion const &Expansion);

  bool printFILE(StreamState const &State,
                 seec::cm::graph::Expansion const &Expansion);

  bool printDIR(DIRState const &State,
                seec::cm::graph::Expansion const &Expansion);

  void printHeap();

  void setPreviousPositions(clang::SourceLocation Start,
                            clang::SourceLocation End,
                            clang::ASTContext const &AST);

  bool printAndMoveState();

  bool printAllStates();

public:
  static bool print(OPTSettings const &Settings,
                    llvm::raw_ostream &Out,
                    ProcessTrace const &Trace)
  {
    OPTPrinter Printer(Settings, Out, Trace);
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

void OPTPrinter::printArray(ValueOfArray const &V)
{
  auto const Limit = V.getChildCount();

  Out << "[\n";
  Indent.indent();
  Out << Indent.getString() << "\"C_ARRAY\",\n"
      << Indent.getString() << makeAddressString(V.getAddress()) << ",\n";

  for (unsigned i = 0; i < Limit; ++i) {
    if (i != 0)
      Out << ",\n";
    Out << Indent.getString();
    printPossibleNullValue(V.getChildAt(i));
  }
  Out << "\n";

  Indent.unindent();
  Out << Indent.getString() << "]";
}

void OPTPrinter::printRecord(ValueOfRecord const &V)
{
  auto const Limit = V.getChildCount();

  Out << "[\n";
  Indent.indent();
  Out << Indent.getString() << "\"C_STRUCT\",\n"
      << Indent.getString() << makeAddressString(V.getAddress()) << ",\n";

  // Write the struct type name.
  Out << Indent.getString();
  writeJSONStringLiteral(V.getTypeAsString(), Out);
  Out << ",\n";

  for (unsigned i = 0; i < Limit; ++i) {
    if (i != 0)
      Out << ",\n";

    Out << Indent.getString() << "[\n";
    Indent.indent();

    // field name
    Out << Indent.getString();
    writeJSONStringLiteral(V.getChildField(i)->getNameAsString(), Out);
    Out << ",\n";

    // field value
    Out << Indent.getString();
    printPossibleNullValue(V.getChildAt(i));
    Out << "\n";

    Indent.unindent();
    Out << Indent.getString() << "]";
  }
  Out << "\n";

  Indent.unindent();
  Out << Indent.getString() << "]";
}

void OPTPrinter::printPointer(ValueOfPointer const &PV)
{
  Out << "[\n";
  Indent.indent();
  Out << Indent.getString() << "\"C_DATA\",\n"
      << Indent.getString() << makeAddressString(PV.getAddress()) << ",\n"
      << Indent.getString() << "\"pointer\",\n"
      << Indent.getString();

  if (!PV.isCompletelyInitialized()) {
    Out << "\"<UNINITIALIZED>\"\n";
  }
  else {
    auto const RawValue = PV.getRawValue();
    if (!RawValue) {
      Out << "\"NULL\"\n";
    }
    else if (PV.getDereferenceIndexLimit() != 0 || PV.isValidOpaque()) {
      Out << makeAddressString(RawValue) << "\n";
    }
    else {
      Out << "\"<INVALID>\"\n";
    }
  }

  Indent.unindent();
  Out << Indent.getString() << "]";
}

void OPTPrinter::printValue(Value const &V)
{
  switch (V.getKind()) {
    case Value::Kind::Basic:   SEEC_FALLTHROUGH;
    case Value::Kind::Complex: SEEC_FALLTHROUGH;
    case Value::Kind::Scalar:
      {
        Out << "[\"C_DATA\", "
            << makeAddressString(V.getAddress()) << ", ";
        writeJSONStringLiteral(V.getTypeAsString(), Out);
        Out << ",";

        if (V.isCompletelyInitialized()) {
          auto const Str = V.getValueAsStringFull();

          int (*IsDigitPtr)(int) = &std::isdigit;
          auto const IsNumeric = std::all_of(Str.begin(), Str.end(), IsDigitPtr);

          if (IsNumeric)
            Out << Str;
          else
            writeJSONStringLiteral(Str, Out);
        }
        else {
          Out << "\"<UNINITIALIZED>\"";
        }

        Out << "]";
      }
      break;

    case Value::Kind::Array:
      printArray(llvm::cast<ValueOfArray>(V));
      break;

    case Value::Kind::Record:
      printRecord(llvm::cast<ValueOfRecord>(V));
      break;

    case Value::Kind::Pointer:
      printPointer(llvm::cast<ValueOfPointer>(V));
      break;
  }
}

void OPTPrinter::printPossibleNullValue(std::shared_ptr<Value const> const &V)
{
  if (V) {
    printValue(*V);
  }
  else {
    Out << "null";
  }
}

void OPTPrinter::printHeapValue(std::shared_ptr<Value const> const &V)
{
  Out << "[\n";
  Indent.indent();
  Out << Indent.getString() << "\"HEAP_PRIMITIVE\",\n"
      << Indent.getString() << "\"\",\n";

  printPossibleNullValue(V);

  Indent.unindent();
  Out << Indent.getString() << "]";
}

std::string OPTPrinter::printGlobal(GlobalVariable const &GV)
{
  std::string NameOut = GV.getClangValueDecl()->getNameAsString();

  Out << Indent.getString();
  writeJSONStringLiteral(NameOut, Out);
  Out << ": ";

  printPossibleNullValue(GV.getValue());

  return NameOut;
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

    OrderedNames.emplace_back(printGlobal(*GV));
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

  printPossibleNullValue(Param.getValue());
}

void OPTPrinter::printLocal(LocalState const &Local, std::string &NameOut)
{
  NameOut = Local.getDecl()->getNameAsString();

  Out << Indent.getString();
  writeJSONStringLiteral(NameOut, Out);
  Out << ": ";

  printPossibleNullValue(Local.getValue());
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
  std::string ExceptionMessage;

  // func_name
  // stack_to_render
  if (!Stack.empty()) {
    // Write the active function's name.
    auto const &Active = Stack.back().get();
    Out << Indent.getString() << "\"func_name\": ";
    writeJSONStringLiteral(Active.getNameAsString(), Out);
    Out << ",\n";

    // Add runtime error descriptions to ExceptionMessage
    {
      llvm::raw_string_ostream ExceptionStream(ExceptionMessage);
      auto const AugmentFn = Settings.getAugmentations().getCallbackFn();
      for (auto const &RunError : Active.getRuntimeErrorsActive()) {
        auto MaybeDesc = RunError.getDescription(AugmentFn);
        if (MaybeDesc.assigned(0)) {
          DescriptionPrinterUnicode Printer(MaybeDesc.move<0>(), "\n", "  ");
          ExceptionStream << Printer.getString() << "\n";
        }
      }
    }

    if (!ExceptionMessage.empty()) {
      Out << Indent.getString() << "\"exception_msg\": ";
      writeJSONStringLiteral(ExceptionMessage, Out);
      Out << ",\n";
    }

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
  if (!ExceptionMessage.empty())
    Out << "\"exception\"";
  else if (Thread.isAtEnd())
    Out << "\"return\"";
  else
    Out << "\"step_line\"";
  Out << ",\n";
}

void OPTPrinter::printAreaList(std::shared_ptr<ValueOfPointer const> const &Ref,
                               unsigned const Limit)
{
  Out << "[\n";
  Indent.indent();
  Out << Indent.getString() << "\"C_ARRAY\",\n"
      << Indent.getString() << makeAddressString(Ref->getRawValue()) << ",\n";

  for (unsigned i = 0; i < Limit; ++i) {
    if (i != 0)
      Out << ",\n";
    Out << Indent.getString();
    printPossibleNullValue(Ref->getDereferenced(i));
  }
  Out << "\n";

  Indent.unindent();
  Out << Indent.getString() << "]";
}

bool OPTPrinter::printArea(MemoryArea const &Area,
                           seec::cm::graph::Expansion const &Expansion)
{
  auto Refs = Expansion.getReferencesOfArea(Area.start(), Area.end());
  if (Refs.empty())
    return false;

  // Remove pointers to void, incomplete types, or to children of other
  // pointees (e.g. pointers to struct members).
  seec::cm::graph::reduceReferences(Refs);

  Out << Indent.getString() << makeAddressString(Area.start()) << ": ";

  auto const &Ref = Refs.front();
  auto const Limit = Ref->getDereferenceIndexLimit();

  switch (Limit) {
    case 0:
      Out << "[\"HEAP_PRIMITIVE\", \"\", [ \"C_DATA\", "
          << makeAddressString(Area.start()) << ", \"\", "
          << "\"non-dereferenceable area\"]]";
      break;

    case 1:
      printHeapValue(Ref->getDereferenced(0));
      break;

    default:
      printAreaList(Ref, Limit);
      break;
  }

  return true;
}

bool OPTPrinter::printFILE(StreamState const &State,
                           seec::cm::graph::Expansion const &Expansion)
{
  Out << Indent.getString() << makeAddressString(State.getAddress()) << ": [\n";
  Indent.indent();
  Out << Indent.getString() << "\"HEAP_PRIMITIVE\",\n"
      << Indent.getString() << "\"\",\n"
      << Indent.getString() << "[ \"C_DATA\", "
                            << makeAddressString(State.getAddress()) << ", "
                            << "\"FILE\", ";

  Out << Indent.getString();
  writeJSONStringLiteral(State.getFilename(), Out);
  Out << " ] \n";

  Indent.unindent();
  Out << Indent.getString() << "]";

  return true;
}

bool OPTPrinter::printDIR(DIRState const &State,
                          seec::cm::graph::Expansion const &Expansion)
{
  Out << Indent.getString() << makeAddressString(State.getAddress()) << ": [\n";
  Indent.indent();
  Out << Indent.getString() << "\"HEAP_PRIMITIVE\",\n"
      << Indent.getString() << "\"\",\n"
      << Indent.getString() << "[ \"C_DATA\", "
                            << makeAddressString(State.getAddress()) << ", "
                            << "\"DIR\", ";

  Out << Indent.getString();
  writeJSONStringLiteral(State.getDirname(), Out);
  Out << " ] \n";

  Indent.unindent();
  Out << Indent.getString() << "]";

  return true;
}

void OPTPrinter::printHeap()
{
  Out << Indent.getString() << "\"heap\": {\n";
  Indent.indent();

  auto const &Expansion = seec::cm::graph::Expansion::from(Process);
  bool Printed = false;

  for (auto const &Area : Process.getUnmappedStaticAreas()) {
    if (Printed)
      Out << ",\n";
    Printed = printArea(Area, Expansion);
  }

  for (auto const &Malloc : Process.getDynamicMemoryAllocations()) {
    if (Printed)
      Out << ",\n";
    Printed = printArea(MemoryArea(Malloc.getAddress(),
                                   Malloc.getSize()),
                                   Expansion);
  }

  for (auto const &Known : Process.getUnmappedProcessState().getKnownMemory()) {
    if (Printed)
      Out << ",\n";

    auto const Size = (Known.End - Known.Begin) + 1;

    Printed = printArea(MemoryArea(Known.Begin, Size), Expansion);
  }

  // Print FILEs
  for (auto const &File : Process.getStreams()) {
    if (Printed)
      Out << ",\n";
    Printed = printFILE(File.second, Expansion);
  }

  // Print DIRs
  for (auto const &Dir : Process.getDIRs()) {
    if (Printed)
      Out << ",\n";
    Printed = printDIR(Dir.second, Expansion);
  }

  Out << "\n";
  Indent.unindent();
  Out << Indent.getString() << "},\n";
}

void OPTPrinter::setPreviousPositions(clang::SourceLocation Start,
                                      clang::SourceLocation End,
                                      clang::ASTContext const &AST)
{
  auto const &SourceManager = AST.getSourceManager();

  while (Start.isMacroID())
    Start = SourceManager.getExpansionLoc(Start);

  while (End.isMacroID())
    End = SourceManager.getExpansionLoc(End);

  PreviousLine = SourceManager.getSpellingLineNumber(Start);
  PreviousExprColumn = SourceManager.getSpellingColumnNumber(Start);

  // Find the first character following the last token.
  auto const FollowingEnd =
    clang::Lexer::getLocForEndOfToken(End,
                                      0,
                                      SourceManager,
                                      AST.getLangOpts());

  auto const BestEnd = FollowingEnd.isValid() ? FollowingEnd : End;

  if (SourceManager.isWrittenInSameFile(Start, BestEnd)) {
    PreviousExprWidth = SourceManager.getFileOffset(BestEnd)
                      - SourceManager.getFileOffset(Start);
  }
  else {
    PreviousExprWidth = 1;
  }
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

  // heap
  printHeap();

  // Move now so that we can get the "next" line number.
  auto const Moved = moveForward(Process.getThread(0));

  // line: int
  auto const &Thread = Process.getThread(0);
  auto const &Stack = Thread.getCallStack();
  auto const OldPreviousLine = PreviousLine;

  if (!Stack.empty()) {
    auto const &ActiveFn = Stack.back().get();
    auto const &AST = ActiveFn.getMappedAST()->getASTUnit().getASTContext();

    if (auto const ActiveStmt = ActiveFn.getActiveStmt())
      setPreviousPositions(ActiveStmt->getLocStart(),
                           ActiveStmt->getLocEnd(),
                           AST);
    else
      setPreviousPositions(ActiveFn.getFunctionDecl()->getLocStart(),
                           ActiveFn.getFunctionDecl()->getLocEnd(),
                           AST);
  }

  if (Settings.getPyCrazyMode()) {
    Out << Indent.getString() << "\"line\": " << PreviousLine << ",\n"
        << Indent.getString() << "\"expr_start_col\": "
                              << (PreviousExprColumn - 1) << ",\n"
        << Indent.getString() << "\"expr_width\": "
                              << PreviousExprWidth << "\n";
  }
  else {
    Out << Indent.getString() << "\"line\": " << PreviousLine << "\n";
  }

  Indent.unindent();
  Out << Indent.getString() << "}";

  if (Moved != seec::cm::MovementResult::Unmoved)
    Out << ",\n";
  else
    Out << "\n";

  Out.flush();

  if (Settings.getPyCrazyMode()) {
    // In pyCrazyMode our default movement is suitable.
    Stream << StateString;
  }
  else {
    // If the line moves at the next state, then print this state. This models
    // the behaviour expected by OnlinePythonTutor (each step represents the
    // complete execution of one line).
    if (OldPreviousLine != PreviousLine || Thread.isAtEnd())
      Stream << StateString;
  }

  StateString.clear();

  return Moved != seec::cm::MovementResult::Unmoved;
}

bool OPTPrinter::printAllStates()
{
  // OnlinePythonTutor output only supports single-threaded programs.
  if (Process.getThreadCount() != 1)
    return false;

  bool const UseVarName = !Settings.getVariableName().empty();

  Stream << Indent.getString();
  if (UseVarName)
    Stream << "var " << Settings.getVariableName() << " = ";
  Stream << "{\n";
  Indent.indent();

  // Write the source code.
  auto const SourceCode = GetSingularMainFileContents(Trace);
  if (SourceCode.empty())
    return false;

  Stream << Indent.getString() << "\"code\": ";
  writeJSONStringLiteral(SourceCode, Stream);
  Stream << ",\n";

  Stream << Indent.getString() << "\"trace\": [\n";
  Indent.indent();

  while (printAndMoveState()) {}

  Indent.unindent();
  Stream << Indent.getString() << "]\n";

  Indent.unindent();
  Stream << Indent.getString();
  if (UseVarName)
    Stream << "};\n";
  else
    Stream << "}\n";

  return true;
}

void PrintOnlinePythonTutor(ProcessTrace const &Trace,
                            OPTSettings const &Settings,
                            llvm::raw_ostream &Out)
{
  OPTPrinter::print(Settings, Out, Trace);
}

void PrintOnlinePythonTutor(ProcessTrace const &Trace,
                            OPTSettings const &Settings)
{
  OPTPrinter::print(Settings, llvm::outs(), Trace);
}

} // namespace cm (in seec)

} // namespace seec
