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


#include "seec/Clang/GraphExpansion.hpp"
#include "seec/Clang/GraphLayout.hpp"
#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/MakeUnique.hpp"

#include "clang/AST/Decl.h"

#include "llvm/Support/raw_ostream.h"

#include "unicode/locid.h"
#include "unicode/unistr.h"

#include <algorithm>
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

static std::string EscapeForHTML(UnicodeString const &String)
{
  auto const Length = String.length();
  
  std::string Escaped;
  Escaped.reserve(Length);
  
  for (int32_t i = 0; i < Length; ++i) {
    auto const Char = String[i];
    
    if (std::isalnum(Char))
      Escaped.push_back(Char);
    else {
      Escaped.push_back('&');
      Escaped.push_back('#');
      Escaped += std::to_string(Char);
      Escaped.push_back(';');
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
      
      Stream << '>';
      
      if (IsInit)
        Stream << EscapeForHTML(V.getValueAsStringFull());
      else
        Stream << ' ';
      
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
          Stream << "<TD> </TD></TR>";
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
          Stream << "<TD> </TD></TR>";
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
      else if (Ptr.getDereferenceIndexLimit() != 0 || Ptr.isValidOpaque()) {
        // A valid pointer.
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"";
        
        getHandler().writeStandardProperties(Stream, V);
        
        Stream << "> </TD>";
      }
      else {
        // An invalid pointer (as far as we're concerned).
        Stream << "<TD PORT=\""
               << getStandardPortFor(V)
               << "\"";
        
        getHandler().writeStandardProperties(Stream, V);
        
        Stream << ">!</TD>";
      }
      
      break;
    }
  }
  
  if (V.getKind() == Value::Kind::Pointer || E.isReferencedDirectly(V))
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

  if (E.isReferencedDirectly(V))
    Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  auto const &Handler = getHandler();
  auto const &Array = static_cast<ValueOfArray const &>(V);
  unsigned const ChildCount = Array.getChildCount();
  
  Stream << "<TD PORT=\""
         << getStandardPortFor(V)
         << "\"";
  
  Handler.writeStandardProperties(Stream, V);
  
  Stream << "><TABLE BORDER=\"0\" CELLPADDING=\"0\" "
              "CELLSPACING=\"0\" CELLBORDER=\"1\"><TR>";
  
  bool Eliding = false;
  std::size_t ElidingCount;
  
  for (unsigned i = 0; i < ChildCount; ++i) {
    auto const ChildValue = Array.getChildAt(i);
    if (!ChildValue) {
      if (!Eliding)
        Stream << "<TD> </TD>";
      continue;
    }
    
    if (Eliding) {
      if (E.isReferencedDirectly(*ChildValue)) {
        // This char is referenced. Stop eliding and resume layout. Write a cell
        // to represent the elided characters.
        Stream << "<TD> </TD>";
        Eliding = false;
      }
      else {
        // Elide this char and move to the next.
        ++ElidingCount;
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
      Stream << "> </TD>";
      
      if (E.isReferencedDirectly(*ChildValue))
        Ports.add(*ChildValue, ValuePort{EdgeEndType::Standard});
    }
    
    // If this was a terminating null character, start eliding.
    auto const &Scalar = static_cast<ValueOfScalar const &>(*ChildValue);
    if (Scalar.isZero()) {
      Eliding = true;
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
  /// Placeholder text to use for elided values.
  UnicodeString ElidedText;
  
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
  : LayoutEngineForValue{InHandler},
    ElidedText()
  {
    // Attempt to load placeholder text from the resource bundle.
    auto const MaybeText =
      seec::getString("SeeCClang",
                      (char const *[]){ "Graph", "Layout",
                                        "LEVElideUnreferenced", "Elided" });
    
    if (MaybeText.assigned<UnicodeString>())
      ElidedText = MaybeText.get<UnicodeString>();
  }
  
  ~LEVElideUnreferenced() noexcept(true) {}
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

  if (E.isReferencedDirectly(V))
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
  
  auto const ArrayStart = Array.getAddress();
  auto const ArrayEnd = ArrayStart + Array.getTypeSizeInChars().getQuantity();
  if (!E.isAreaReferenced(ArrayStart, ArrayEnd)) {
    Eliding = true;
    ElidingFrom = 0;
    ElidedPort = getStandardPortFor(V) + "_elided_" +
                 std::to_string(ElidingFrom);
  }
  else if (ChildCount > 0) {
    auto ChildValue = Array.getChildAt(0);
    if (ChildValue) {
      auto const FirstAddress = ChildValue->getAddress();
      auto const ChildSize = ChildValue->getTypeSizeInChars().getQuantity();
      
      for (unsigned i = 0; i < ChildCount; ++i) {
        auto const Start = FirstAddress + (i * ChildSize);
        auto const End = Start + ChildSize;
        
        // If the area occupied by the child is referenced, then we check if the
        // child itself is referenced (to account for type-punning). Otherwise
        // we won't generate the child Value (as it is only used if referenced).
        
        bool IsReferenced = E.isAreaReferenced(Start, End);
        if (IsReferenced) {
          ChildValue = Array.getChildAt(i);
          if (!ChildValue)
            continue;
          
          IsReferenced = searchChildren(*ChildValue,
                                        [&] (Value const &V) {
                                          return E.isReferencedDirectly(V);
                                        });
        }
        
        if (!IsReferenced) {
          if (!Eliding) {
            Eliding = true;
            ElidingFrom = i;
            ElidedPort = getStandardPortFor(V) + "_elided_" +
                         std::to_string(ElidingFrom);
          }
          
          continue;
        }
        
        if (Eliding) {
          // This element is referenced. Stop eliding and resume layout.
          Eliding = false;
          
          // Write a row for the elided elements.
          Stream << "<TR><TD PORT=\"" << ElidedPort
                 << "\">&#91;" << ElidingFrom;
          if (ElidingFrom < i - 1)
            Stream << " &#45; " << (i - 1);
          Stream << "&#93;</TD><TD>";
          
          // Attempt to format and insert the elision placeholder text.
          UErrorCode Status = U_ZERO_ERROR;
          auto const Formatted = seec::format(ElidedText, Status,
                                              int64_t(i - ElidingFrom));
          if (U_SUCCESS(Status))
            Stream << EscapeForHTML(Formatted);
          
          Stream << "</TD></TR>";
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
          Stream << "<TD> </TD></TR>";
        }
      }
    }
  }
  
  // Write one final row for the trailing elided elements.
  if (Eliding) {
    Stream << "<TR><TD PORT=\"" << ElidedPort << "\">&#91;" << ElidingFrom;
    if (ElidingFrom < ChildCount - 1)
      Stream << " &#45; " << (ChildCount - 1);
    Stream << "&#93;</TD><TD>";
    
    // Attempt to format and insert the elision placeholder text.
    UErrorCode Status = U_ZERO_ERROR;
    auto const Formatted = seec::format(ElidedText, Status,
                                        int64_t(ChildCount - ElidingFrom));
    if (U_SUCCESS(Status))
      Stream << EscapeForHTML(Formatted);
    
    Stream << "</TD></TR>";
  }
  
  Stream << "</TABLE></TD>";
  Stream.flush();
  
  return LayoutOfValue(std::move(DotString), std::move(Ports));
}


//===----------------------------------------------------------------------===//
// LEVElideEmptyUnreferencedStrings
//===----------------------------------------------------------------------===//

/// \brief Value layout engine "Elide empty unreferenced strings".
///
class LEVElideEmptyUnreferencedStrings final : public LayoutEngineForValue {
  /// Placeholder text to use for elided values.
  UnicodeString ElidedText;
  
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout",
                                     "LEVElideEmptyUnreferencedStrings",
                                     "Name"});
  }
  
  virtual bool canLayoutImpl(Value const &V) const override;
  
  virtual LayoutOfValue
  doLayoutImpl(Value const &V, Expansion const &E) const override;
  
public:
  LEVElideEmptyUnreferencedStrings(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler},
    ElidedText()
  {
    // Attempt to load placeholder text from the resource bundle.
    auto const MaybeText =
      seec::getString("SeeCClang",
                      (char const *[]){ "Graph", "Layout",
                                        "LEVElideEmptyUnreferencedStrings",
                                        "Elided" });
    
    if (MaybeText.assigned<UnicodeString>())
      ElidedText = MaybeText.get<UnicodeString>();
  }
  
  ~LEVElideEmptyUnreferencedStrings() noexcept(true) {}
};

bool LEVElideEmptyUnreferencedStrings::canLayoutImpl(Value const &V) const
{
  if (V.getKind() != Value::Kind::Array)
    return false;
  
  auto const ArrayTy = llvm::dyn_cast<clang::ArrayType>(V.getCanonicalType());
  if (!ArrayTy)
    return false;
  
  auto const ElemTy = ArrayTy->getElementType().getTypePtrOrNull();
  if (!ElemTy)
    return false;
  
  auto const ElemArrayTy = llvm::dyn_cast<clang::ArrayType>(ElemTy);
  if (!ElemArrayTy)
    return false;
  
  return ElemArrayTy->getElementType()->isAnyCharacterType();
}

LayoutOfValue
LEVElideEmptyUnreferencedStrings::doLayoutImpl(Value const &V,
                                               Expansion const &E) const
{
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;

  if (E.isReferencedDirectly(V))
    Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  auto const &Handler = getHandler();
  auto const &Array = llvm::cast<ValueOfArray const>(V);
  unsigned const ChildCount = Array.getChildCount();
  
  Stream << "<TD PORT=\""
         << getStandardPortFor(V)
         << "\"";
  Handler.writeStandardProperties(Stream, V);
  Stream << ">";
  
  // Exit early if the array has no children.
  if (ChildCount == 0) {
    Stream << "</TD>";
    Stream.flush();
    return LayoutOfValue(std::move(DotString), std::move(Ports));
  }
  
  Stream << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
  
  bool Eliding = false;
  std::size_t ElidingFrom;
  std::string ElidedPort;
  
  assert(Array.isInMemory());
  auto const FirstAddress = Array.getAddress();
  auto const ChildSize    = Array.getChildSize();
  auto const Region       = Array.getUnmappedMemoryRegion().get<0>();

  auto const Data = Region.getByteValues();
  auto const Init = Region.getByteInitialization();

  if (ChildCount * ChildSize <= Data.size()) {
    for (unsigned i = 0; i < ChildCount; ++i) {
      auto const Offset = (i * ChildSize);
      auto const IsEmpty =
        Init[Offset] != std::numeric_limits<unsigned char>::max()
        || Data[Offset] == 0;

      auto const Start = FirstAddress + Offset;
      auto const End   = Start + ChildSize;
      auto const IsReferenced = E.isAreaReferenced(Start, End);

      if (IsEmpty && !IsReferenced) {
        if (!Eliding) {
          Eliding = true;
          ElidingFrom = i;
          ElidedPort = getStandardPortFor(V) + "_elided_" +
                        std::to_string(ElidingFrom);
        }

        continue;
      }

      auto const ChildValue = Array.getChildAt(i);
      if (!ChildValue)
        continue;

      if (Eliding) {
        // This element is non-empty or referenced, so stop eliding and
        // resume layout.
        Eliding = false;

        // Write a row for the elided elements.
        Stream << "<TR><TD PORT=\"" << ElidedPort
                << "\">&#91;" << ElidingFrom;
        if (ElidingFrom < i - 1)
          Stream << " &#45; " << (i - 1);
        Stream << "&#93;</TD><TD>";

        // Attempt to format and insert the elision placeholder text.
        UErrorCode Status = U_ZERO_ERROR;
        auto const Formatted = seec::format(ElidedText, Status,
                                            int64_t(i - ElidingFrom));
        if (U_SUCCESS(Status))
          Stream << EscapeForHTML(Formatted);

        Stream << "</TD></TR>";
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
        Stream << "<TD> </TD></TR>";
      }
    }
  }
  
  // Write one final row for the trailing elided elements.
  if (Eliding) {
    Stream << "<TR><TD PORT=\"" << ElidedPort << "\">&#91;" << ElidingFrom;
    if (ElidingFrom < ChildCount - 1)
      Stream << " &#45; " << (ChildCount - 1);
    Stream << "&#93;</TD><TD>";
    
    // Attempt to format and insert the elision placeholder text.
    UErrorCode Status = U_ZERO_ERROR;
    auto const Formatted = seec::format(ElidedText, Status,
                                        int64_t(ChildCount - ElidingFrom));
    if (U_SUCCESS(Status))
      Stream << EscapeForHTML(Formatted);
    
    Stream << "</TD></TR>";
  }
  
  Stream << "</TABLE></TD>";
  Stream.flush();
  
  return LayoutOfValue(std::move(DotString), std::move(Ports));
}


//===----------------------------------------------------------------------===//
// LEVElideUninitOrZeroElements
//===----------------------------------------------------------------------===//

/// \brief Value layout engine "Elide uninitialized or zero elements".
///
class LEVElideUninitOrZeroElements final : public LayoutEngineForValue {
  /// Placeholder text to use for elided values.
  UnicodeString ElidedText;
  
  virtual std::unique_ptr<seec::LazyMessage> getNameImpl() const override {
    return LazyMessageByRef::create("SeeCClang",
                                    {"Graph", "Layout",
                                     "LEVElideUninitOrZeroElements",
                                     "Name"});
  }
  
  virtual bool canLayoutImpl(Value const &V) const override;
  
  virtual LayoutOfValue
  doLayoutImpl(Value const &V, Expansion const &E) const override;
  
public:
  LEVElideUninitOrZeroElements(LayoutHandler const &InHandler)
  : LayoutEngineForValue{InHandler},
    ElidedText()
  {
    // Attempt to load placeholder text from the resource bundle.
    auto const MaybeText =
      seec::getString("SeeCClang",
                      (char const *[]){ "Graph", "Layout",
                                        "LEVElideUninitOrZeroElements",
                                        "Elided" });
    
    if (MaybeText.assigned<UnicodeString>())
      ElidedText = MaybeText.get<UnicodeString>();
  }
  
  ~LEVElideUninitOrZeroElements() noexcept(true) {}
};

bool LEVElideUninitOrZeroElements::canLayoutImpl(Value const &V) const
{
  if (V.getKind() != Value::Kind::Array)
    return false;
  
  auto const ArrayTy = llvm::dyn_cast<clang::ArrayType>(V.getCanonicalType());
  if (!ArrayTy)
    return false;
  
  auto const ElemTy = ArrayTy->getElementType().getTypePtrOrNull();
  if (!ElemTy)
    return false;
  
  return !ElemTy->isAnyPointerType();
}

LayoutOfValue
LEVElideUninitOrZeroElements::doLayoutImpl(Value const &V,
                                           Expansion const &E) const
{
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;

  if (E.isReferencedDirectly(V))
    Ports.add(V, ValuePort{EdgeEndType::Standard});
  
  auto const &Handler = getHandler();
  auto const &Array = llvm::cast<ValueOfArray const>(V);
  
  Stream << "<TD PORT=\""
         << getStandardPortFor(V)
         << "\"";
  Handler.writeStandardProperties(Stream, V);
  Stream << ">";
  
  assert(Array.isInMemory());

  auto const FirstAddress = Array.getAddress();
  auto const ChildCount   = Array.getChildCount();
  auto const ChildSize    = Array.getChildSize();
  auto const Region       = Array.getUnmappedMemoryRegion().get<0>();

  assert(Region.getByteValues().size() >= ChildCount * ChildSize);

  // Exit early if the array has no children, or if the memory region isn't
  // sufficiently large to contain all of the children.
  if (ChildCount == 0) {
    Stream << "</TD>";
    Stream.flush();
    return LayoutOfValue(std::move(DotString), std::move(Ports));
  }
  
  Stream << "<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLBORDER=\"1\">";
  
  bool Eliding = false;
  std::size_t ElidingFrom;
  std::string ElidedPort;
  
  for (unsigned i = 0; i < ChildCount; ++i) {
    auto const Start = FirstAddress + (i * ChildSize);
    auto const SubRegion = Region.getState().getRegion(MemoryArea(Start,
                                                                  ChildSize));

    auto const Data = SubRegion.getByteValues();
    auto const IsEmpty = SubRegion.isUninitialized()
      || std::all_of(Data.begin(), Data.end(),
                     [] (char const C) { return C == 0; });

    auto const IsReferenced = E.isAreaReferenced(Start, Start + ChildSize);
    
    if (IsEmpty && !IsReferenced) {
      if (!Eliding) {
        Eliding = true;
        ElidingFrom = i;
        ElidedPort = getStandardPortFor(V) + "_elided_" +
                     std::to_string(ElidingFrom);
      }
      
      continue;
    }
    
    if (Eliding) {
      // This element is non-zero or referenced, so stop eliding.
      Eliding = false;
      
      // Write a row for the elided elements.
      Stream << "<TR><TD PORT=\"" << ElidedPort
             << "\">&#91;" << ElidingFrom;
      if (ElidingFrom < i - 1)
        Stream << " &#45; " << (i - 1);
      Stream << "&#93;</TD><TD>";
      
      // Attempt to format and insert the elision placeholder text.
      UErrorCode Status = U_ZERO_ERROR;
      auto const Formatted = seec::format(ElidedText, Status,
                                          int64_t(i - ElidingFrom));
      if (U_SUCCESS(Status))
        Stream << EscapeForHTML(Formatted);
      
      Stream << "</TD></TR>";
    }
    
    // Layout this referenced value.
    auto const ChildValue = Array.getChildAt(i);
    if (!ChildValue)
      continue;
    
    Stream << "<TR><TD>&#91;" << i << "&#93;</TD>";
    
    auto const MaybeLayout = Handler.doLayout(*ChildValue, E);
    if (MaybeLayout.assigned<LayoutOfValue>()) {
      auto const &Layout = MaybeLayout.get<LayoutOfValue>();
      Stream << Layout.getDotString() << "</TR>";
      Ports.addAllFrom(Layout.getPorts());
    }
    else {
      Stream << "<TD> </TD></TR>";
    }
  }
  
  // Write one final row for the trailing elided elements.
  if (Eliding) {
    Stream << "<TR><TD PORT=\"" << ElidedPort << "\">&#91;" << ElidingFrom;
    if (ElidingFrom < ChildCount - 1)
      Stream << " &#45; " << (ChildCount - 1);
    Stream << "&#93;</TD><TD>";
    
    // Attempt to format and insert the elision placeholder text.
    UErrorCode Status = U_ZERO_ERROR;
    auto const Formatted = seec::format(ElidedText, Status,
                                        int64_t(ChildCount - ElidingFrom));
    if (U_SUCCESS(Status))
      Stream << EscapeForHTML(Formatted);
    
    Stream << "</TD></TR>";
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
  auto const IDString = std::string{"area_at_"} + std::to_string(Area.start());
  auto const &Handler = this->getHandler();
  
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  ValuePortMap Ports;
  
  DotStream << IDString
            << " [ label = <"
               "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLPADDING=\"2\"";
  Handler.writeHREF(DotStream, Area, Reference);
  DotStream << "><TR><TD>"
               "<TABLE"
               " BORDER=\"0\""
               " CELLSPACING=\"0\""
               " CELLBORDER=\"1\""
               " CELLPADDING=\"0\">";
  
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
    for (int i = 0; i < Limit; ++i) {
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
    DotStream << "<TR><TD> </TD></TR>";
  }
  
  DotStream << "</TABLE></TD></TR></TABLE>> ];\n";
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
  auto const IDString = std::string{"area_at_"} + std::to_string(Area.start());
  auto const &Handler = this->getHandler();
  
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;

  if (E.isReferencedDirectly(Reference))
    Ports.add(Reference, ValuePort{EdgeEndType::Standard});
  
  Stream << IDString
         << " [ label = <"
            "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLPADDING=\"2\"";
  Handler.writeHREF(Stream, Area, Reference);
  Stream << "><TR><TD>"
            "<TABLE"
            " BORDER=\"0\""
            " CELLSPACING=\"0\""
            " CELLBORDER=\"1\""
            " CELLPADDING=\"0\">"
            "<TR>";
  
  auto const Limit = Reference.getDereferenceIndexLimit();
  
  bool Eliding = false;
  std::size_t ElidingFrom;
  std::size_t ElidingCount;
  
  for (int i = 0; i < Limit; ++i) {
    auto const ChildValue = Reference.getDereferenced(i);
    if (!ChildValue) {
      if (!Eliding)
        Stream << "<TD> </TD>";
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
                 << "\"> </TD>";
        }
        
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
      Stream << "> </TD>";

      if (E.isReferencedDirectly(*ChildValue))
        Ports.add(*ChildValue, ValuePort{EdgeEndType::Standard});
    }
    
    // If this was a terminating null character, start eliding.
    auto const &Scalar = static_cast<ValueOfScalar const &>(*ChildValue);
    if (Scalar.isCompletelyInitialized() && Scalar.isZero()) {
      Eliding = true;
      ElidingFrom = i + 1;
      ElidingCount = 0;
    }
  }
  
  Stream << "</TR></TABLE></TD></TR></TABLE>> ];\n";
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
  
  DotStream << "<TR><TD HREF=\"local "
            << reinterpret_cast<uintptr_t>(&State) << "\">"
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
  
  DotStream << "<TR><TD HREF=\"param "
            << reinterpret_cast<uintptr_t>(&State) << "\">"
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
               "<TR><TD COLSPAN=\"2\" PORT=\"fname\" COLOR=\"#268bd2\">"
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
  
  DotStream << "subgraph " << IDString << " {\n";
  
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
                << ":fname -> "
                << FunctionLayouts[i].getID()
                << ":fname "
                   "[dir=back color=\"#268bd2\""
                   " style=\"dashed\"];\n";
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
  
  auto const TheDecl = State.getClangValueDecl();
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  DotStream << IDString
            << " [ label = <"
            << "<TABLE BORDER=\"0\" "
                "CELLSPACING=\"0\" CELLBORDER=\"1\" HREF=\"global "
            << reinterpret_cast<uintptr_t>(&State) << "\"><TR><TD>";

  if (auto const TheVarDecl = llvm::dyn_cast<clang::VarDecl>(TheDecl)) {
    if (TheVarDecl->isStaticLocal()) {
      // Attempt to find the FunctionDecl that owns this VarDecl.
      auto Context = TheVarDecl->getDeclContext();
      while (!llvm::isa<clang::FunctionDecl>(Context))
        Context = Context->getLexicalParent();
      auto const OwningFn = llvm::cast<clang::FunctionDecl>(Context);
      DotStream << OwningFn->getName() << " :: ";
    }
  }

  DotStream << TheDecl->getName() << "</TD>";
  
  ValuePortMap Ports;
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

/// \brief Generate a placeholder layout for an unreferenced memory area.
///
static
std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>
layoutUnreferencedArea(LayoutHandler const &Handler,
                       MemoryArea const &Area,
                       AreaType const Type)
{
  // Don't generate placeholder layouts for unreferenced static memory.
  switch (Type) {
    case AreaType::Static:
      return std::make_pair(seec::Maybe<LayoutOfArea>(), Area);
    
    case AreaType::Dynamic:
      break;
  }
  
  // Generate the identifier for this node.
  auto const IDString = std::string{"area_at_"} + std::to_string(Area.start());
  
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  
  Stream << IDString
         << " [ label = <"
            "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLPADDING=\"2\"";
  // TODO: Make a href for unreferenced areas.
  Stream << "><TR><TD COLOR=\"#dc322f\">";
  
  // Attempt to load placeholder text from the resource bundle.
  auto const MaybeText =
    seec::getString("SeeCClang",
                    (char const *[]){"Graph", "Descriptions",
                                     "UnreferencedDynamic"});
  
  if (MaybeText.assigned<UnicodeString>()) {
    // Attempt to format and insert the elision placeholder text.
    UErrorCode Status = U_ZERO_ERROR;
    auto const Formatted = seec::format(MaybeText.get<UnicodeString>(), Status,
                                        int64_t(Area.length()));
    if (U_SUCCESS(Status))
      Stream << EscapeForHTML(Formatted);
  }
  
  Stream << "</TD></TR></TABLE>> ];\n";
  Stream.flush();
  
  return std::make_pair(seec::Maybe<LayoutOfArea>
                                   (LayoutOfArea{std::move(IDString),
                                    std::move(DotString),
                                    ValuePortMap{}}),
                        Area);
}

/// \brief Select a reference to an area and use it to perform the layout.
///
static
std::pair<seec::Maybe<LayoutOfArea>, MemoryArea>
doLayout(LayoutHandler const &Handler,
         MemoryArea const &Area,
         Expansion const &Expansion,
         AreaType const Type)
{
  typedef std::shared_ptr<ValueOfPointer const> ValOfPtr;
  
  auto Refs = Expansion.getReferencesOfArea(Area.start(), Area.end());
  
  // Layout as an unreferenced area? We should only do this for mallocs.
  if (Refs.empty())
    return layoutUnreferencedArea(Handler, Area, Type);
  
  if (Refs.size() == 1)
    return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion),
                          Area);
  
  // Use the user-selected ref, if there is one.
  auto const OverrideType = Handler.getAreaReferenceType(Area);
  if (OverrideType) {
    auto const OverrideIt =
      std::find_if(Refs.begin(), Refs.end(),
                    [=] (ValOfPtr const &Ptr) -> bool {
                      return Ptr->getCanonicalType() == OverrideType;
                    });
    
    if (OverrideIt != Refs.end())
      return std::make_pair(Handler.doLayout(Area, **OverrideIt, Expansion),
                            Area);
  }
  
  // Remove pointers to void, incomplete types, or to children of other
  // pointees (e.g. pointers to struct members).
  seec::cm::graph::reduceReferences(Refs);
  assert(!Refs.empty());
  
  if (Refs.size() == 1)
    return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion),
                          Area);
  
  // TODO: Layout as type-punned (or pass to a layout engine that supports
  //       multiple references).
  return std::make_pair(Handler.doLayout(Area, *Refs.front(), Expansion), Area);
}


//===----------------------------------------------------------------------===//
// Render Streams
//===----------------------------------------------------------------------===//

static
std::pair<Maybe<LayoutOfArea>, MemoryArea>
doLayout(StreamState const &State, Expansion const &Expansion)
{
  auto const Address = State.getAddress();
  
  // Don't render unreferenced streams.
  if (State.isstd() && !Expansion.isAreaReferenced(Address, Address + 1))
    return std::make_pair(Maybe<LayoutOfArea>(), MemoryArea{Address, 1});
  
  // Generate the identifier for this node.
  auto const IDString = std::string{"area_at_"} + std::to_string(Address);
  
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  
  Stream << IDString
         << " [ label = <"
            "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLPADDING=\"2\"";
  // TODO: Make a href for unreferenced Streams?
  Stream << "><TR><TD PORT=\"opaque\">";
  
  // Attempt to load text from the resource bundle.
  auto const MaybeText =
    seec::getString("SeeCClang",
                    (char const *[]){"Graph", "Descriptions", "Stream"});
  
  if (MaybeText.assigned<UnicodeString>()) {
    // Attempt to format and insert the text.
    UErrorCode Status = U_ZERO_ERROR;
    auto const Formatted = seec::format(MaybeText.get<UnicodeString>(), Status,
                                        State.getFilename().c_str());
    if (U_SUCCESS(Status))
      Stream << EscapeForHTML(Formatted);
  }
  
  Stream << "</TD></TR></TABLE>> ];\n";
  Stream.flush();
  
  return std::make_pair(Maybe<LayoutOfArea>
                             (LayoutOfArea{std::move(IDString),
                              std::move(DotString),
                              ValuePortMap{}}),
                        MemoryArea{Address, 1});
}


//===----------------------------------------------------------------------===//
// Render DIRs
//===----------------------------------------------------------------------===//

static
std::pair<Maybe<LayoutOfArea>, MemoryArea>
doLayout(DIRState const &State, Expansion const &Expansion)
{
  auto const Address = State.getAddress();
  
  // Generate the identifier for this node.
  auto const IDString = std::string{"area_at_"} + std::to_string(Address);
  
  std::string DotString;
  llvm::raw_string_ostream Stream {DotString};
  
  ValuePortMap Ports;
  
  Stream << IDString
         << " [ label = <"
            "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLPADDING=\"2\"";
  // TODO: Make a href for unreferenced DIRs?
  Stream << "><TR><TD PORT=\"opaque\">";
  
  // Attempt to load text from the resource bundle.
  auto const MaybeText =
    seec::getString("SeeCClang",
                    (char const *[]){"Graph", "Descriptions", "DIR"});
  
  if (MaybeText.assigned<UnicodeString>()) {
    // Attempt to format and insert the text.
    UErrorCode Status = U_ZERO_ERROR;
    auto const Formatted = seec::format(MaybeText.get<UnicodeString>(), Status,
                                        State.getDirname().c_str());
    if (U_SUCCESS(Status))
      Stream << EscapeForHTML(Formatted);
  }
  
  Stream << "</TD></TR></TABLE>> ];\n";
  Stream.flush();
  
  return std::make_pair(Maybe<LayoutOfArea>
                             (LayoutOfArea{std::move(IDString),
                              std::move(DotString),
                              ValuePortMap{}}),
                        MemoryArea{Address, 1});
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
    std::string EdgeAttributes = "href=\"dereference "
                               + std::to_string(reinterpret_cast<uintptr_t>
                                                                (Pointer.get()))
                               + "\" ";
    bool IsPunned = false;
    
    // Write the tail.
    DotStream << TailIt->getID();
    
    if (MaybeTailPort.assigned<ValuePort>()) {
      // Tail port was explicitly defined during layout.
      auto const &TailPort = MaybeTailPort.get<ValuePort>();
      
      if (TailPort.getEdgeEnd() == EdgeEndType::Standard)
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
        
        if (HeadPort.getEdgeEnd() == EdgeEndType::Standard)
          DotStream << ':'
                    << getStandardPortFor(*Pointee)
                    << ":nw";
      }
      else {
        if (HeadAddress == HeadIt->getArea().start())
          DotStream << ":nw";
        
        EdgeAttributes += "arrowhead=onormal ";
        IsPunned = true;
      }
    }
    else if (Pointer->isValidOpaque()) {
      if (HeadAddress == HeadIt->getArea().start())
        DotStream << ":opaque:nw";
    }
    else {
      // There's no pointee value. Either the memory area is too small, or the
      // pointer's element type is incomplete. For now, make this look like a
      // punned pointer.
      if (HeadAddress == HeadIt->getArea().start())
        DotStream << ":nw";
      
      EdgeAttributes += "arrowhead=onormal ";
      IsPunned = true;
    }
    
    if (IsPunned)
      EdgeAttributes += "style=dashed ";

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
         seec::cm::graph::Expansion const &Expansion,
         std::atomic_bool &CancelIfFalse)
{
  auto const TimeStart = std::chrono::steady_clock::now();
  
  // Create tasks to generate global variable layouts.
  std::vector<std::future<LayoutOfGlobalVariable>> GlobalVariableLayouts;
  
  auto const &Globals = State.getGlobalVariables();
  for (auto It = Globals.begin(), End = Globals.end(); It != End; ++It) {
    if ((*It)->isInSystemHeader() && !(*It)->isReferenced())
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
      std::async([&, Area] () {
        return doLayout(Handler, Area, Expansion, AreaType::Static); }));
  }
  
  // Create tasks to generate malloc area layouts.
  for (auto const &Malloc : State.getDynamicMemoryAllocations()) {
    auto const Area = seec::MemoryArea(Malloc.getAddress(), Malloc.getSize());
    
    AreaLayouts.emplace_back(
      std::async([&, Area] () {
        return doLayout(Handler, Area, Expansion, AreaType::Dynamic); } ));
  }
  
  // Create tasks to generate known memory area layouts.
  for (auto const &Known : State.getUnmappedProcessState().getKnownMemory()) {
    auto const Area = seec::MemoryArea(Known.Begin,
                                       (Known.End - Known.Begin) + 1,
                                       Known.Value);
    
    AreaLayouts.emplace_back(
      std::async([&, Area] () {
        return doLayout(Handler, Area, Expansion, AreaType::Static); } ));
  }
  
  // Generate stream layouts.
  for (auto const &Stream : State.getStreams()) {
    AreaLayouts.emplace_back(
      std::async([&, Stream] () {
        return doLayout(Stream.second, Expansion); } ));
  }
  
  // Generate DIR layouts.
  for (auto const &Dir : State.getDIRs()) {
    AreaLayouts.emplace_back(
      std::async([&, Dir] () {
        return doLayout(Dir.second, Expansion); } ));
  }
  
  // Retrieve results and combine layouts.
  std::string DotString;
  llvm::raw_string_ostream DotStream {DotString};
  
  std::vector<NodeInfo> AllNodeInfo;
  
  DotStream << "digraph Process {\n"
            << "node [shape=plaintext fontname=\"Monospace\"];\n" //  fontsize=6
            // << "penwidth=0.5;\n"
            << "rankdir=LR;\n";
  
  for (auto &GlobalFuture : GlobalVariableLayouts) {
    if (CancelIfFalse == false)
      return LayoutOfProcess{std::string{}, std::chrono::nanoseconds{0}};

    auto const Layout = GlobalFuture.get();

    DotStream << Layout.getDotString();
    
    AllNodeInfo.emplace_back(NodeType::Global,
                             Layout.getID(),
                             Layout.getArea(),
                             Layout.getPorts());
  }
  
  for (auto &ThreadFuture : ThreadLayouts) {
    if (CancelIfFalse == false)
      return LayoutOfProcess{std::string{}, std::chrono::nanoseconds{0}};

    auto const Layout = ThreadFuture.get();
    
    DotStream << Layout.getDotString();
    
    AllNodeInfo.insert(AllNodeInfo.end(),
                       Layout.getNodes().begin(),
                       Layout.getNodes().end());
  }
  
  for (auto &AreaFuture : AreaLayouts) {
    if (CancelIfFalse == false)
      return LayoutOfProcess{std::string{}, std::chrono::nanoseconds{0}};

    auto const Result = AreaFuture.get();

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
  addLayoutEngine(seec::makeUnique<LEVElideEmptyUnreferencedStrings>(*this));
  addLayoutEngine(seec::makeUnique<LEVElideUninitOrZeroElements>(*this));
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

std::vector<LayoutEngineForArea const *>
LayoutHandler::
listLayoutEnginesSupporting(MemoryArea const &Area,
                            ValueOfPointer const &Reference) const
{
  std::vector<LayoutEngineForArea const *> List;
  
  for (auto const &EnginePtr : AreaEngines)
    if (EnginePtr->canLayout(Area, Reference))
      List.emplace_back(EnginePtr.get());
  
  return List;
}

bool
LayoutHandler::setLayoutEngine(MemoryArea const &ForArea,
                               ValueOfPointer const &ForReference,
                               uintptr_t EngineID)
{
  // Attempt to find the engine.
  auto const EngineIt =
    std::find_if(AreaEngines.begin(), AreaEngines.end(),
                [=] (std::unique_ptr<LayoutEngineForArea> const &Engine) {
                  return reinterpret_cast<uintptr_t>(Engine.get()) == EngineID;
                });
  
  if (EngineIt == AreaEngines.end())
    return false;
  
  auto const Ptr = EngineIt->get();
  
  AreaEngineOverride[std::make_pair(ForArea.start(),
                                    ForReference.getCanonicalType())] = Ptr;

  return true;
}

bool LayoutHandler::setAreaReference(ValueOfPointer const &Reference)
{
  AreaReferenceOverride[Reference.getRawValue()] = Reference.getCanonicalType();
  return true;
}

clang::Type const *
LayoutHandler::getAreaReferenceType(seec::MemoryArea const &ForArea) const
{
  auto const OverrideIt = AreaReferenceOverride.find(ForArea.start());
  return OverrideIt != AreaReferenceOverride.end() ? OverrideIt->second
                                                   : nullptr;
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

void LayoutHandler::writeHREF(llvm::raw_ostream &Out,
                              MemoryArea const &ForArea,
                              ValueOfPointer const &ForReference) const
{
  Out << " HREF=\"area "
      << ForArea.start() << "," << ForArea.end() << ","
      << reinterpret_cast<uintptr_t>(&ForReference)
      << "\"";
}

void LayoutHandler::writeStandardProperties(llvm::raw_ostream &Out,
                                            Value const &ForValue) const
{
  writeHREF(Out, ForValue);
  
  // For some values, show initialization as a style.
  switch (ForValue.getKind()) {
    case seec::cm::Value::Kind::Basic: SEEC_FALLTHROUGH;
    case seec::cm::Value::Kind::Scalar: SEEC_FALLTHROUGH;
    case seec::cm::Value::Kind::Pointer:
      if (!ForValue.isCompletelyInitialized())
        Out << " BGCOLOR=\"#AAAAAA\"";
      break;
    
    case seec::cm::Value::Kind::Array: break;
    case seec::cm::Value::Kind::Record: break;
  }
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
  // If there's a user-selected engine, try to use that.
  auto const It =
    AreaEngineOverride.find(std::make_pair(Area.start(),
                                           Reference.getCanonicalType()));
  
  if (It != AreaEngineOverride.end() && It->second->canLayout(Area, Reference))
    return It->second->doLayout(Area, Reference, Exp);
  
  // Otherwise try to use any engine that will work.
  for (auto const &EnginePtr : AreaEngines)
    if (EnginePtr->canLayout(Area, Reference))
      return EnginePtr->doLayout(Area, Reference, Exp);
  
  return seec::Maybe<LayoutOfArea>();
}

LayoutOfProcess
LayoutHandler::doLayout(seec::cm::ProcessState const &State,
                        std::atomic_bool &CancelIfFalse) const
{
  return seec::cm::graph::doLayout(*this,
                                   State,
                                   Expansion::from(State),
                                   CancelIfFalse);
}

LayoutOfProcess
LayoutHandler::doLayout(seec::cm::ProcessState const &State) const
{
  std::atomic_bool CancelIfFalse (true);
  return doLayout(State, CancelIfFalse);
}


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec
