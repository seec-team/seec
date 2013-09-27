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
#include "seec/DSA/MemoryArea.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/Util/Maybe.hpp"

#include <chrono>
#include <map>
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


/// \brief Get the standard port string for a Value.
///
std::string getStandardPortFor(Value const &V);

/// \brief Write a property.
///
void encodeProperty(llvm::raw_ostream &Out,
                    llvm::Twine Property);

/// \brief Write a (key,value) pair property.
///
void encodeProperty(llvm::raw_ostream &Out,
                    llvm::Twine Key,
                    llvm::Twine Value);


/// \brief Type of the end of an edge.
///
enum class EdgeEndType {
  Standard,
  Punned,
  Elided
};


/// \brief Type of a memory area.
///
enum class AreaType {
  Static,
  Dynamic
};


/// \brief Contains information about the location of a single seec::cm::Value.
///
class ValuePort {
  EdgeEndType EdgeEnd;
  
  std::string CustomPort;
  
public:
  ValuePort(EdgeEndType WithEdgeEnd)
  : EdgeEnd(WithEdgeEnd),
    CustomPort()
  {}
  
  ValuePort(EdgeEndType WithEdgeEnd,
            std::string WithCustomPort)
  : EdgeEnd(WithEdgeEnd),
    CustomPort(std::move(WithCustomPort))
  {}
  
  EdgeEndType getEdgeEnd() const { return EdgeEnd; }
  
  /// \brief Get the custom port for this Value, or an empty string if there is
  ///        none.
  ///
  /// A custom port is used if the standard port that would be returned from
  /// getStandardPortFor() does not exist, but a useful port is still available.
  /// For example, if a layout engine elides a number of values, but shows a
  /// marker where the values would be, then a port for this marker may be a
  /// reasonable custom port for all of the elided values.
  ///
  std::string getCustomPort() const {
    return CustomPort;
  }
};


/// \brief Contains several ValuePorts.
///
class ValuePortMap {
  std::map<Value const *, ValuePort> Map;
  
public:
  /// \brief Find the port for a Value, if it exists.
  ///
  seec::Maybe<ValuePort> getPortForValue(Value const &Val) const {
    auto const It = Map.find(&Val);
    return It != Map.end() ? seec::Maybe<ValuePort>(It->second)
                           : seec::Maybe<ValuePort>();
  }
  
  /// \brief Add a single port.
  ///
  void add(Value const &Val, ValuePort Port) {
    Map.insert(std::make_pair(&Val, Port));
  }
  
  /// \brief Add all ports from Other to this.
  ///
  void addAllFrom(ValuePortMap const &Other) {
    Map.insert(Other.Map.begin(), Other.Map.end());
  }
};


/// \brief Represents the layout of a seec::cm::Value and its children.
///
class LayoutOfValue {
  std::string DotString;
  
  ValuePortMap Ports;
  
public:
  /// \brief Constructor.
  ///
  LayoutOfValue(std::string WithDotString,
                ValuePortMap WithPorts)
  : DotString(std::move(WithDotString)),
    Ports(std::move(WithPorts))
  {}
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get a string describing the layout for dot.
  ///
  std::string const &getDotString() const { return DotString; }
  
  /// \brief Find the port for a Value, if any was created.
  ///
  seec::Maybe<ValuePort> getPortForValue(Value const &Val) const {
    return Ports.getPortForValue(Val);
  }
  
  /// \brief Get all ports.
  ///
  decltype(Ports) const &getPorts() const { return Ports; }
  
  /// @} (Accessors.)
};


/// \brief Represents the layout of an area.
///
class LayoutOfArea {
  std::string ID;
  
  std::string DotString;
  
  ValuePortMap Ports;
  
public:
  LayoutOfArea(std::string WithID,
               std::string WithDotString,
               ValuePortMap WithPorts)
  : ID(std::move(WithID)),
    DotString(std::move(WithDotString)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getID() const { return ID; }
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


/// \brief Represents the layout of a seec::cm::ProcessState.
///
class LayoutOfProcess {
  std::string DotString;
  
  std::chrono::nanoseconds TimeTaken;
  
public:
  LayoutOfProcess(std::string WithDotString,
                  std::chrono::nanoseconds TimeTakenToGenerate)
  : DotString(std::move(WithDotString)),
    TimeTaken(std::move(TimeTakenToGenerate))
  {}
  
  std::string const &getDotString() const { return DotString; }
  
  std::chrono::nanoseconds const &getTimeTaken() const { return TimeTaken; }
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
  virtual LayoutOfValue
  doLayoutImpl(seec::cm::Value const &Value, Expansion const &E) const =0;
  
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
  LayoutOfValue doLayout(seec::cm::Value const &Value, Expansion const &E) const
  {
    return doLayoutImpl(Value, E);
  }
};


/// \brief Interface for area layout engines.
///
class LayoutEngineForArea : public LayoutEngine {
  /// \brief Internal implementation of canLayout().
  ///
  virtual bool
  canLayoutImpl(seec::MemoryArea const &Area,
                seec::cm::ValueOfPointer const &Reference) const =0;
  
  /// \brief Internal implementation of doLayout().
  ///
  virtual LayoutOfArea
  doLayoutImpl(seec::MemoryArea const &Area,
               seec::cm::ValueOfPointer const &Reference,
               Expansion const &E) const =0;
  
protected:
  /// \brief Constructor.
  ///
  LayoutEngineForArea(LayoutHandler const &InHandler)
  : LayoutEngine{InHandler}
  {}
  
public:
  /// \brief Check if this engine is capable of laying out an area.
  /// \return true iff this engine is capable of laying out the area.
  ///
  bool canLayout(seec::MemoryArea const &Area,
                 seec::cm::ValueOfPointer const &Reference) const
  {
    return canLayoutImpl(Area, Reference);
  }
  
  /// \brief Layout an area.
  ///
  LayoutOfArea doLayout(seec::MemoryArea const &Area,
                        seec::cm::ValueOfPointer const &Reference,
                        Expansion const &E) const
  {
    return doLayoutImpl(Area, Reference, E);
  }
};


/// \brief Coordinates layout and manages preferences.
///
class LayoutHandler final {
  /// \name Value Layout
  /// @{
  
  std::vector<std::unique_ptr<LayoutEngineForValue>> ValueEngines;
  
  LayoutEngineForValue const *ValueEngineDefault;
  
  std::map<std::pair<uintptr_t, clang::Type const *>,
           LayoutEngineForValue const *> ValueEngineOverride;
  
  /// @}
  
  /// \name Area Layout
  /// @{
  
  std::vector<std::unique_ptr<LayoutEngineForArea>> AreaEngines;
  
  std::map<std::pair<uintptr_t, clang::Type const *>,
           LayoutEngineForArea const *> AreaEngineOverride;
  
  /// @}
  
public:
  /// \brief Default constructor.
  ///
  LayoutHandler()
  : ValueEngines(),
    ValueEngineDefault(nullptr),
    ValueEngineOverride(),
    AreaEngines(),
    AreaEngineOverride()
  {}
  
  
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
  
  /// \brief List the Value layout engines that support a Value.
  ///
  std::vector<LayoutEngineForValue const *>
  listLayoutEnginesSupporting(Value const &ForValue) const;
  
  /// \brief Set the layout engine to use for a particular value.
  ///
  bool setLayoutEngine(Value const &ForValue, uintptr_t EngineID);
  
  /// \brief List the Area layout engines that support an Area and Pointer.
  ///
  std::vector<LayoutEngineForArea const *>
  listLayoutEnginesSupporting(seec::MemoryArea const &Area,
                              seec::cm::ValueOfPointer const &Reference) const;
  
  /// \brief Set the Area layout engine to use for an Area and Pointer.
  ///
  bool setLayoutEngine(seec::MemoryArea const &ForArea,
                       seec::cm::ValueOfPointer const &ForReference,
                       uintptr_t EngineID);
  
  /// @}
  
  
  /// \name Layout Creation
  /// @{
  
public:
  /// \brief Write a HREF to identify a Value.
  ///
  void writeHREF(llvm::raw_ostream &Out, Value const &ForValue) const;
  
  /// \brief Write a HREF to identify an Area.
  ///
  void writeHREF(llvm::raw_ostream &Out,
                 seec::MemoryArea const &ForArea,
                 seec::cm::ValueOfPointer const &ForReference) const;
  
  /// \brief Write standard properties for a Value.
  ///
  void writeStandardProperties(llvm::raw_ostream &Out,
                               Value const &ForValue) const;
  
  /// \brief Perform the layout for a value.
  ///
  seec::Maybe<LayoutOfValue>
  doLayout(seec::cm::Value const &State, Expansion const &Exp) const;
  
  /// \brief Perform the layout for an area.
  ///
  seec::Maybe<LayoutOfArea>
  doLayout(seec::MemoryArea const &Area,
           seec::cm::ValueOfPointer const &Reference,
           Expansion const &Exp) const;
  
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
