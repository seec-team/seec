#ifndef SEEC_RUNTIMEERRORS_ARGUMENTTYPES_HPP
#define SEEC_RUNTIMEERRORS_ARGUMENTTYPES_HPP

#include "seec/RuntimeErrors/FormatSelects.hpp"

#include "llvm/Support/Casting.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <memory>

namespace seec {

namespace runtime_errors {

/// Enumeration of all basic runtime error argument types.
enum class ArgType : uint8_t {
  Address = 1,
  Object,
  Select,
  Size
};

/// \brief Base class for all runtime error arguments.
///
/// This class can not be constructed directly, but holds functionality common
/// to all arguments.
class Arg {
  ArgType Type;

  Arg(Arg const &Other) = delete;
  Arg &operator=(Arg const &RHS) = delete;

protected:
  Arg(ArgType Type)
  : Type(Type)
  {}

public:
  virtual ~Arg() = 0;

  /// Get the type of this argument.
  ArgType type() const { return Type; }
  
  /// Serialize data.
  virtual uint64_t data() const = 0;

  /// Support LLVM's dynamic casting.
  static bool classof(Arg const *A) { return true; }
};

/// \brief An argument that holds a runtime address.
///
class ArgAddress : public Arg {
  uint64_t Address;

public:
  ArgAddress(uint64_t Address)
  : Arg(ArgType::Address),
    Address(Address)
  {}

  ArgAddress(ArgAddress const &Other)
  : Arg(ArgType::Address),
    Address(Other.Address)
  {}

  virtual ~ArgAddress();
  
  virtual uint64_t data() const { return Address; }

  /// Support LLVM's dynamic casting.
  /// \return true.
  static bool classof(ArgAddress const *A) { return true; }

  /// Support LLVM's dynamic casting.
  /// \return true iff *A is an ArgAddress.
  static bool classof(Arg const *A) { return A->type() == ArgType::Address; }

  uint64_t address() const { return Address; }
};

/// \brief An argument that represents a runtime object.
///
class ArgObject : public Arg {

public:
  ArgObject()
  : Arg(ArgType::Object)
  {}

  ArgObject(ArgObject const &Other)
  : Arg(ArgType::Object)
  {}

  virtual ~ArgObject();
  
  virtual uint64_t data() const { return 0; }

  /// Support LLVM's dynamic casting.
  /// \return true.
  static bool classof(ArgObject const *A) { return true; }

  /// Support LLVM's dynamic casting.
  /// \return true iff *A is an ArgObject.
  static bool classof(Arg const *A) { return A->type() == ArgType::Object; }
};

/// \brief Base class for all ArgSelect objects.
///
class ArgSelectBase : public Arg {
public:
  ArgSelectBase()
  : Arg(ArgType::Select)
  {}

  virtual ~ArgSelectBase();
  
  virtual uint64_t data() const {
    uint64_t Data = static_cast<uint32_t>(getSelectID());
    Data <<= 32;
    Data |= static_cast<uint32_t>(getRawItemValue());
    return Data;
  }

  /// Support LLVM's dynamic casting.
  /// \return true.
  static bool classof(ArgSelectBase const *A) { return true; }

  /// Support LLVM's dynamic casting.
  /// \return true iff A->type() is ArgType::Select.
  static bool classof(Arg const *A) { return A->type() == ArgType::Select; }

  /// Get the address of the format_selects::getCString() overload for the
  /// select type of this ArgSelect object. Used by ArgSelect to support LLVM's
  /// dynamic casting.
  virtual uintptr_t getCStringAddress() const = 0;

  virtual format_selects::SelectID getSelectID() const = 0;

  virtual uint32_t getRawItemValue() const = 0;
};

/// \brief An argument that represents a selection.
///
template<typename SelectType>
class ArgSelect : public ArgSelectBase {
  SelectType Item;

protected:
  static uintptr_t getCStringAddressImpl() {
    char const *(*func)(SelectType) = format_selects::getCString;
    return (uintptr_t) func;
  }

  virtual uintptr_t getCStringAddress() const {
    return getCStringAddressImpl();
  }

public:
  ArgSelect(SelectType Item)
  : ArgSelectBase(),
    Item(Item)
  {}

  ArgSelect(ArgSelect<SelectType> const &Other)
  : ArgSelectBase(),
    Item(Other.Item)
  {}

  virtual ~ArgSelect() {}

  static bool classof(ArgSelect<SelectType> const *A) { return true; }

  static bool classof(Arg const *A) {
    if (A->type() != ArgType::Select)
      return false;

    ArgSelectBase const *Base = llvm::cast<ArgSelectBase>(A);
    return Base->getCStringAddress() == getCStringAddressImpl();
  }

  virtual format_selects::SelectID getSelectID() const {
    return format_selects::GetSelectID<SelectType>::value();
  }

  virtual uint32_t getRawItemValue() const {
    return static_cast<uint32_t>(Item);
  }
};

///
std::unique_ptr<Arg> createArgSelect(format_selects::SelectID Select,
                                     uint32_t Item);

/// \brief An argument that holds a size.
///
class ArgSize : public Arg {
  uint64_t Size;

public:
  ArgSize(uint64_t Size)
  : Arg(ArgType::Size),
    Size(Size)
  {}

  ArgSize(ArgSize const &Other)
  : Arg(ArgType::Size),
    Size(Other.Size)
  {}

  virtual ~ArgSize();
  
  virtual uint64_t data() const { return Size; }

  /// Support LLVM's dynamic casting.
  /// \return true.
  static bool classof(ArgSize const *A) { return true; }

  /// Support LLVM's dynamic casting.
  /// \return true iff *A is an ArgAddress.
  static bool classof(Arg const *A) { return A->type() == ArgType::Size; }

  uint64_t size() const { return Size; }
};

} // namespace runtime_errors (in seec)

} // namespace seec

#endif // SEEC_RUNTIMEERRORS_ARGUMENTTYPES_HPP
