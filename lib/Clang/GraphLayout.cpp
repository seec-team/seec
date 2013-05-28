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
#include "seec/Util/MakeUnique.hpp"

#include <future>


namespace seec {

namespace cm {

namespace graph {


//===----------------------------------------------------------------------===//
// Helper functions
//===----------------------------------------------------------------------===//

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
// LEVStandard
//===----------------------------------------------------------------------===//

/// \brief Value layout engine "Standard".
///
class LEVStandard final : public LayoutEngineForValue {
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout", "LEVStandard", "Name"});
  }
  
  virtual bool canLayoutImpl(seec::cm::Value const &Value) const override {
    return true;
  }
  
  virtual LayoutOfValue
  doLayoutImpl(seec::cm::Value const &Value) const override {
    std::string DotString;
    
    DotString = EscapeForHTML(Value.getValueAsStringFull());
    
    return LayoutOfValue{std::move(DotString)};
  }
  
public:
  LEVStandard(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler}
  {}
};


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
  
public:
  LEAStandard(LayoutHandler const &InHandler)
  : LayoutEngineForArea{InHandler}
  {}
};


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
LayoutHandler::doLayout(seec::cm::Value const &State) const {
  for (auto const &EnginePtr : ValueEngines)
    if (EnginePtr->canLayout(State))
      return EnginePtr->doLayout(State);
  
  return seec::Maybe<LayoutOfValue>();
}

// doLayoutForAlloca LayoutHandler, Expansion, Alloca -> LayoutOfAlloca

LayoutOfFunction
LayoutHandler::doLayout(seec::cm::FunctionState const &State,
                        seec::cm::graph::Expansion const &Expansion) const
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "function_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::vector<LayoutOfValue> ParameterLayouts;
  std::vector<LayoutOfValue> LocalLayouts;
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">"
            << "<TR><TD COLSPAN=\"2\">"
            << State.getNameAsString()
            << "</TD></TR>";
  
  for (auto const &Parameter : State.getParameters()) {
    // Attempt to get the value.
    auto const Value = Parameter.getValue();
    if (!Value)
      continue;
    
    DotStream << "<TR><TD>"
              << Parameter.getDecl()->getNameAsString()
              << "</TD><TD>";
    
    // Attempt to layout the value.
    auto MaybeLayout = doLayout(*Value);
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      ParameterLayouts.emplace_back(MaybeLayout.move<LayoutOfValue>());
      DotStream << ParameterLayouts.back().getDotString();
    }
    else {
      DotStream << "~";
    }
    
    DotStream << "</TD></TR>";
  }
  
  for (auto const &Local : State.getLocals()) {
    auto const Value = Local.getValue();
    if (!Value)
      continue;
    
    DotStream << "<TR><TD>"
              << Local.getDecl()->getNameAsString()
              << "</TD><TD>";
    
    auto MaybeLayout = doLayout(*Value);
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      LocalLayouts.emplace_back(MaybeLayout.move<LayoutOfValue>());
      DotStream << LocalLayouts.back().getDotString();
    }
    else {
      DotStream << "~";
    }
    
    DotStream << "</TD></TR>";
  }
  
  DotStream << "</TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfFunction{std::move(DotString)};
}

LayoutOfThread
LayoutHandler::doLayout(seec::cm::ThreadState const &State,
                        seec::cm::graph::Expansion const &Expansion) const
{
  return LayoutOfThread{};
}

LayoutOfGlobalVariable
LayoutHandler::doLayout(seec::cm::GlobalVariable const &State,
                        seec::cm::graph::Expansion const &Expansion) const
{
  return LayoutOfGlobalVariable{};
}

LayoutOfProcess
LayoutHandler::doLayout(seec::cm::ProcessState const &State,
                        seec::cm::graph::Expansion const &Expansion) const
{
  // Create tasks to generate global variable layouts.
  std::vector<std::future<LayoutOfGlobalVariable>> GlobalVariableLayouts;
  
  auto const Globals = State.getGlobalVariables();
  for (auto It = Globals.begin(), End = Globals.end(); It != End; ++It)
    GlobalVariableLayouts.emplace_back(
      std::async( [&, It, this] () {
                    return this->doLayout(*It, Expansion);
                  } ));
  
  // Create tasks to generate thread layouts.
  auto const ThreadCount = State.getThreadCount();
  
  std::vector<std::future<LayoutOfThread>> ThreadLayouts;
  ThreadLayouts.reserve(ThreadCount);
  
  for (std::size_t i = 0; i < ThreadCount; ++i)
    ThreadLayouts.emplace_back(
      std::async( [&, i, this] () {
                    return this->doLayout(State.getThread(i), Expansion);
                  } ));
  
  // TODO: Create tasks to generate malloc area layouts.
  // foreach malloc:
  //   async lambda doLayoutOfArea
  
  // TODO: Create tasks to generate known memory area layouts.
  // foreach known:
  //   async lambda doLayoutOfArea
  
  // TODO: Retrieve results and combine layouts.
  
  return LayoutOfProcess{};
}

LayoutOfProcess
LayoutHandler::doLayout(seec::cm::ProcessState const &State) const
{
  return doLayout(State, Expansion::from(State));
}


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec
