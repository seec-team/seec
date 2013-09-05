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
#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/MakeUnique.hpp"

#include "llvm/Support/raw_ostream.h"

#include "unicode/locid.h"

#include <future>


namespace seec {

namespace cm {

namespace graph {


//===----------------------------------------------------------------------===//
// Helper functions
//===----------------------------------------------------------------------===//

std::string getStandardPortFor(Value const &V)
{
  std::string Port;
  
  {
    llvm::raw_string_ostream PortStream {Port};
    PortStream << "value_at_" << reinterpret_cast<uintptr_t>(&V);
  }
  
  return Port;
}

static std::string EscapeForHTML(llvm::StringRef String)
{
  std::string Escaped;
  Escaped.reserve(String.size());
  
  for (auto const Char : String) {
    if (std::isalnum(Char)) {
      Escaped.push_back(Char);
    }
    else if (std::isprint(Char)) {
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
      else {
        Escaped.push_back('0' + Char);
      }
      
      Escaped.push_back(';');
    }
    else {
      Escaped += "&#92;";
      Escaped += std::to_string(Char);
    }
  }
  
  return Escaped;
}


//===----------------------------------------------------------------------===//
// Value types
//===----------------------------------------------------------------------===//


enum class NodeType {
  None,
  Function,
  Global
};

class NodeInfo {
  NodeType Type;
  
  std::string ID;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  NodeInfo(NodeType WithType,
           std::string WithID,
           MemoryArea WithArea,
           ValuePortMap WithPorts)
  : Type(WithType),
    ID(std::move(WithID)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  NodeType getType() const { return Type; }
  
  std::string const &getID() const { return ID; }
  
  MemoryArea const &getArea() const { return Area; }
  
  seec::Maybe<ValuePort> getPortForValue(Value const &Val) const {
    return Ports.getPortForValue(Val);
  }
};

/// \brief Represents an edge that has been laid out.
///
class LayoutOfPointer {
  EdgeEndType TailType;
  
  EdgeEndType HeadType;
  
public:
  LayoutOfPointer(EdgeEndType WithTailType,
                  EdgeEndType WithHeadType)
  : TailType(WithTailType),
    HeadType(WithHeadType)
  {}
  
  EdgeEndType getTailType() const { return TailType; }
  
