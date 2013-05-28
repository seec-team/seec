//===- lib/Clang/GraphLayout.hpp ------------------------------------------===//
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

#ifndef SEEC_LIB_CLANG_GRAPHLAYOUT_HPP
#define SEEC_LIB_CLANG_GRAPHLAYOUT_HPP


#include "seec/Clang/MappedValue.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/Util/Maybe.hpp"

#include <memory>
#include <vector>


namespace seec {

namespace cm {

class FunctionState;
class GlobalVariable;
class ProcessState;
class ThreadState;
class Value;

namespace graph {

class Expansion;
class LayoutHandler;


/// \brief Represents the layout of a seec::cm::Value.
///
class LayoutOfValue {
  std::string DotString;
  
public:
  LayoutOfValue(std::string WithDotString)
  : DotString(std::move(WithDotString))
  {}
  
  LayoutOfValue(LayoutOfValue const &Other) = default;
  
  LayoutOfValue(LayoutOfValue &&Other) = default;
  
  LayoutOfValue &operator=(LayoutOfValue const &Other) = default;
  
  LayoutOfValue &operator=(LayoutOfValue &&Other) = default;
  
  std::string const &getDotString() const { return DotString; }
};

/// \brief Represents the layout of an area.
///
class LayoutOfArea {
};

/// \brief Represents the layout of a seec::cm::FunctionState.
///
class LayoutOfFunction {
  std::string DotString;
  
public:
  LayoutOfFunction(std::string WithDotString)
  : DotString(std::move(WithDotString))
  {}
  
  LayoutOfFunction(LayoutOfFunction const &Other) = default;
  
  LayoutOfFunction(LayoutOfFunction &&Other) = default;
  
  LayoutOfFunction &operator=(LayoutOfFunction const &Other) = default;
  
  LayoutOfFunction &operator=(LayoutOfFunction &&Other) = default;
  
  std::string const &getDotString() const { return DotString; }
};

/// \brief Represents the layout of a seec::cm::ThreadState.
///
class LayoutOfThread {
};

/// \brief Represents the layout of a seec::cm::GlobalVariable.
///
class LayoutOfGlobalVariable {
};

/// \brief Represents the layout of a seec::cm::ProcessState.
///
class LayoutOfProcess {
};


/// \brief Interface common to all layout engines.
///
class LayoutEngine {
  /// The handler that this engine belongs to.
  LayoutHandler const &Handler;
  
  /// \brief Internal implementation of getName().
  ///
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const =0;
  
protected:
  /// \brief Constructor.
  ///
  LayoutEngine(LayoutHandler const &InHandler)
  : Handler(InHandler)
  {}
  
public:
  /// \brief Allow destruction from base class pointer.
  ///
  virtual ~LayoutEngine() = default;
  
  /// \brief Get the handler that this engine belongs to.
  ///
  LayoutHandler const &getHandler() const { return Handler; }
  
  /// \brief Get a localized name for this layout engine.
  ///
  std::unique_ptr<seec::LazyMessage> getName() const {
    return getNameImpl();
  }
};


/// \brief Interface for seec::cm::Value layout engines.
///
class LayoutEngineForValue : public LayoutEngine {
  /// \brief Internal implementation of canLayout().
  ///
  virtual bool canLayoutImpl(seec::cm::Value const &Value) const =0;
  
  /// \brief Internal implementation of doLayout().
  ///
  virtual LayoutOfValue doLayoutImpl(seec::cm::Value const &Value) const =0;
  
protected:
  /// \brief Constructor.
  ///
  LayoutEngineForValue(LayoutHandler const &InHandler)
  : LayoutEngine{InHandler}
  {}
  
public:
  /// \brief Check if this engine is capable of laying out a Value.
  /// \param Value the Value to check.
  /// \return true iff this engine is capable of laying out Value.
  ///
  bool canLayout(seec::cm::Value const &Value) const {
    return canLayoutImpl(Value);
  }
  
  /// \brief Layout a Value.
  /// \param Value the Value to layout.
  ///
  LayoutOfValue doLayout(seec::cm::Value const &Value) const {
    return doLayoutImpl(Value);
  }
};


/// \brief Interface for area layout engines.
///
class LayoutEngineForArea : public LayoutEngine {
protected:
  /// \brief Constructor.
  ///
  LayoutEngineForArea(LayoutHandler const &InHandler)
  : LayoutEngine{InHandler}
  {}
  
public:
};


/// \brief Coordinates layout and manages preferences.
///
class LayoutHandler final {
  /// \name Value Layout
  /// @{
  
  std::vector<std::unique_ptr<LayoutEngineForValue>> ValueEngines;
  
  // TODO: User-selected default engine.
  
  // TODO: Per-Value override.
  
  /// @}
  
  /// \name Area Layout
  /// @{
  
  std::vector<std::unique_ptr<LayoutEngineForArea>> AreaEngines;
  
  // TODO: Per-Area override.
  
  /// @}
  
public:
  /// \brief Default constructor.
  ///
  LayoutHandler() {}
  
  
  /// \name Layout Engine Handling
  /// @{
  
  /// \brief Add all of the builtin layout engines to this handler.
  ///
  void addBuiltinLayoutEngines();
  
  /// \brief Add a Value layout engine.
  ///
  void addLayoutEngine(std::unique_ptr<LayoutEngineForValue> Engine);
  
  /// \brief Add an Area layout engine.
  ///
  void addLayoutEngine(std::unique_ptr<LayoutEngineForArea> Engine);
  
  // TODO: List the Value layout engines.
  
  // TODO: Set the Value layout engine to use for a particular Value.
  
  /// @}
  
  
  /// \name Layout Creation
  /// @{
  
private:
  /// \brief Perform the layout for a value.
  ///
  seec::Maybe<LayoutOfValue>
  doLayout(seec::cm::Value const &State) const;
  
  /// \brief Perform the layout for an expanded function state.
  ///
  LayoutOfFunction
  doLayout(seec::cm::FunctionState const &State,
           seec::cm::graph::Expansion const &Expansion) const;
  
  /// \brief Perform the layout for an expanded thread state.
  ///
  LayoutOfThread
  doLayout(seec::cm::ThreadState const &State,
           seec::cm::graph::Expansion const &Expansion) const;
  
  /// \brief Perform the layout for an expanded global variable state.
  ///
  LayoutOfGlobalVariable
  doLayout(seec::cm::GlobalVariable const &State,
           seec::cm::graph::Expansion const &Expansion) const;
  
  /// \brief Perform the layout for an expanded process state.
  ///
  LayoutOfProcess
  doLayout(seec::cm::ProcessState const &State,
           seec::cm::graph::Expansion const &Expansion) const;
  
public:
  /// \brief Perform expansion and layout for a process state.
  ///
  LayoutOfProcess
  doLayout(seec::cm::ProcessState const &State) const;
  
  /// @}
};


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_LIB_CLANG_GRAPHLAYOUT_HPP
