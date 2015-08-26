//===- tools/seec-trace-print/Unmapped.cpp --------------------------------===//
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

#include "seec/Clang/GraphLayout.hpp"
#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StateMovement.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Printing.hpp"
#include "seec/Util/Resources.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"

#include "unicode/unistr.h"

#include "OnlinePythonTutor.hpp"
#include "Unmapped.hpp"

#include <array>
#include <memory>
#include <system_error>
#include <type_traits>

using namespace seec;
using namespace llvm;

namespace seec {
  namespace trace_print {
    extern cl::opt<std::string> InputDirectory;

    extern cl::opt<bool> UseClangMapping;

    extern cl::opt<std::string> OutputDirectoryForClangMappedDot;

    extern cl::opt<bool> TestGraphGeneration;

    extern cl::opt<bool> ShowCounts;

    extern cl::opt<bool> ShowRawEvents;

    extern cl::opt<bool> ShowStates;

    extern cl::opt<bool> ShowErrors;

    extern cl::opt<bool> OnlinePythonTutor;

    extern cl::opt<bool> ReverseStates;
  }
}

using namespace seec::trace_print;

void WriteDotGraph(seec::cm::ProcessState const &State,
                   char const *Filename,
                   std::string const &DotString)
{
  assert(Filename && "NULL Filename.");

  std::error_code EC;
  llvm::raw_fd_ostream Stream {Filename, EC, llvm::sys::fs::OpenFlags::F_Text};

  if (EC) {
    llvm::errs() << "Error opening dot file: " << EC.message() << "\n";
    return;
  }

  Stream << DotString;
}

void PrintClangMappedStates(seec::cm::ProcessTrace const &Trace,
                            seec::AugmentationCollection const &Augmentations)
{
  seec::cm::ProcessState State(Trace);

  // If we're going to output dot graph files for the states, then setup the
  // output directory and layout handler now.
  std::unique_ptr<seec::cm::graph::LayoutHandler> LayoutHandler;
  llvm::SmallString<256> OutputForDot;
  std::string FilenameString;
  llvm::raw_string_ostream FilenameStream {FilenameString};
  long StateNumber = 1;

  if (!OutputDirectoryForClangMappedDot.empty()) {
    OutputForDot = OutputDirectoryForClangMappedDot;

    bool Existed = false;
    auto const Err =
      llvm::sys::fs::create_directories(llvm::StringRef(OutputForDot),
                                        Existed);

    if (Err) {
      llvm::errs() << "Couldn't create output directory: "
                   << Err.message() << "\n";
      return;
    }
  }

  if (!OutputDirectoryForClangMappedDot.empty() || TestGraphGeneration) {
    LayoutHandler.reset(new seec::cm::graph::LayoutHandler());
    LayoutHandler->addBuiltinLayoutEngines();
  }

  if (State.getThreadCount() == 1) {
    llvm::outs() << "Using thread-level iterator.\n";
    auto const WriteGraphs = !OutputForDot.empty() || TestGraphGeneration;
    seec::util::IndentationGuide Indent("  ");
    auto Augmenter = Augmentations.getCallbackFn();

    do {
      // Write textual description to stdout.
      State.print(llvm::outs(), Indent, Augmenter);
      llvm::outs() << "\n";

      // If enabled, write graphs in dot format.
      if (WriteGraphs) {
        auto const Layout = LayoutHandler->doLayout(State);

        if (!OutputForDot.empty()){
          // Add filename for this state.
          FilenameString.clear();
          FilenameStream << "state." << StateNumber++ << ".dot";
          FilenameStream.flush();

          llvm::sys::path::append(OutputForDot, FilenameString);

          // Write the graph.
          WriteDotGraph(State, OutputForDot.c_str(), Layout.getDotString());

          // Remove filename for this state.
          llvm::sys::path::remove_filename(OutputForDot);
        }
      }
    } while (seec::cm::moveForward(State.getThread(0))
             != seec::cm::MovementResult::Unmoved);

    if (ReverseStates) {
      while (seec::cm::moveBackward(State.getThread(0))
             != seec::cm::MovementResult::Unmoved)
      {
        llvm::outs() << State << "\n";
      }
    }
  }
  else {
    llvm::outs() << "Using process-level iteration.\n";
    llvm::outs() << State;
  }
}

void PrintClangMapped(seec::AugmentationCollection const &Augmentations)
{
  // Attempt to setup the trace reader.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(InputDirectory);
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeIBA.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }

  auto IBA = seec::makeUnique<trace::InputBufferAllocator>
                             (MaybeIBA.move<trace::InputBufferAllocator>());

  // Read the trace.
  auto CMProcessTraceLoad = cm::ProcessTrace::load(std::move(IBA));

  if (CMProcessTraceLoad.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = CMProcessTraceLoad.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }

  auto CMProcessTrace = CMProcessTraceLoad.move<0>();

  if (ShowStates) {
    PrintClangMappedStates(*CMProcessTrace, Augmentations);
  }
  else if (OnlinePythonTutor) {
    PrintOnlinePythonTutor(*CMProcessTrace,
                           OPTSettings{}.setPyCrazyMode(true));
  }
}