  EdgeEndType getHeadType() const { return HeadType; }
};


/// \brief Represents the layout of a seec::cm::LocalState.
///
class LayoutOfLocal {
  std::string DotString;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  LayoutOfLocal(std::string WithDotString,
                MemoryArea WithArea,
                ValuePortMap WithPorts)
  : DotString(std::move(WithDotString)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Area) const &getArea() const { return Area; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


/// \brief Represents the layout of a seec::cm::FunctionState.
///
class LayoutOfFunction {
  std::string ID;
  
  std::string DotString;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  LayoutOfFunction(std::string WithID,
                   std::string WithDotString,
                   MemoryArea WithArea,
                   ValuePortMap WithPorts)
  : ID(std::move(WithID)),
    DotString(std::move(WithDotString)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getID() const { return ID; }
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Area) const &getArea() const { return Area; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


/// \brief Represents the layout of a seec::cm::ThreadState.
///
class LayoutOfThread {
  std::string DotString;
  
  std::vector<NodeInfo> Nodes;
  
public:
  LayoutOfThread(std::string WithDotString,
                 std::vector<NodeInfo> WithNodes)
  : DotString(std::move(WithDotString)),
    Nodes(std::move(WithNodes))
  {}
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Nodes) const &getNodes() const { return Nodes; }
};


/// \brief Represents the layout of a seec::cm::GlobalVariable.
///
class LayoutOfGlobalVariable {
  std::string ID;
  
  std::string DotString;
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
public:
  LayoutOfGlobalVariable(std::string WithID,
                         std::string WithDotString,
                         MemoryArea WithArea,
                         ValuePortMap WithPorts)
  : ID(std::move(WithID)),
    DotString(std::move(WithDotString)),
    Area(std::move(WithArea)),
    Ports(std::move(WithPorts))
  {}
  
  std::string const &getID() const { return ID; }
  
  std::string const &getDotString() const { return DotString; }
  
  decltype(Area) const &getArea() const { return Area; }
  
  decltype(Ports) const &getPorts() const { return Ports; }
};


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
  
  virtual bool canLayoutImpl(Value const &V) const override {
    return true;
  }
  
  virtual LayoutOfValue
  doLayoutImpl(Value const &V, Expansion const &E) const override;
  
public:
  LEVStandard(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler}
  {}
};

LayoutOfValue
LEVStandard::doLayoutImpl(Value const &V, Expansion const &E) const
{
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  
  switch (V.getKind()) {
    case Value::Kind::Basic: SEEC_FALLTHROUGH;
    case Value::Kind::Scalar:
    {
      auto const IsInit = V.isCompletelyInitialized();
      
      Stream << "<TD PORT=\""
             << getStandardPortFor(V)
             << "\"";
      
      getHandler().writeStandardProperties(Stream, V);
      
      Stream << ">";
      
      if (IsInit)
        Stream << EscapeForHTML(V.getValueAsStringFull());
      
      Stream << "</TD>";
      
      break;
    }
    
    case Value::Kind::Array:
    {
      auto const &Array = static_cast<ValueOfArray const &>(V);
      unsigned const ChildCount = Array.getChildCount();
      
      Stream << "<TD PORT=\""
             << getStandardPortFor(V)
             << "\"";
      
      getHandler().writeStandardProperties(Stream, V);
      
      Stream << "><TABLE BORDER=\"0\" "
                  "CELLSPACING=\"0\" CELLBORDER=\"1\">";
      
      for (unsigned i = 0; i < ChildCount; ++i) {
        auto const ChildValue = Array.getChildAt(i);
        if (!ChildValue)
          continue;
        
        Stream << "<TR><TD>&#91;" << i << "&#93;</TD>";
        
        auto const MaybeLayout = this->getHandler().doLayout(*ChildValue, E);
        if (!MaybeLayout.assigned<LayoutOfValue>()) {
          Stream << "<TD></TD></TR>";
          continue;
        }
        
        auto const &Layout = MaybeLayout.get<LayoutOfValue>();
        
        Stream << Layout.getDotString()
               << "</TR>";
        
        Ports.addAllFrom(Layout.getPorts());
      }
      
      Stream << "</TABLE></TD>";
      
      break;
    }
    
    case Value::Kind::Record:
    {
      auto const &Record = static_cast<ValueOfRecord const &>(V);
      unsigned const ChildCount = Record.getChildCount();
      
      Stream << "<TD PORT=\""
             << getStandardPortFor(V)
             << "\"";
      
      getHandler().writeStandardProperties(Stream, V);
      
      Stream << "><TABLE BORDER=\"0\" "
                  "CELLSPACING=\"0\" CELLBORDER=\"1\">";
      
      for (unsigned i = 0; i < ChildCount; ++i) {
        auto const ChildValue = Record.getChildAt(i);
        if (!ChildValue)
          continue;
        
        Stream << "<TR><TD>";
        
        auto const ChildField = Record.getChildField(i);
        if (ChildField)
          Stream << ChildField->getName();
        else
          Stream << "unknown field"; // TODO: Localize
        
        Stream << "</TD>";
        
        auto const MaybeLayout = this->getHandler().doLayout(*ChildValue, E);
        if (!MaybeLayout.assigned<LayoutOfValue>()) {
          Stream << "<TD></TD></TR>";
          continue;
        }
        
        auto const &Layout = MaybeLayout.get<LayoutOfValue>();
        
        Stream << Layout.getDotString()
               << "</TR>";
        
        Ports.addAllFrom(Layout.getPorts());
      }
      
      Stream << "</TABLE></TD>";
      
      break;
    }
    
    case Value::Kind::Pointer:
    {
      auto const &Ptr = static_cast<ValueOfPointer const &>(V);
      
      if (!Ptr.isCompletelyInitialized()) {
        // An uninitialized pointer.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"";
        
        getHandler().writeStandardProperties(Stream, V);
        
        Stream << ">?</TD>";
      }
      else if (!Ptr.getRawValue()) {
        // A NULL pointer.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"";
        
        getHandler().writeStandardProperties(Stream, V);
        
        Stream << ">NULL</TD>";
      }
      else if (Ptr.getDereferenceIndexLimit() == 0) {
        // An invalid pointer (as far as we're concerned).
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"";
        
        getHandler().writeStandardProperties(Stream, V);
        
        Stream << ">!</TD>";
      }
      else {
        // A valid pointer with at least one dereference.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"";
        
        getHandler().writeStandardProperties(Stream, V);
        
        Stream << "> </TD>";
      }
      
      break;
    }
    
    case Value::Kind::PointerToFILE:
    {
      auto const &Ptr = llvm::cast<ValueOfPointerToFILE const>(V);
      
      if (!Ptr.isCompletelyInitialized()) {
        Stream << "<TD PORT=\"" << getStandardPortFor(V) << "\"";
        getHandler().writeStandardProperties(Stream, V);
        Stream << ">?</TD>";
      }
      else if (!Ptr.getRawValue()) {
        Stream << "<TD PORT=\"" << getStandardPortFor(V) << "\"";
        getHandler().writeStandardProperties(Stream, V);
        Stream << ">NULL</TD>";
      }
      else if (!Ptr.isValid()) {
        Stream << "<TD PORT=\"" << getStandardPortFor(V) << "\"";
        getHandler().writeStandardProperties(Stream, V);
        Stream << ">!</TD>";
      }
      else {
        Stream << "<TD PORT=\"" << getStandardPortFor(V) << "\"";
        getHandler().writeStandardProperties(Stream, V);
        Stream << ">" << Ptr.getValueAsStringFull() << "</TD>";
      }
    }
  }
  
  Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  Stream.flush();
  return LayoutOfValue{std::move(DotString), std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// LEVCString
//===----------------------------------------------------------------------===//

/// \brief Value layout engine "C-String".
///
class LEVCString final : public LayoutEngineForValue {
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout", "LEVCString", "Name"});
  }
  
  virtual bool canLayoutImpl(Value const &V) const override;
  
  virtual LayoutOfValue
  doLayoutImpl(Value const &V, Expansion const &E) const override;
  
public:
  LEVCString(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler}
  {}
};

bool LEVCString::canLayoutImpl(Value const &V) const
{
  if (V.getKind() != Value::Kind::Array)
    return false;
  
  auto const Ty = llvm::cast< ::clang::ArrayType >(V.getCanonicalType());
  return Ty->getElementType()->isCharType();
}

LayoutOfValue LEVCString::doLayoutImpl(Value const &V, Expansion const &E) const
{
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  auto const &Handler = getHandler();
  auto const &Array = static_cast<ValueOfArray const &>(V);
  unsigned const ChildCount = Array.getChildCount();
  
  Stream << "<TD PORT=\""
         << getStandardPortFor(V)
         << "\"";
  
  Handler.writeStandardProperties(Stream, V);
  
  Stream << "><TABLE BORDER=\"0\" CELLPADDING=\"0\" COLOR=\"#BBBBBB\" "
              "CELLSPACING=\"0\" CELLBORDER=\"1\"><TR>";
  
  bool Eliding = false;
  std::size_t ElidingFrom;
  std::size_t ElidingCount;
  
  for (unsigned i = 0; i < ChildCount; ++i) {
    auto const ChildValue = Array.getChildAt(i);
    if (!ChildValue) {
      if (!Eliding)
        Stream << "<TD></TD>";
      continue;
    }
    
    if (Eliding) {
      if (E.isReferencedDirectly(*ChildValue)) {
        // This char is referenced. Stop eliding and resume layout.
        Eliding = false;
      }
      else {
        // Elide this char and move to the next.
        if (++ElidingCount == 1) {
          // Write a cell that will be used for all elided chars.
          Stream << "<TD PORT=\""
                 << getStandardPortFor(V)
                 << "_elided_" << std::to_string(ElidingFrom)
                 << "\"></TD>";
        }
        
        Ports.add(*ChildValue,
                  ValuePort(EdgeEndType::Elided,
                            getStandardPortFor(V)
                            + "_elided_" + std::to_string(ElidingFrom)));
        
        continue;
      }
    }
    
    // Attempt to generate and use the standard layout for this char.
    auto const MaybeLayout = Handler.doLayout(*ChildValue, E);
    
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      auto const &Layout = MaybeLayout.get<LayoutOfValue>();
      Stream << Layout.getDotString();
      Ports.addAllFrom(Layout.getPorts());
    }
    else {
      // No layout generated.
      
      Stream << "<TD PORT=\""
             << getStandardPortFor(*ChildValue)
             << "\"";
      Handler.writeStandardProperties(Stream, *ChildValue);
      Stream << "></TD>";
      
      Ports.add(*ChildValue, ValuePort{EdgeEndType::Standard});
    }
    
    // If this was a terminating null character, start eliding.
    auto const &Scalar = static_cast<ValueOfScalar const &>(*ChildValue);
    if (Scalar.isZero()) {
      Eliding = true;
      ElidingFrom = i + 1;
      ElidingCount = 0;
    }
  }
  
  Stream << "</TR></TABLE></TD>";
  Stream.flush();
  
  return LayoutOfValue(std::move(DotString), std::move(Ports));
}

//===----------------------------------------------------------------------===//
// LEVElideUnreferenced
//===----------------------------------------------------------------------===//

/// \brief Value layout engine "Elide Unreferenced".
///
class LEVElideUnreferenced final : public LayoutEngineForValue {
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout",
                                     "LEVElideUnreferenced", "Name"});
  }
  
  virtual bool canLayoutImpl(Value const &V) const override;
  
  virtual LayoutOfValue
  doLayoutImpl(Value const &V, Expansion const &E) const override;
  
public:
  LEVElideUnreferenced(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler}
  {}
};

bool LEVElideUnreferenced::canLayoutImpl(Value const &V) const
{
  return V.getKind() == Value::Kind::Array;
}

LayoutOfValue
LEVElideUnreferenced::doLayoutImpl(Value const &V, Expansion const &E) const
{
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  auto const &Handler = getHandler();
  auto const &Array = llvm::cast<ValueOfArray const>(V);
  unsigned const ChildCount = Array.getChildCount();
  
  Stream << "<TD PORT=\""
         << getStandardPortFor(V)
         << "\"";
  
  Handler.writeStandardProperties(Stream, V);
  
  Stream << "><TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
  
  bool Eliding = false;
  std::size_t ElidingFrom;
  std::string ElidedPort;
  
  for (unsigned i = 0; i < ChildCount; ++i) {
    auto const ChildValue = Array.getChildAt(i);
    if (!ChildValue)
      continue;
    
    auto const Start = ChildValue->getAddress();
    auto const End = Start + ChildValue->getTypeSizeInChars().getQuantity();
    auto const IsReferenced = E.isAreaReferenced(Start, End)
                            ? searchChildren(*ChildValue,
                                            [&] (Value const &V) {
                                              return E.isReferencedDirectly(V);
                                            })
                            : false;
    
    if (!IsReferenced) {
      if (!Eliding) {
        Eliding = true;
        ElidingFrom = i;
        ElidedPort = getStandardPortFor(V) + "_elided_" +
                     std::to_string(ElidingFrom);
      }
      
      Ports.add(*ChildValue, ValuePort(EdgeEndType::Elided, ElidedPort));
      
      continue;
    }
    
    if (Eliding) {
      // This element is referenced. Stop eliding and resume layout.
      Eliding = false;
      
      // Write a row for the elided elements.
      Stream << "<TR><TD PORT=\"" << ElidedPort << "\">&#91;" << ElidingFrom;
      if (ElidingFrom < i - 1)
        Stream << " &#45; " << (i - 1);
      Stream << "&#93;</TD><TD>Elided</TD></TR>";
    }
    
    // Layout this referenced value.
    Stream << "<TR><TD>&#91;" << i << "&#93;</TD>";
    
    auto const MaybeLayout = Handler.doLayout(*ChildValue, E);
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      auto const &Layout = MaybeLayout.get<LayoutOfValue>();
      Stream << Layout.getDotString() << "</TR>";
      Ports.addAllFrom(Layout.getPorts());
    }
    else {
      Stream << "<TD></TD></TR>";
    }
  }
  
  // Write one final row for the trailing elided elements.
  if (Eliding) {
    Stream << "<TR><TD PORT=\"" << ElidedPort << "\">&#91;" << ElidingFrom;
    if (ElidingFrom < ChildCount - 1)
      Stream << " &#45; " << (ChildCount - 1);
    Stream << "&#93;</TD><TD>Elided</TD></TR>";
  }
  
  Stream << "</TABLE></TD>";
  Stream.flush();
  
  return LayoutOfValue(std::move(DotString), std::move(Ports));
}


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
  
  virtual bool
  canLayoutImpl(seec::MemoryArea const &Area,
                seec::cm::ValueOfPointer const &Reference) const override
  {
    return true;
  }
  
  virtual LayoutOfArea
  doLayoutImpl(seec::MemoryArea const &Area,
               seec::cm::ValueOfPointer const &Reference,
               Expansion const &E) const override;
  
public:
  LEAStandard(LayoutHandler const &InHandler)
  : LayoutEngineForArea{InHandler}
  {}
};

LayoutOfArea
LEAStandard::doLayoutImpl(seec::MemoryArea const &Area,
                          seec::cm::ValueOfPointer const &Reference,
                          Expansion const &E) const
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "area_at_" << Area.start();
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" "
                "CELLSPACING=\"0\" CELLBORDER=\"1\">";
  
  auto const &Handler = this->getHandler();
  auto const Limit = Reference.getDereferenceIndexLimit();
  
  if (Limit == 1) {
    auto const Pointee = Reference.getDereferenced(0);
    if (Pointee) {
      auto const MaybeLayout = Handler.doLayout(*Pointee, E);
      if (MaybeLayout.assigned<LayoutOfValue>()) {
        auto const &Layout = MaybeLayout.get<LayoutOfValue>();
        DotStream << "<TR>" << Layout.getDotString() << "</TR>";
        Ports.addAllFrom(Layout.getPorts());
      }
    }
  }
  else if (Limit > 1) {
    for (unsigned i = 0; i < Limit; ++i) {
      auto const Pointee = Reference.getDereferenced(i);
      if (Pointee) {
        auto const MaybeLayout = Handler.doLayout(*Pointee, E);
        if (MaybeLayout.assigned<LayoutOfValue>()) {
          auto const &Layout = MaybeLayout.get<LayoutOfValue>();
          DotStream << "<TR><TD>&#91;" << i << "&#93;</TD>"
                    << Layout.getDotString()
                    << "</TR>";
          Ports.addAllFrom(Layout.getPorts());
        }
      }
    }
  }
  else {
    // Can't dereference the pointer. Either the memory region is insufficient,
    // or it's a pointer to an incomplete type.
    DotStream << "<TR><TD></TD></TR>";
  }
  
  DotStream << "</TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfArea{std::move(IDString),
                      std::move(DotString),
                      std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// LEACString
//===----------------------------------------------------------------------===//

/// \brief Area layout engine "C String".
///
class LEACString final : public LayoutEngineForArea {
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout", "LEACString", "Name"});
  }
  
  virtual bool
  canLayoutImpl(seec::MemoryArea const &Area,
                seec::cm::ValueOfPointer const &Reference) const override
  {
    auto const Ty = llvm::cast< ::clang::PointerType >
                              (Reference.getCanonicalType());
    return Ty->getPointeeType()->isCharType();
  }
  
  virtual LayoutOfArea
  doLayoutImpl(seec::MemoryArea const &Area,
               seec::cm::ValueOfPointer const &Reference,
               Expansion const &E) const override;
  
public:
  LEACString(LayoutHandler const &InHandler)
  : LayoutEngineForArea{InHandler}
  {}
};

LayoutOfArea
LEACString::doLayoutImpl(seec::MemoryArea const &Area,
                         seec::cm::ValueOfPointer const &Reference,
                         Expansion const &E) const
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "area_at_" << Area.start();
  }
  
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  Ports.add(Reference, ValuePort{EdgeEndType::Standard});
  
  Stream << IDString
         << " [ label = <"
         << "<TABLE BORDER=\"0\" CELLPADDING=\"0\" COLOR=\"#BBBBBB\" "
             "CELLSPACING=\"0\" CELLBORDER=\"1\"><TR>";
  
  auto const &Handler = this->getHandler();
  auto const Limit = Reference.getDereferenceIndexLimit();
  
  bool Eliding = false;
  std::size_t ElidingFrom;
  std::size_t ElidingCount;
  
  for (unsigned i = 0; i < Limit; ++i) {
    auto const ChildValue = Reference.getDereferenced(i);
    if (!ChildValue) {
      if (!Eliding)
        Stream << "<TD></TD>";
      continue;
    }
    
    if (Eliding) {
      if (E.isReferencedDirectly(*ChildValue)) {
        // This char is referenced. Stop eliding and resume layout.
        Eliding = false;
      }
      else {
        // Elide this char and move to the next.
        if (++ElidingCount == 1) {
          // Write a cell that will be used for all elided chars.
          Stream << "<TD PORT=\""
                 << IDString
                 << "_elided_" << std::to_string(ElidingFrom)
                 << "\"></TD>";
        }
        
        Ports.add(*ChildValue,
                  ValuePort(EdgeEndType::Elided,
                            IDString
                            + "_elided_" + std::to_string(ElidingFrom)));
        
        continue;
      }
    }
    
    // Attempt to generate and use the standard layout for this char.
    auto const MaybeLayout = Handler.doLayout(*ChildValue, E);
    
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      auto const &Layout = MaybeLayout.get<LayoutOfValue>();
      Stream << Layout.getDotString();
      Ports.addAllFrom(Layout.getPorts());
    }
    else {
      // No layout generated.
      
      Stream << "<TD PORT=\""
             << getStandardPortFor(*ChildValue)
             << "\"";
      Handler.writeStandardProperties(Stream, *ChildValue);
      Stream << "></TD>";
      
      Ports.add(*ChildValue, ValuePort{EdgeEndType::Standard});
    }
    
    // If this was a terminating null character, start eliding.
    auto const &Scalar = static_cast<ValueOfScalar const &>(*ChildValue);
    if (Scalar.isZero()) {
      Eliding = true;
      ElidingFrom = i + 1;
      ElidingCount = 0;
    }
  }
  
  Stream << "</TR></TABLE>> ];\n";
  Stream.flush();
  
  return LayoutOfArea{std::move(IDString),
                      std::move(DotString),
                      std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// Layout Local
//===----------------------------------------------------------------------===//

static
LayoutOfLocal
doLayout(LayoutHandler const &Handler,
         seec::cm::LocalState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Attempt to get the value.
  auto const Value = State.getValue();
  if (!Value)
    return LayoutOfLocal{std::string{}, MemoryArea{}, ValuePortMap{}};
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
  DotStream << "<TR><TD>"
            << State.getDecl()->getNameAsString()
            << "</TD>";
  
  // Attempt to layout the value.
  auto MaybeLayout = Handler.doLayout(*Value, Expansion);
  if (MaybeLayout.assigned<LayoutOfValue>()) {
    auto const &Layout = MaybeLayout.get<LayoutOfValue>();
    DotStream << Layout.getDotString();
    Ports.addAllFrom(Layout.getPorts());
  }
  else {
    DotStream << "<TD>no layout</TD>"; // TODO: Localise
  }
  
  DotStream << "</TR>";
  DotStream.flush();
  
  if (Value->isInMemory())
    Area = MemoryArea(Value->getAddress(),
                      Value->getTypeSizeInChars().getQuantity());
  
  return LayoutOfLocal{std::move(DotString),
                       std::move(Area),
                       std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// Layout Parameter
//===----------------------------------------------------------------------===//

static
LayoutOfLocal
doLayout(LayoutHandler const &Handler,
         seec::cm::ParamState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Attempt to get the value.
  auto const Value = State.getValue();
  if (!Value)
    return LayoutOfLocal{std::string{}, MemoryArea{}, ValuePortMap{}};
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
  DotStream << "<TR><TD>"
            << State.getDecl()->getNameAsString()
            << "</TD>";
  
  // Attempt to layout the value.
  auto MaybeLayout = Handler.doLayout(*Value, Expansion);
  if (MaybeLayout.assigned<LayoutOfValue>()) {
    auto const &Layout = MaybeLayout.get<LayoutOfValue>();
    DotStream << Layout.getDotString();
    Ports.addAllFrom(Layout.getPorts());
  }
  else {
    DotStream << "<TD>no layout</TD>"; // TODO: Localise
  }
  
  DotStream << "</TR>";
  DotStream.flush();
  
  if (Value->isInMemory())
    Area = MemoryArea(Value->getAddress(),
                      Value->getTypeSizeInChars().getQuantity());
  
  return LayoutOfLocal{std::move(DotString),
                       std::move(Area),
                       std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// Layout Function
//===----------------------------------------------------------------------===//

static
LayoutOfFunction
doLayout(LayoutHandler const &Handler,
         seec::cm::FunctionState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "function_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  MemoryArea Area;
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" "
               "CELLSPACING=\"0\" CELLBORDER=\"1\" HREF=\"function "
            << reinterpret_cast<uintptr_t>(&State)
            << "\">"
               "<TR><TD COLSPAN=\"2\">"
            << State.getNameAsString()
            << "</TD></TR>";
  
  for (auto const &Parameter : State.getParameters()) {
    auto const Layout = doLayout(Handler, Parameter, Expansion);
    
    DotStream << Layout.getDotString();
    
    Ports.addAllFrom(Layout.getPorts());
    
    auto const &AllocaArea = Layout.getArea();
    if (AllocaArea.end() > Area.end())
      Area.setEnd(AllocaArea.end());
    if (Area.start() == 0 || AllocaArea.start() < Area.start())
      Area.setStart(AllocaArea.start());
  }
  
  for (auto const &Local : State.getLocals()) {
    auto const Layout = doLayout(Handler, Local, Expansion);
    
    DotStream << Layout.getDotString();
    
    Ports.addAllFrom(Layout.getPorts());
    
    auto const &AllocaArea = Layout.getArea();
    if (AllocaArea.end() > Area.end())
      Area.setEnd(AllocaArea.end());
    if (Area.start() == 0 || AllocaArea.start() < Area.start())
      Area.setStart(AllocaArea.start());
  }
  
  DotStream << "</TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfFunction{std::move(IDString),
                          std::move(DotString),
                          std::move(Area),
                          std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// Layout Thread
//===----------------------------------------------------------------------===//

static
LayoutOfThread
doLayout(LayoutHandler const &Handler,
         seec::cm::ThreadState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Generate the identifier for this subgraph.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "thread_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  std::vector<NodeInfo> FunctionNodes;
  
  DotStream << "subgraph " << IDString << " {\n"
            << "rank=same;\n";
  
  // Layout all functions.
  std::vector<LayoutOfFunction> FunctionLayouts;
  
  for (auto const &FunctionState : State.getCallStack()) {
    FunctionLayouts.emplace_back(doLayout(Handler, FunctionState, Expansion));
    
    auto const &Layout = FunctionLayouts.back();
    
    DotStream << Layout.getDotString();
    
    FunctionNodes.emplace_back(NodeType::Function,
                               Layout.getID(),
                               Layout.getArea(),
                               Layout.getPorts());
  }
  
  // Add edges to force function nodes to appear in order.
  if (FunctionLayouts.size() > 1) {
    auto const OrderEdgeCount = FunctionLayouts.size() - 1;
    
    for (unsigned i = 0; i < OrderEdgeCount; ++i) {
      DotStream << FunctionLayouts[i+1].getID()
                << ":sw -> "
                << FunctionLayouts[i].getID()
                << ":nw [style=invis weight=1000];\n";
    }
  }
  
  DotStream << "}\n";
  DotStream.flush();
  
  return LayoutOfThread{std::move(DotString), std::move(FunctionNodes)};
}


//===----------------------------------------------------------------------===//
// Layout Global Variable
//===----------------------------------------------------------------------===//

static
LayoutOfGlobalVariable
doLayout(LayoutHandler const &Handler,
         seec::cm::GlobalVariable const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  // Generate the identifier for this node.
  std::string IDString;
  
  {
    llvm::raw_string_ostream IDStream {IDString};
    IDStream << "global_at_" << reinterpret_cast<uintptr_t>(&State);
  }
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" "
                "CELLSPACING=\"0\" CELLBORDER=\"1\">"
            << "<TR><TD>"
            << State.getClangValueDecl()->getName()
            << "</TD>";
  
  MemoryArea Area;
  
  auto const Value = State.getValue();
  if (Value) {
    auto const MaybeLayout = Handler.doLayout(*Value, Expansion);
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      auto const &Layout = MaybeLayout.get<LayoutOfValue>();
      DotStream << Layout.getDotString();
      Ports.addAllFrom(Layout.getPorts());
    }
    
    if (Value->isInMemory()) {
      Area = MemoryArea(Value->getAddress(),
                        Value->getTypeSizeInChars().getQuantity());
    }
  }
  
  DotStream << "</TR></TABLE>> ];\n";
  DotStream.flush();
  
  return LayoutOfGlobalVariable{std::move(IDString),
                                std::move(DotString),
                                std::move(Area),
                                std::move(Ports)};
}


//===----------------------------------------------------------------------===//
// Layout Memory Area
//===----------------------------------------------------------------------===//

static
bool
isChildOfAnyDereference(std::shared_ptr<Value const> const &Child,
                        std::shared_ptr<ValueOfPointer const> const &Ptr)
{
  auto const Limit = Ptr->getDereferenceIndexLimit();
  
  for (unsigned i = 0; i < Limit; ++i) {
    auto const Pointee = Ptr->getDereferenced(i);
    if (isContainedChild(*Child, *Pointee))
      return true;
  }
  
  return false;
}

/// \brief Select a reference to an area and use it to perform the layout.
///
static
std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>
doLayout(LayoutHandler const &Handler,
         MemoryArea const &Area,
         Expansion const &Expansion)
{
  typedef std::shared_ptr<ValueOfPointer const> ValOfPtr;
  
  auto Refs = Expansion.getReferencesOfArea(Area.start(), Area.end());
  
  // TODO: Layout as an unreferenced area? We should only do this for mallocs.
  if (Refs.empty())
    return std::make_pair(seec::Maybe<LayoutOfArea>(), Area);
  
  if (Refs.size() == 1)
    return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion),
                          Area);
  
  // TODO: Select the user-selected ref, if there is one.
  
  // Move all the void pointers to the end of the list. If we have nothing but
  // void pointers then layout using any one of them, otherwise remove all of
  // them from consideration.
  auto const VoidIt =
    std::partition(Refs.begin(), Refs.end(),
                  [] (ValOfPtr const &Ptr) -> bool {
                    auto const CanTy = Ptr->getCanonicalType();
                    auto const PtrTy = llvm::cast<clang::PointerType>(CanTy);
                    return !PtrTy->getPointeeType()->isVoidType();
                  });
  
  if (VoidIt == Refs.begin())
    return std::make_pair(Handler.doLayout(Area, **VoidIt, Expansion), Area);
  
  Refs.erase(VoidIt, Refs.end());
  
  if (Refs.size() == 1)
    return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion),
                          Area);
  
  // Remove all pointers to incomplete types to the end of the list. If we have
  // nothing but pointers to incomplete types, then layout using any one of
  // them (we can't do the child reference removal below).
  auto const IncompleteIt =
    std::partition(Refs.begin(), Refs.end(),
                    [] (ValOfPtr const &Ptr) -> bool {
                      auto const CanTy = Ptr->getCanonicalType();
                      auto const PtrTy = llvm::cast<clang::PointerType>(CanTy);
                      return !PtrTy->getPointeeType()->isIncompleteType();
                    });
  
  if (IncompleteIt == Refs.begin())
    return std::make_pair(Handler.doLayout(Area, **IncompleteIt, Expansion),
                          Area);
  
  Refs.erase(IncompleteIt, Refs.end());
  
  if (Refs.size() == 1)
    return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion),
                          Area);
  
  // Remove all references which refer to a child of another reference. E.g. if
  // we have a pointer to a struct, and a pointer to a member of that struct,
  // then we should remove the member pointer (if the struct is selected for
  // layout then the pointer will be rendered correctly, otherwise it will be
  // rendered as punned).
  //
  // Also remove references which refer either to the same value as another
  // reference. If one of these references has a lower raw value then it should
  // be kept, as it will have more dereferences (and thus a more complete
  // layout will be produced using it).
  {
    auto const CurrentRefs = Refs;
    
    auto const RemovedIt =
      std::remove_if(Refs.begin(), Refs.end(),
        [&] (ValOfPtr const &Ptr) -> bool
        {
          auto const Pointee = Ptr->getDereferenced(0);
          return std::any_of(CurrentRefs.begin(), CurrentRefs.end(),
                    [&] (ValOfPtr const &Other) -> bool {
                      if (Ptr == Other)
                        return false;
                      
                      // Check if we directly reference another pointer's
                      // dereference. If so, don't bother checking children, as
                      // either us or the other pointer will be removed anyway.
                      auto const Direct = doReferenceSameValue(*Ptr, *Other);
                      if (Direct)
                        return Ptr->getRawValue() >= Other->getRawValue();
                      
                      // Check if this pointer directly references a child.
                      return isChildOfAnyDereference(Pointee, Other);
                    });
        });
    
    Refs.erase(RemovedIt, Refs.end());
  }
  
  if (Refs.size() == 1)
    return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion),
                          Area);
  
  // TODO: Layout as type-punned (or pass to a layout engine that supports
  //       multiple references).
  return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion), Area);
}


//===----------------------------------------------------------------------===//
// Render Pointers
//===----------------------------------------------------------------------===//

static void renderEdges(llvm::raw_string_ostream &DotStream,
                        std::vector<NodeInfo> const &AllNodeInfo,
                        Expansion const &Expansion)
{
  // Layout all pointers.
  for (auto const &Pointer : Expansion.getAllPointers()) {
    if (!Pointer->isInMemory())
      continue;
    
    // Don't layout null pointers.
    auto const HeadAddress = Pointer->getRawValue();
    if (!HeadAddress)
      continue;
    
    // Find the node that owns the pointee address.
    auto const HeadIt =
      std::find_if(AllNodeInfo.begin(),
                   AllNodeInfo.end(),
                   [=] (NodeInfo const &NI) { return NI.getArea()
                                                       .contains(HeadAddress);
                                            });
    
    if (HeadIt == AllNodeInfo.end())
      continue;
    
    // Find the node that owns the pointer's memory.
    auto const TailAddress = Pointer->getAddress();
    auto const TailIt =
      std::find_if(AllNodeInfo.begin(),
                   AllNodeInfo.end(),
                   [=] (NodeInfo const &NI) { return NI.getArea()
                                                       .contains(TailAddress);
                                            });
    
    if (TailIt == AllNodeInfo.end())
      continue;
    
    auto const MaybeTailPort = TailIt->getPortForValue(*Pointer);
    
    // Accumulate all attributes.
    std::string EdgeAttributes;
    bool IsPunned = false;
    
    // Write the tail.
    DotStream << TailIt->getID();
    
    if (MaybeTailPort.assigned<ValuePort>()) {
      // Tail port was explicitly defined during layout.
      auto const &TailPort = MaybeTailPort.get<ValuePort>();
      
      if (!TailPort.getCustomPort().empty())
        DotStream << ':' << TailPort.getCustomPort();
      else if (TailPort.getEdgeEnd() == EdgeEndType::Standard)
        DotStream << ':'
                  << getStandardPortFor(*Pointer)
                  << ":c";
      
      EdgeAttributes += "tailclip=false ";
    }
    else {
      // The tail port wasn't found, we must consider it punned.
      EdgeAttributes += "dir=both arrowtail=odot ";
      IsPunned = true;
    }
    
    // Write the arrow.
    DotStream << " -> ";
    
    // Write the head.
    DotStream << HeadIt->getID();
    
    if (Pointer->getDereferenceIndexLimit() != 0) {
      auto const Pointee = Pointer->getDereferenced(0);
      auto const MaybeHeadPort = HeadIt->getPortForValue(*Pointee);
      
      if (MaybeHeadPort.assigned<ValuePort>()) {
        // Tail port was explicitly defined during layout.
        auto const &HeadPort = MaybeHeadPort.get<ValuePort>();
        
        if (!HeadPort.getCustomPort().empty())
          DotStream << ':' << HeadPort.getCustomPort();
        else if (HeadPort.getEdgeEnd() == EdgeEndType::Standard)
          DotStream << ':'
                    << getStandardPortFor(*Pointee)
                    << ":nw";
      }
      else {
        if (HeadAddress == HeadIt->getArea().start())
          DotStream << ":nw";
        
        EdgeAttributes += "arrowhead=odot ";
        IsPunned = true;
      }
    }
    else {
      // There's no pointee value. Either the memory area is too small, or the
      // pointer's element type is incomplete. For now, make this look like a
      // punned pointer.
      if (HeadAddress == HeadIt->getArea().start())
        DotStream << ":nw";
      
      EdgeAttributes += "arrowhead=odot ";
      IsPunned = true;
    }
    
    if (IsPunned)
      EdgeAttributes += "style=dashed ";
    
    // Allow functions to be stacked vertically by forcing edges between
    // functions to not be used in ranking.
    if (HeadIt->getType() == NodeType::Function
        && TailIt->getType() == NodeType::Function)
      EdgeAttributes += "constraint=false ";
    
    // Write attributes.
    if (!EdgeAttributes.empty()) {
      EdgeAttributes.pop_back();
      DotStream << " [" << EdgeAttributes << "]";
    }
    
    DotStream << ";\n";
  }
}


