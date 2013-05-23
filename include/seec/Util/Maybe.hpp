//===- seec/Util/Maybe.hpp ------------------------------------------ C++ -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// A generic "discriminated union" template, called Maybe.
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_MAYBE_HPP
#define SEEC_UTIL_MAYBE_HPP

#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>
#include <type_traits>

namespace seec {

/// Contains implementation details for seec::Maybe.
namespace maybe_impl {

//===------------------------------------------------------------------------===
// typeInList
//===------------------------------------------------------------------------===

template<typename T>
constexpr bool typeInList() { return false; }

template<typename T, typename Head, typename... Tail>
constexpr bool typeInList() {
  return std::is_same<T, Head>::value ? true : typeInList<T, Tail...>();
}


//===------------------------------------------------------------------------===
// MaybeStore
//===------------------------------------------------------------------------===

template<typename...>
class MaybeStore; // undefined

// base case
template<>
class MaybeStore<> {
public:
  void construct(uint8_t Index, MaybeStore<> const & Other) {}
  void construct(uint8_t Index, MaybeStore<> & Other) {}
  void construct(uint8_t Index, MaybeStore<> && Other) {}
  void copy(uint8_t Index, MaybeStore<> const & Other) {}
  void move(uint8_t Index, MaybeStore<> && Other) {}
  void destroy(uint8_t Index) {}
};

// recursive case
template<typename Head, typename... Tail>
class MaybeStore<Head, Tail...> {
public:
  typedef MaybeStore<Tail...> TailT;

private:
  union {
    Head Item;
    TailT TailItems;
  };

public:
  /// \brief Constructor (no-op).
  MaybeStore() : TailItems() {}

  /// \brief Destructor (no-op).
  ~MaybeStore() {}

  /// \brief Get a reference to the Head object of this union.
  Head &head() { return Item; }

  /// \brief Get a const reference to the Head object of this union.
  Head const &head() const { return Item; }

  /// \brief Get a reference to the tail MaybeStore of this union.
  TailT &tail() { return TailItems; }

  /// \brief Get a const reference to the tail MaybeStore of this union.
  TailT const &tail() const { return TailItems; }

  /// \brief Assign a newly constructed Head object, copying from Value.
  uint8_t assign(Head const &Value) {
    // Construct a new Head object, copying from Value
    new(&Item) Head(Value);
    return 0;
  }

  /// \brief Assign a newly constructed Head object, copying from Value.
  uint8_t assign(Head &Value) {
    // Construct a new Head object, copying from Value
    new(&Item) Head(Value);
    return 0;
  }

  /// \brief Assign a newly constructed Head object, moving from Value.
  uint8_t assign(Head &&Value) {
    // Construct a new Head object, moving from Value
    new(&Item) Head(std::move(Value));
    return 0;
  }

  /// \brief Assign a newly constructed T object, using Value.
  template<typename T>
  uint8_t assign(T &&Value) {
    return 1 + TailItems.assign(std::forward<T>(Value));
  }

  /// \brief Copy-construct a new object at index from the object in Other.
  void construct(uint8_t Index, MaybeStore<Head, Tail...> const & Other) {
    if (Index == 0)
      new(&Item) Head(Other.Item);
    else
      TailItems.construct(Index - 1, Other.TailItems);
  }

  /// \brief Copy-construct a new object at index from the object in Other.
  void construct(uint8_t Index, MaybeStore<Head, Tail...> & Other) {
    if (Index == 0)
      new(&Item) Head(Other.Item);
    else
      TailItems.construct(Index - 1, Other.TailItems);
  }

  /// \brief Move-construct a new object at index from the object in Other.
  void construct(uint8_t Index, MaybeStore<Head, Tail...> && Other) {
    if (Index == 0)
      new(&Item) Head(std::move(Other.Item));
    else
      TailItems.construct(Index - 1, std::move(Other.TailItems));
  }

  /// \brief Set the object at Index to the value from Other, using copy
  ///        assignment.
  void copy(uint8_t Index, MaybeStore<Head, Tail...> const &Other) {
    if (Index == 0)
      Item = Other.Item;
    else
      TailItems.copy(Index - 1, Other.TailItems);
  }

  /// \brief Set the object at Index to the value from Other, using move
  ///        assignment.
  void move(uint8_t Index, MaybeStore<Head, Tail...> &&Other) {
    if (Index == 0)
      Item = std::move(Other.Item);
    else
      TailItems.move(Index - 1, std::move(Other.TailItems));
  }

  /// \brief Run the destructor for the object at Index.
  void destroy(uint8_t Index) {
    if (Index == 0)
      Item.~Head(); // explicitly destroy item
    else
      TailItems.destroy(Index - 1);
  }
};

// Base case of MaybeValue (should never be used).
template<uint8_t I, typename T>
struct MaybeValue {
  typedef void type;
  static type get(T &maybe) {}
  static type get(T const &maybe) {}
};

template<uint8_t I, typename Head, typename... Tail>
struct MaybeValue<I, MaybeStore<Head, Tail...>> {
  // Ensure that I is allowable.
  static_assert(I < sizeof...(Tail) + 1, "Value of I is too large.");

  typedef MaybeStore<Head, Tail...> StoreT;
  typedef MaybeStore<Tail...> TailStoreT;
  typedef MaybeValue<I - 1, TailStoreT> TailValueT;

  typedef typename TailValueT::type type;

  static type &get(StoreT &maybe) {
    return TailValueT::get(maybe.tail());
  }

  static type const &get(StoreT const &maybe) {
    return TailValueT::get(maybe.tail());
  }

  template<typename... Args>
  static void construct(StoreT &maybe, Args&&... args) {
    TailValueT::construct(maybe.tail(), std::forward<Args>(args)...);
  }
};

template<typename Head, typename... Tail>
struct MaybeValue<0, MaybeStore<Head, Tail...>> {
  typedef MaybeStore<Head, Tail...> StoreT;

  typedef Head type;

  static type &get(StoreT &maybe) {
    return maybe.head();
  }

  static type const &get(StoreT const &maybe) {
    return maybe.head();
  }

  template<typename... Args>
  static void construct(StoreT &maybe, Args&&... args) {
    new (&maybe.head()) Head(std::forward<Args>(args)...);
  }
};

// Base case of MaybeIndexByType: undefined.
template<typename...>
struct MaybeIndexByType;

// Specialization of MaybeIndexByType that matches when the type is the same as
// the head type of the MaybeStore.
template<typename Element, typename... Tail>
struct MaybeIndexByType<Element, MaybeStore<Element, Tail...>> {
  static constexpr uint8_t Index = 0;
};

// General case of MaybeIndexByType, which finds the index of the type in the
// tail of the MaybeStore, and adds 1 to that index (to account for the head of
// the MaybeStore).
template<typename Element, typename Head, typename... Tail>
struct MaybeIndexByType<Element, MaybeStore<Head, Tail...>> {
  static_assert(typeInList<Element, Tail...>(),
                "Type was not found in element types.");

  typedef MaybeStore<Tail...> TailStoreT;
  typedef MaybeIndexByType<Element, TailStoreT> TailIndexT;

  static constexpr uint8_t Index = 1 + TailIndexT::Index;
};

} // namespace maybe_impl

/// \brief A generic "discriminated union" template.
///
/// This template class implements a discriminated union, which is able to hold
/// a single object of any of the types supplied in Elems. The active element
/// slot is recorded, so that:
///  \li If a different slot becomes active, the previous object is destructed.
///  \li If a slot is accessed (e.g. using get()), an assertion checks that the
///      slot is currently active.
///
/// \tparam Elems... the types that this union should be able to store. The
///         number of types must be less than 255, but the same type can be used
///         more than once. If the same type is used more than once, any method
///         that accesses an element by type will access the first element with
///         that type.
///
template<typename... Elems>
class Maybe {
private:
  /// Determines which element in Store is currently active. A zero value
  /// indicates that no element is active. 1 is the first element, etc.
  uint8_t Which;

  // Ensure that the number of elements supplied can be represented by Which.
  // Hardcoded until our libc++ is updated to support constexpr max().
  static_assert(sizeof...(Elems) < 255,
                "Too many elements for Maybe.");

  // Define the type of the store for our given elements.
  typedef maybe_impl::MaybeStore<Elems...> StoreT;

  /// Implements a union capable of storing any of the types in Elems.
  StoreT Store;

public:
  /// \brief Construct with no active element.
  Maybe()
  : Which(0),
    Store()
  {}

  /// \brief Construct by copying the active element from Other.
  Maybe(Maybe<Elems...> const & Other)
  : Which(Other.Which),
    Store()
  {
    if (Which != 0)
      Store.construct(Which - 1, Other.Store);
  }

  /// \brief Construct by copying the active element from Other.
  Maybe(Maybe<Elems...> & Other)
  : Which(Other.Which),
    Store()
  {
    if (Which != 0)
      Store.construct(Which - 1, Other.Store);
  }

  /// \brief Construct by moving the active element from Other.
  Maybe(Maybe<Elems...> && Other)
  : Which(Other.Which),
    Store()
  {
    if (Which != 0)
      Store.construct(Which - 1, std::move(Other.Store));
  }

  /// \brief Construct by moving from Value.
  /// Construct a new Maybe which initializes the first element of type T to be
  /// active by moving from Value.
  template<typename T>
  Maybe(T &&Value)
  : Which(0),
    Store()
  {
    assign(std::forward<T>(Value));
  }

  /// \brief Construct the I-th element using the given arguments.
  ///
  /// Construct a new Maybe with the I-th element intialized using the
  /// supplied constructor arguments.
  template<uint8_t I, typename... ArgTs>
  static Maybe construct(ArgTs &&...Args) {
    // Ensure that I is allowable.
    static_assert(I < sizeof...(Elems), "Value of I is too large.");

    // Get the accessor for index I in our store type.
    typedef maybe_impl::MaybeValue<I, StoreT> ValueT;

    // Construct and return the new Maybe.
    Maybe Object;
    Object.Which = I + 1;
    ValueT::construct(Object.Store, std::forward<ArgTs>(Args)...);
    return Object;
  }

  /// \brief Destruct the currently active element (and this object).
  ///
  /// Destruct this Maybe, which will cause the destructor of the active element
  /// to be run (if there is an active element).
  ~Maybe() {
    if (Which != 0)
      Store.destroy(Which - 1);
  }

  /// \brief Determine whether a value is assigned to this Maybe.
  /// \return true iff an element is active.
  bool assigned() const { return Which != 0; }

  /// \brief Determine if the first element with type T is currently assigned.
  ///
  /// If T is a reference type, the reference is removed. We then find the
  /// first element which matches this type, and check if it is the currently
  /// active element.
  ///
  /// \return true iff the first element matching type T is currently assigned.
  template<typename T>
  bool assigned() const {
    // Remove reference from type T.
    typedef typename std::remove_reference<T>::type RawT;

    // Ensure that this type exists in our element types.
    static_assert(seec::maybe_impl::typeInList<RawT, Elems...>(),
                  "Type was not found in element types.");

    if (Which == 0)
      return false;

    // Find the first index of an element with type T.
    typedef maybe_impl::MaybeIndexByType<RawT, StoreT> IndexT;
    auto constexpr Index = IndexT::Index;

    // Return true if this index is the currently active index.
    return (Which - 1 == Index);
  }

  /// \brief Determine if the element at index I is currently assigned.
  ///
  bool assigned(uint8_t I) const {
    if (Which == 0)
      return false;

    return Which - 1 == I;
  }

  /// \brief Get the currently active element's index, starting from 1.
  ///
  /// If no element is assigned, returns 0.
  uint8_t which() const { return Which; }

  /// \brief Clear any current assignment (destructing the active element).
  void reset() {
    if (Which != 0)
      Store.destroy(Which - 1);

    Which = 0;
  }

  /// \brief Clear any current assignment and assign a new T object created by
  /// moving Value.
  template<typename T>
  void assign(T &&Value) {
    reset();

    Which = 1 + Store.assign(std::forward<T>(Value));
  }

  /// \brief Clear any current assignment and construct a new object for the
  /// I-th element.
  template<uint8_t I, typename... ArgTs>
  void assign(ArgTs&&... Args) {
    // Ensure that I is allowable.
    static_assert(I < sizeof...(Elems), "Value of I is too large.");

    // Clear any currently active element.
    reset();

    // Set this element to be active.
    Which = I + 1;

    // Get an accessor for this element.
    typedef maybe_impl::MaybeValue<I, StoreT> ValueT;

    // Construct this element using the supplied arguments.
    ValueT::construct(Store, std::forward<ArgTs>(Args)...);
  }

  /// \brief Copy the active element from another Maybe of the same type.
  Maybe<Elems...> & operator= (Maybe<Elems...> const &RHS) {
    if (Which == RHS.Which) {
      if (Which == 0) // Both Maybes are unassigned, so do nothing.
        return *this;

      // Directly copy element from RHS.
      Store.copy(Which - 1, RHS.Store);

      return *this;
    }

    // Clear any currently active element.
    reset();

    if (RHS.Which != 0) {
      // Set the active element of RHS to be active.
      Which = RHS.Which;

      // Copy-construct the active element from RHS.
      Store.construct(Which - 1, RHS.Store);
    }

    return *this;
  }

  /// \brief Copy the active element from another Maybe of the same type.
  Maybe<Elems...> & operator= (Maybe<Elems...> &RHS) {
    if (Which == RHS.Which) {
      if (Which == 0) // Both Maybes are unassigned, so do nothing.
        return *this;

      // Directly copy element from RHS.
      Store.copy(Which - 1, RHS.Store);

      return *this;
    }

    // Clear any currently active element.
    reset();

    if (RHS.Which != 0) {
      // Set the active element of RHS to be active.
      Which = RHS.Which;

      // Copy-construct the active element from RHS.
      Store.construct(Which - 1, RHS.Store);
    }

    return *this;
  }

  /// \brief Move from another Maybe of the same type.
  Maybe<Elems...> & operator= (Maybe<Elems...> &&RHS) {
    if (Which == RHS.Which) {
      if (Which == 0) // Both Maybes are unassigned, so do nothing.
        return *this;

      // Directly move element from RHS.
      Store.copy(Which - 1, std::move(RHS.Store));

      return *this;
    }

    // Clear any currently active element.
    reset();

    if (RHS.Which != 0) {
      // Set the active element of RHS to be active.
      Which = RHS.Which;

      // Move-construct the active element from RHS.
      Store.construct(Which - 1, std::move(RHS.Store));
    }

    return *this;
  }

  /// \brief Assign this Maybe's first element of type T to Value.
  template<typename T>
  Maybe<Elems...> & operator= (T &&Value) {
    // Get the type of T with any reference removed.
    typedef typename std::remove_reference<T>::type RawT;

    // Ensure that this type exists in our element types.
    static_assert(seec::maybe_impl::typeInList<RawT, Elems...>(),
                  "Type was not found in element types.");

    // First, check if the currently active element is of type T, in which case
    // we can directly use its assignment operator.
    if (Which != 0) {
      // Find the index of the first element matching type RawT.
      typedef maybe_impl::MaybeIndexByType<RawT, StoreT> IndexT;
      auto constexpr Index = IndexT::Index;

      if (Which - 1 == Index) {
        // Get the accessor for this element.
        typedef maybe_impl::MaybeValue<Index, StoreT> ValueT;

        // Assign to the element using its assignment operator.
        ValueT::get(Store) = std::forward<T>(Value);

        return *this;
      }
    }

    // Destroy any currently existing element.
    reset();

    // Construct a new element of type T from the given Value.
    assign(std::forward<T>(Value));

    return *this;
  }

  /// \brief Get a reference to the I-th element of this Maybe.
  template<uint8_t I>
  typename maybe_impl::MaybeValue<I, StoreT>::type &
  get() {
    // Ensure that I is allowable.
    static_assert(I < sizeof...(Elems), "Value of I is too large.");

    // Ensure that element I is active.
    if (!Which) {
      // TODO: We must be able to default-construct this element.
      Which = I + 1;
    }
    else
      assert(Which == I + 1 && "Illegal access to Maybe.");

    // Return a reference to element I.
    typedef typename maybe_impl::MaybeValue<I, StoreT> ValueT;
    return ValueT::get(Store);
  }

  /// \brief Get a const reference to the I-th element of this Maybe.
  template<uint8_t I>
  typename maybe_impl::MaybeValue<I, StoreT>::type const &
  get() const {
    // Ensure that I is allowable.
    static_assert(I < sizeof...(Elems), "Value of I is too large.");

    // Ensure that element I is active.
    assert(Which == I + 1 && "Illegal access to Maybe.");

    typedef typename maybe_impl::MaybeValue<I, StoreT> ValueT;
    return ValueT::get(Store);
  }
  
  /// \brief Get an rvalue reference to the I-th element of this Maybe.
  ///
  template<uint8_t I>
  typename maybe_impl::MaybeValue<I, StoreT>::type &&
  move()
  {
    return std::move(get<I>());
  }

  /// \brief Get a reference to the first element with type T.
  template<typename T>
  T &get() {
    // Find the first index of an element with type T (statically)
    typedef maybe_impl::MaybeIndexByType<T, StoreT> IndexT;
    auto constexpr Index = IndexT::Index;

    if (!Which)
      Which = Index + 1;
    else
      assert((Which == Index + 1) && "Illegal access to Maybe.");

    typedef maybe_impl::MaybeValue<Index, StoreT> ValueT;
    return ValueT::get(Store);
  }

  /// \brief Get a const reference to the first element with type T.
  template<typename T>
  T const &get() const {
    // Find the first index of an element with type T (statically)
    typedef maybe_impl::MaybeIndexByType<T, StoreT> IndexT;
    auto constexpr Index = IndexT::Index;
    
    assert((Which == Index + 1) && "Illegal access to Maybe.");
    
    typedef maybe_impl::MaybeValue<Index, StoreT> ValueT;
    return ValueT::get(Store);
  }
  
  /// \brief Get an rvalue reference to the first element with type T.
  ///
  template<typename T>
  T &&move()
  {
    return std::move(get<T>());
  }
};

} // namespace seec

#endif // SEEC_UTIL_MAYBE_HPP
