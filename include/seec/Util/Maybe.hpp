//===- seec/Util/Maybe.hpp ------------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This file implements a generic "discriminated union" template, called Maybe.
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_MAYBE_HPP_
#define _SEEC_UTIL_MAYBE_HPP_

#include <cassert>
#include <cstdint>
#include <utility>
#include <type_traits>

namespace seec {

namespace util {

namespace maybe_impl {

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
  void doSwitch(uint8_t Index) {}
};

// recursive case
template<typename Head, typename... Tail>
class MaybeStore<Head, Tail...> {
public:
  typedef MaybeStore<Tail...> tail_type;

private:
  union {
    Head Item;
    tail_type TailItems;
  };

public:
  /// Constructor (no-op).
  MaybeStore()
  : TailItems()
  {}

  /// Destructor (no-op).
  ~MaybeStore()
  {}

  /// Get a reference to the Head object of this union.
  Head & head() { return Item; }

  /// Get a const reference to the Head object of this union.
  Head const & head() const { return Item; }

  /// Get a reference to the tail MaybeStore of this union.
  tail_type & tail() { return TailItems; }

  /// Get a const reference to the tail MaybeStore of this union.
  tail_type const & tail() const { return TailItems; }

  /// Assign a newly constructed Head object, copying from Value.
  uint8_t assign(Head const &Value) {
    // Construct a new Head object, copying from Value
    new(&Item) Head(Value);
    return 0;
  }

  /// Assign a newly constructed Head object, copying from Value.
  uint8_t assign(Head &Value) {
    // Construct a new Head object, copying from Value
    new(&Item) Head(Value);
    return 0;
  }

  /// Assign a newly constructed Head object, moving from Value.
  uint8_t assign(Head &&Value) {
    // Construct a new Head object, moving from Value
    new(&Item) Head(std::move(Value));
    return 0;
  }

  /// Assign a newly constructed T object, using Value.
  template<typename T>
  uint8_t assign(T &&Value) {
    return 1 + TailItems.assign(std::forward<T>(Value));
  }

  void construct(uint8_t Index, MaybeStore<Head, Tail...> const & Other) {
    if (Index == 0)
      new(&Item) Head(Other.Item);
    else
      TailItems.construct(Index - 1, Other.TailItems);
  }

  void construct(uint8_t Index, MaybeStore<Head, Tail...> & Other) {
    if (Index == 0)
      new(&Item) Head(Other.Item);
    else
      TailItems.construct(Index - 1, Other.TailItems);
  }

  void construct(uint8_t Index, MaybeStore<Head, Tail...> && Other) {
    if (Index == 0)
      new(&Item) Head(std::move(Other.Item));
    else
      TailItems.construct(Index - 1, std::move(Other.TailItems));
  }

  /// Set the object at Index to the value from Other, using copy assignment.
  void copy(uint8_t Index, MaybeStore<Head, Tail...> const &Other) {
    if (Index == 0)
      Item = Other.Item;
    else
      TailItems.copy(Index - 1, Other.TailItems);
  }

  /// Set the object at Index to the value from Other, using move assignment.
  void move(uint8_t Index, MaybeStore<Head, Tail...> &&Other) {
    if (Index == 0)
      Item = std::move(Other.Item);
    else
      TailItems.move(Index - 1, std::move(Other.TailItems));
  }

  /// Run the destructor for the object at Index.
  void destroy(uint8_t Index) {
    if (Index == 0)
      Item.~Head(); // explicitly destroy item
    else
      TailItems.destroy(Index - 1);
  }

  /// Apply the Index-th predicate to the Index-th object.
  template<typename PredTy, typename... PredTys>
  void doSwitch(uint8_t Index, PredTy Pred, PredTys... Preds) {
    if (Index == 0)
      Pred(Item);
    else
      TailItems.doSwitch(Index - 1, Preds...);
  }
};

template<uint8_t I, typename T>
struct MaybeValue {
  typedef void type;
  static type get(T &maybe) {}
  static type get(T const &maybe) {}
};

template<uint8_t I, typename Head, typename... Tail>
struct MaybeValue<I, MaybeStore<Head, Tail...>> {
  typedef MaybeStore<Head, Tail...> store_type;
  typedef MaybeStore<Tail...> tail_store_type;
  typedef MaybeValue<I - 1, tail_store_type> tail_value;

  typedef typename tail_value::type type;

  static type &get(store_type &maybe) {
    return tail_value::get(maybe.tail());
  }

  static type const &get(store_type const &maybe) {
    return tail_value::get(maybe.tail());
  }

  template<typename... Args>
  static void construct(store_type &maybe, Args&&... args) {
    tail_value::construct(maybe.tail(), std::forward<Args>(args)...);
  }
};

template<typename Head, typename... Tail>
struct MaybeValue<0, MaybeStore<Head, Tail...>> {
  typedef MaybeStore<Head, Tail...> store_type;

  typedef Head type;

  static type &get(store_type &maybe) {
    return maybe.head();
  }

  static type const &get(store_type const &maybe) {
    return maybe.head();
  }

  template<typename... Args>
  static void construct(store_type &maybe, Args&&... args) {
    new (&maybe.head()) Head(std::forward<Args>(args)...);
  }
};

template<typename...>
struct MaybeIndexByType; // undefined

template<typename Element, typename... Tail>
struct MaybeIndexByType<Element, MaybeStore<Element, Tail...>> {
  static constexpr uint8_t Index = 0;
};

template<typename Element, typename Head, typename... Tail>
struct MaybeIndexByType<Element, MaybeStore<Head, Tail...>> {
  typedef MaybeStore<Tail...> tail_store_type;
  typedef MaybeIndexByType<Element, tail_store_type> tail_index;

  static constexpr uint8_t Index = 1 + tail_index::Index;
};

} // namespace maybe_impl

template<uint8_t I>
struct MaybeIndex {};

template<typename... Elems>
class Maybe {
private:
  typedef maybe_impl::MaybeStore<Elems...> store_type;

  /// Determines which element in Store is currently active. A zero value
  /// indicates that no element is active. 1 is the first element, etc.
  uint8_t Which;

  /// Implements a union capable of storing any of the types in Elems.
  store_type Store;

public:
  /// Construct a new Maybe with no active element.
  Maybe()
  : Which(0),
    Store()
  {}

  /// Construct a new Maybe which copies the active element from Other.
  Maybe(Maybe<Elems...> const & Other)
  : Which(Other.Which),
    Store()
  {
    if (Which != 0)
      Store.construct(Which - 1, Other.Store);
  }

  /// Construct a new Maybe which copies the active element from Other.
  Maybe(Maybe<Elems...> & Other)
  : Which(Other.Which),
    Store()
  {
    if (Which != 0)
      Store.construct(Which - 1, Other.Store);
  }

  /// Construct a new Maybe which moves the active element from Other.
  Maybe(Maybe<Elems...> && Other)
  : Which(Other.Which),
    Store()
  {
    if (Which != 0)
      Store.construct(Which - 1, std::move(Other.Store));
  }

  /// Construct a new Maybe which initializes the active element of type T by
  /// moving Value.
  template<typename T>
  Maybe(T &&Value)
  : Which(0),
    Store()
  {
    assign(std::forward<T>(Value));
  }

  /// Construct a new Maybe which initializes the I-th element using the
  /// supplied constructor arguments.
  template<uint8_t I, typename... Args>
  Maybe(MaybeIndex<I> Index, Args&&... args)
  : Which(I + 1),
    Store()
  {
    typedef maybe_impl::MaybeValue<I, store_type> maybe_value_type;
    maybe_value_type::construct(Store, std::forward<Args>(args)...);
  }

  /// Construct a new Maybe with the I-th element intialized using the
  /// supplied constructor arguments.
  template<uint8_t I, typename... Args>
  static Maybe construct(Args &&...args) {
    typedef maybe_impl::MaybeValue<I, store_type> maybe_value_type;

    Maybe Object;

    Object.Which = I + 1;
    maybe_value_type::construct(Object.Store, std::forward<Args>(args)...);

    return std::move(Object);
  }

  /// Destruct this Maybe, which will cause the destructor of the active element
  /// to be run (if there is an active element).
  ~Maybe() {
    if (Which != 0)
      Store.destroy(Which - 1);
  }

  /// Determine whether a value is assigned to this Maybe.
  /// \return true iff an element is active.
  bool assigned() const { return Which != 0; }

  /// Determine if the first element with type T is current assigned.
  template<typename T>
  bool assigned() const {
    if (Which == 0)
      return false;

    // Find the first index of an element with type T (statically)
    typedef typename std::remove_reference<T>::type RawT;
    typedef maybe_impl::MaybeIndexByType<RawT, store_type> maybe_index_type;
    auto constexpr Index = maybe_index_type::Index;

    return (Which - 1 == Index);
  }

  /// Determine if the element at index I is currently assigned.
  bool assigned(uint8_t I) const {
    if (Which == 0)
      return false;
    return Which - 1 == I;
  }

  /// Get the currently active element's index, starting from 1. If no element
  /// is assigned, returns 0.
  uint8_t which() const { return Which; }

  /// Clear any current assignment.
  void reset() {
    if (Which != 0)
      Store.destroy(Which - 1);
    Which = 0;
  }

  /// Clear any current assignment and assign a new T object created by moving
  /// Value.
  template<typename T>
  void assign(T &&Value) {
    reset();
    Which = 1 + Store.assign(std::forward<T>(Value));
  }

  /// Clear any current assignment and construct a new object for the I-th
  /// element.
  template<uint8_t I, typename... Args>
  void assign(Args&&... args) {
    reset();
    Which = I + 1;

    typedef maybe_impl::MaybeValue<I, store_type> maybe_value_type;
    maybe_value_type::construct(Store, std::forward<Args>(args)...);
  }

  /// Copy another Maybe of the same type.
  Maybe<Elems...> & operator= (Maybe<Elems...> const & RHS) {
    if (Which == RHS.Which) {
      if (Which == 0)
        return *this;

      // directly copy element from RHS
      Store.copy(Which - 1, RHS.Store);

      return *this;
    }

    // clear current element and construct new element from RHS
    reset();
    Which = RHS.Which;
    Store.construct(Which - 1, RHS.Store);

    return *this;
  }

  /// Copy another Maybe of the same type.
  // explicitly provide non-const method, otherwise non-const references will
  // use the perfect forwarding template operator=.
  Maybe<Elems...> & operator= (Maybe<Elems...> & RHS) {
    if (Which == RHS.Which) {
      if (Which == 0)
        return *this;

      // directly copy element from RHS
      Store.copy(Which - 1, RHS.Store);

      return *this;
    }

    // clear current element and construct new element from RHS
    reset();
    Which = RHS.Which;
    Store.construct(Which - 1, RHS.Store);

    return *this;
  }

  /// Move from another Maybe of the same type.
  Maybe<Elems...> & operator= (Maybe<Elems...> && RHS) {
    if (Which == RHS.Which) {
      if (Which == 0)
        return *this;

      // directly move element from RHS
      Store.move(Which - 1, std::move(RHS.Store));

      return *this;
    }

    // clear current element and construct new element from RHS
    reset();
    Which = RHS.Which;
    Store.construct(Which - 1, std::move(RHS.Store));

    return *this;
  }

  /// Assign this Maybe's first element of type T to Value.
  template<typename T>
  Maybe<Elems...> & operator= (T &&Value) {
    // If our active element is already of type T, then use its operator=.
    if (Which != 0) {
      // Find the first index of an element with type T (statically)
      typedef typename std::remove_reference<T>::type RawT;
      typedef maybe_impl::MaybeIndexByType<RawT, store_type> maybe_index_type;
      auto constexpr Index = maybe_index_type::Index;

      if (Which - 1 == Index) {
        typedef maybe_impl::MaybeValue<Index, store_type> maybe_value_type;
        maybe_value_type::get(Store) = std::forward<T>(Value);

        return *this;
      }
    }

    // Otherwise, destroy it and construct a new object.
    reset();
    assign(std::forward<T>(Value));

    return *this;
  }

  /// Get a reference to the I-th element of this Maybe.
  template<uint8_t I>
  typename maybe_impl::MaybeValue<I, store_type>::type &
  get() {
    if (!Which)
      Which = I + 1;
    else
      assert(Which == I + 1 && "Illegal access to Maybe.");

    typedef typename maybe_impl::MaybeValue<I, store_type> ValueType;
    return ValueType::get(Store);
  }

  /// Get a const reference to the I-th element of this Maybe. The I-th
  /// element must be the currently active element.
  template<uint8_t I>
  typename maybe_impl::MaybeValue<I, store_type>::type const &
  get() const {
    assert(Which == I + 1 && "Illegal access to Maybe.");

    typedef typename maybe_impl::MaybeValue<I, store_type> ValueType;
    return ValueType::get(Store);
  }

  /// Get a reference to the first element with type T.
  template<typename T>
  T &get() {
    // Find the first index of an element with type T (statically)
    typedef maybe_impl::MaybeIndexByType<T, store_type> maybe_index_type;
    auto constexpr Index = maybe_index_type::Index;

    if (!Which)
      Which = Index + 1;
    else
      assert((Which == Index + 1) && "Illegal access to Maybe.");

    typedef maybe_impl::MaybeValue<Index, store_type> maybe_value_type;
    return maybe_value_type::get(Store);
  }

  /// Get a const reference to the first element with type T.
  template<typename T>
  T const &get() const {
    // Find the first index of an element with type T (statically)
    typedef maybe_impl::MaybeIndexByType<T, store_type> maybe_index_type;
    auto constexpr Index = maybe_index_type::Index;

    if (!Which)
      Which = Index + 1;
    else
      assert((Which == Index + 1) && "Illegal access to Maybe.");

    typedef maybe_impl::MaybeValue<Index, store_type> maybe_value_type;
    return maybe_value_type::get(Store);
  }

  /// Apply the appropriate predicate to the currently active element, or if
  /// there is no active element, apply the UnassignedPred. The predicates
  /// should be supplied in order of element, so the 0th element would be used
  /// with the first predicate in Preds, etc.
  template<typename UnassignedPredTy, typename... PredTys>
  void doSwitch(UnassignedPredTy UnassignedPred, PredTys... Preds) {
    if (Which == 0)
      UnassignedPred();
    else
      Store.doSwitch(Which - 1, Preds...);
  }
};

} // namespace util (in seec)

} // namespace seec

#endif // _SEEC_UTIL_MAYBE_HPP_