//===----------------------------------------------------------------------===//
// Layout Process State
//===----------------------------------------------------------------------===//

static
LayoutOfProcess
doLayout(LayoutHandler const &Handler,
         seec::cm::ProcessState const &State,
         seec::cm::graph::Expansion const &Expansion)
{
  auto const TimeStart = std::chrono::steady_clock::now();
  
#ifndef _LIBCPP_VERSION

  // Create tasks to generate global variable layouts.
  std::vector<std::future<LayoutOfGlobalVariable>> GlobalVariableLayouts;
  
  auto const &Globals = State.getGlobalVariables();
  for (auto It = Globals.begin(), End = Globals.end(); It != End; ++It) {
    if ((*It)->isInSystemHeader())
      continue;
    
    GlobalVariableLayouts.emplace_back(
      std::async( [&, It] () {
                    return doLayout(Handler, **It, Expansion);
                  } ));
  }
  
  // Create tasks to generate thread layouts.
  auto const ThreadCount = State.getThreadCount();
  
  std::vector<std::future<LayoutOfThread>> ThreadLayouts;
  ThreadLayouts.reserve(ThreadCount);
  
  for (std::size_t i = 0; i < ThreadCount; ++i) {
    ThreadLayouts.emplace_back(
      std::async( [&, i] () {
                    return doLayout(Handler, State.getThread(i), Expansion);
                  } ));
  }
  
  // This will hold all of the general area layouts.
  std::vector<std::future<std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>>>
    AreaLayouts;
  
  // Generate layouts for unmapped static areas (unmapped globals).
  for (auto const &Area : State.getUnmappedStaticAreas()) {
    AreaLayouts.emplace_back(
      std::async([&, Area] () { return doLayout(Handler, Area, Expansion); }));
  }
  
  // Create tasks to generate malloc area layouts.
  for (auto const &Malloc : State.getDynamicMemoryAllocations()) {
    auto const Area = seec::MemoryArea(Malloc.getAddress(), Malloc.getSize());
    
    AreaLayouts.emplace_back(
      std::async([&, Area] () { return doLayout(Handler, Area, Expansion); } ));
  }
  
  // Create tasks to generate known memory area layouts.
  for (auto const &Known : State.getUnmappedProcessState().getKnownMemory()) {
    auto const Area = seec::MemoryArea(Known.Begin,
                                       (Known.End - Known.Begin) + 1,
                                       Known.Value);
    
    AreaLayouts.emplace_back(
      std::async([&, Area] () { return doLayout(Handler, Area, Expansion); } ));
  }

#else // _LIBCPP_VERSION

  // Generate global variable layouts.
  std::vector<LayoutOfGlobalVariable> GlobalVariableLayouts;
  
  for (auto const &Global : State.getGlobalVariables()) {
    if (Global->isInSystemHeader())
      continue;
    
    GlobalVariableLayouts.emplace_back(doLayout(Handler, *Global, Expansion));
  }
  
  // Generate thread layouts.
  std::vector<LayoutOfThread> ThreadLayouts;
  
  for (std::size_t i = 0; i < State.getThreadCount(); ++i)
    ThreadLayouts.emplace_back(doLayout(Handler,
                               State.getThread(i),
                               Expansion));
  
  // Generate area layouts.
  std::vector<std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>> AreaLayouts;
  
  for (auto const &Area : State.getUnmappedStaticAreas())
    AreaLayouts.emplace_back(doLayout(Handler, Area, Expansion));
  
  for (auto const &Malloc : State.getDynamicMemoryAllocations())
    AreaLayouts.emplace_back(doLayout(Handler,
                                      MemoryArea(Malloc.getAddress(),
                                                 Malloc.getSize()),
                                      Expansion));
  
  for (auto const &Known : State.getUnmappedProcessState().getKnownMemory())
    AreaLayouts.emplace_back(doLayout(Handler,
                                      MemoryArea(Known.Begin,
                                                 (Known.End - Known.Begin) + 1,
                                                 Known.Value),
                                      Expansion));
  
#endif // _LIBCPP_VERSION
  
  // Retrieve results and combine layouts.
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  std::vector<NodeInfo> AllNodeInfo;
  
  DotStream << "digraph Process {\n"
            << "node [shape=plaintext];\n" //  fontsize=6
            // << "penwidth=0.5;\n"
            << "rankdir=LR;\n";
  
  for (auto &GlobalFuture : GlobalVariableLayouts) {
#ifndef _LIBCPP_VERSION
    auto const Layout = GlobalFuture.get();
#else
    auto const &Layout = GlobalFuture;
#endif

    DotStream << Layout.getDotString();
    
    AllNodeInfo.emplace_back(NodeType::Global,
                             Layout.getID(),
                             Layout.getArea(),
                             Layout.getPorts());
  }
  
  for (auto &ThreadFuture : ThreadLayouts) {
#ifndef _LIBCPP_VERSION
    auto const Layout = ThreadFuture.get();
#else
    auto const &Layout = ThreadFuture;
#endif
    
    DotStream << Layout.getDotString();
    
    AllNodeInfo.insert(AllNodeInfo.end(),
                       Layout.getNodes().begin(),
                       Layout.getNodes().end());
  }
  
  for (auto &AreaFuture : AreaLayouts) {
#ifndef _LIBCPP_VERSION
    auto const Result = AreaFuture.get();
#else
    auto const &Result = AreaFuture;
#endif

    auto const &MaybeLayout = Result.first;
    if (!MaybeLayout.assigned<LayoutOfArea>())
      continue;
    
    auto const &Layout = MaybeLayout.get<LayoutOfArea>();
    
    DotStream << Layout.getDotString();
    
    AllNodeInfo.emplace_back(NodeType::None,
                             Layout.getID(),
                             Result.second,
                             Layout.getPorts());
  }
  
  // Render all of the pointers.
  renderEdges(DotStream, AllNodeInfo, Expansion);
  
  DotStream << "}\n"; // Close the digraph.
  DotStream.flush();
  
  auto const TimeEnd = std::chrono::steady_clock::now();
  auto const TimeTaken = TimeEnd - TimeStart;
  
  return LayoutOfProcess{std::move(DotString),
                         std::chrono::duration_cast<std::chrono::nanoseconds>
                                                   (TimeTaken)};
}


//===----------------------------------------------------------------------===//
// LayoutHandler - Layout Engine Handling
//===----------------------------------------------------------------------===//

void LayoutHandler::addBuiltinLayoutEngines() {
  // LayoutEngineForValue:
  addLayoutEngine(seec::makeUnique<LEVCString>(*this));
  addLayoutEngine(seec::makeUnique<LEVStandard>(*this));
  // addLayoutEngine(seec::makeUnique<LEVElideUnreferenced>(*this));
  
  // LayoutEngineForArea:
  addLayoutEngine(seec::makeUnique<LEACString>(*this));
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

std::vector<LayoutEngineForValue const *>
LayoutHandler::listLayoutEnginesSupporting(Value const &ForValue) const {
  std::vector<LayoutEngineForValue const *> List;
  
  for (auto const &EnginePtr : ValueEngines)
    if (EnginePtr->canLayout(ForValue))
      List.emplace_back(EnginePtr.get());
  
  return List;
}

bool
LayoutHandler::setLayoutEngine(Value const &ForValue, uintptr_t EngineID) {
  if (!ForValue.isInMemory())
    return false;
  
  // Attempt to find the engine.
  auto const EngineIt =
    std::find_if(ValueEngines.begin(), ValueEngines.end(),
                [=] (std::unique_ptr<LayoutEngineForValue> const &Engine) {
                  return reinterpret_cast<uintptr_t>(Engine.get()) == EngineID;
                });
  
  if (EngineIt == ValueEngines.end())
    return false;
  
  auto const Ptr = EngineIt->get();
  
  ValueEngineOverride[std::make_pair(ForValue.getAddress(),
                                     ForValue.getCanonicalType())] = Ptr;

  return true;
}


//===----------------------------------------------------------------------===//
// LayoutHandler - Layout Creation
//===----------------------------------------------------------------------===//

void LayoutHandler::writeHREF(llvm::raw_ostream &Out,
                              Value const &ForValue) const
{
  Out << " HREF=\"value "
      << reinterpret_cast<uintptr_t>(&ForValue)
      << "\"";
}

void LayoutHandler::writeStandardProperties(llvm::raw_ostream &Out,
                                            Value const &ForValue) const
{
  writeHREF(Out, ForValue);
}

seec::Maybe<LayoutOfValue>
LayoutHandler::doLayout(seec::cm::Value const &State, Expansion const &E) const
{
  // If there's an engine for this exact Value, try to use that.
  if (State.isInMemory()) {
    auto const It =
      ValueEngineOverride.find(std::make_pair(State.getAddress(),
                                              State.getCanonicalType()));
    
    if (It != ValueEngineOverride.end() && It->second->canLayout(State))
      return It->second->doLayout(State, E);
  }
  
  // Otherwise try to use the user-selected global default.
  if (ValueEngineDefault && ValueEngineDefault->canLayout(State))
    return ValueEngineDefault->doLayout(State, E);
  
  // Otherwise try to use any engine that will work.
  for (auto const &EnginePtr : ValueEngines)
    if (EnginePtr->canLayout(State))
      return EnginePtr->doLayout(State, E);
  
  return seec::Maybe<LayoutOfValue>();
}

seec::Maybe<LayoutOfArea>
LayoutHandler::doLayout(seec::MemoryArea const &Area,
                        seec::cm::ValueOfPointer const &Reference,
                        Expansion const &Exp) const
{
  for (auto const &EnginePtr : AreaEngines)
    if (EnginePtr->canLayout(Area, Reference))
      return EnginePtr->doLayout(Area, Reference, Exp);
  
  return seec::Maybe<LayoutOfArea>();
}

LayoutOfProcess
LayoutHandler::doLayout(seec::cm::ProcessState const &State) const
{
  return seec::cm::graph::doLayout(*this, State, Expansion::from(State));
}


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec
