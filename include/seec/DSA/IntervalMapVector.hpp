//===- include/seec/DSA/IntervalMapVector.hpp ----------------------- C++ -===//
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

#ifndef _SEEC_DSA_INTERVAL_MAP_VECTOR_HPP_
#define _SEEC_DSA_INTERVAL_MAP_VECTOR_HPP_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <vector>

namespace seec {


/// Holds a single item in an IntervalMapVector.
template<typename Key, typename Data>
class IntervalMapItem {
public:
  Key Begin; ///< Start of this interval.
  Key End; ///< End of this interval.
  Data Value; ///< The value associated with this interval.

  IntervalMapItem() {}

  IntervalMapItem(Key const &Begin, Key const &End, Data const &Value)
  : Begin(Begin),
    End(End),
    Value(Value)
  {}
};


/// Helper function to create IntervalMapItem objects.
template<typename Key, typename Data>
IntervalMapItem<Key, Data> makeIntervalMapItem(Key const &Begin, Key const &End,
                                               Data const &Value) {
  return IntervalMapItem<Key, Data>(Begin, End, Value);
}


// forward-declaration
template<typename Key, typename Data, typename Compare = std::less<Key>,
         typename Alloc = std::allocator<IntervalMapItem<Key, Data> > >
class IntervalMapVector;


/// Iterator for IntervalMapVector.
template<typename Key, typename Data>
class IntervalMapVectorIterator
: public std::iterator<std::random_access_iterator_tag,
                       IntervalMapItem<Key, Data> >
{
  friend class IntervalMapVector<Key, Data>;

private:
  typedef IntervalMapItem<Key, Data> item_type;
  typedef typename std::vector<item_type> container_type;
  typedef typename container_type::iterator container_iterator;
  typedef typename container_type::const_iterator container_const_iterator;

  /// An iterator for the vector that underlies an IntervalMapVector.
  container_iterator It;

  /// Construct from the underlying vector iterator.
  /// \param It an iterator in the vector that underlies an IntervalMapVector.
  IntervalMapVectorIterator(container_iterator It)
  : It(It)
  {}

public:
  /// Default constructor.
  IntervalMapVectorIterator() {}

  /// Copy constructor.
  /// \param Other the iterator to copy.
  IntervalMapVectorIterator(IntervalMapVectorIterator const &Other)
  : It(Other.It)
  {}

  /// Assignment operator.
  /// \param RHS the iterator to copy.
  IntervalMapVectorIterator & operator= (IntervalMapVectorIterator const &RHS) {
    It = RHS.It;
    return *this;
  }

  /// Equality operator.
  /// \param RHS an iterator.
  /// \return true iff this and RHS point to the same element.
  bool operator== (IntervalMapVectorIterator const &RHS) const {
    return It == RHS.It;
  }

  /// Less-than operator.
  /// \param RHS an iterator.
  /// \return true iff this points to an earlier element than RHS.
  bool operator<(IntervalMapVectorIterator const &RHS) const {
    return It < RHS.It;
  }

  /// Greater-than operator.
  /// \param RHS an iterator.
  /// \return true iff this points to a later element than RHS.
  bool operator>(IntervalMapVectorIterator const &RHS) const {
    return It > RHS.It;
  }

  /// Less-than-or-equal operator.
  /// \param RHS an iterator.
  /// \return true iff this points to an earlier element than, or the same
  /// element as, RHS.
  bool operator<=(IntervalMapVectorIterator const &RHS) const {
    return It <= RHS.It;
  }

  /// Greater-than-or-equal operator.
  /// \param RHS an iterator.
  /// \return true iff this points to a later element than, or the same element
  /// as, RHS.
  bool operator>=(IntervalMapVectorIterator const &RHS) const {
    return It >= RHS.It;
  }

  /// Inequality operator.
  /// \param RHS an iterator.
  /// \return true iff this and RHS do not point to the same element.
  bool operator!= (IntervalMapVectorIterator const &RHS) const {
    return It != RHS.It;
  }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type & operator*() { return *It; }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type const & operator*() const { return *It; }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type * operator->() { return &*It; }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type const * operator->() const { return &*It; }

  /// Move this iterator to the next item in the IntervalMapVector.
  /// \return a reference to this iterator.
  IntervalMapVectorIterator & operator++() { ++It; return *this; }

  /// Move this iterator to the next item in the IntervalMapVector.
  /// \return a copy of this iterator, taken before the increment.
  IntervalMapVectorIterator operator++(int) {
    IntervalMapVectorIterator tmp(*this);
    operator++();
    return tmp;
  }

  /// Move this iterator to the previous item in the IntervalMapVector.
  /// \return a reference to this iterator.
  IntervalMapVectorIterator & operator--() { --It; return *this; }

  /// Move this iterator to the previous item in the IntervalMapVector.
  /// \return a copy of this iterator, taken before the decrement.
  IntervalMapVectorIterator operator--(int) {
    IntervalMapVectorIterator tmp(*this);
    operator--();
    return tmp;
  }

  /// Make a copy of this iterator and move the copy forward.
  /// \param n the number of positions to move forward.
  /// \return the copy of the iterator.
  IntervalMapVectorIterator operator+(int n) {
    IntervalMapVectorIterator tmp(*this);
    tmp.It += n;
    return tmp;
  }

  /// Make a copy of this iterator and move the copy backward.
  /// \param n the number of positions to move backward.
  /// \return the copy of the iterator.
  IntervalMapVectorIterator operator-(int n) {
    IntervalMapVectorIterator tmp(*this);
    tmp.It -= n;
    return tmp;
  }

  /// Move this iterator forward.
  /// \param n the number of positions to move forward.
  /// \return a reference to this iterator.
  IntervalMapVectorIterator & operator+=(int n) {
    It += n;
    return *this;
  }

  /// Move this iterator backward.
  /// \param n the number of positions to move backward.
  /// \return a reference to this iterator.
  IntervalMapVectorIterator & operator-=(int n) {
    It -= n;
    return *this;
  }

  /// Indexed dereference.
  /// \param n a position relative to this iterator.
  /// \return a reference to the IntervalMapItem at position n relative to this
  /// iterator.
  item_type & operator[](int n) {
    return It[n];
  }

  /// Indexed dereference.
  /// \param n a position relative to this iterator.
  /// \return a const reference to the IntervalMapItem at position n relative to
  /// this iterator.
  item_type const & operator[](int n) const {
    return It[n];
  }
};


/// Const Iterator for IntervalMapVector.
template<typename Key, typename Data>
class IntervalMapVectorConstIterator
: public std::iterator<std::random_access_iterator_tag,
                       IntervalMapItem<Key, Data> >
{
  friend class IntervalMapVector<Key, Data>;

private:
  typedef IntervalMapVectorIterator<Key, Data> non_const_iterator;
  typedef IntervalMapItem<Key, Data> item_type;
  typedef typename std::vector<item_type> container_type;
  typedef typename container_type::iterator container_iterator;
  typedef typename container_type::const_iterator container_const_iterator;

  /// An iterator for the vector that underlies an IntervalMapVector.
  container_const_iterator It;

  /// Construct from the underlying vector iterator.
  /// \param It an iterator in the vector that underlies an IntervalMapVector.
  IntervalMapVectorConstIterator(container_const_iterator It)
  : It(It)
  {}

public:
  /// Default constructor.
  IntervalMapVectorConstIterator() {}
  
  /// Copy constructor.
  /// \param Other the iterator to copy.
  IntervalMapVectorConstIterator(IntervalMapVectorConstIterator const &Other)
  : It(Other.It)
  {}

  /// Copy from a non-const iterator.
  IntervalMapVectorConstIterator(non_const_iterator const &Other)
  : It(Other.It)
  {}

  /// Assignment operator.
  /// \param RHS the iterator to copy.
  IntervalMapVectorConstIterator &
  operator=(IntervalMapVectorConstIterator const &RHS) {
    It = RHS.It;
    return *this;
  }
  
  /// Copy from a non-const iterator.
  IntervalMapVectorConstIterator &
  operator=(non_const_iterator const &RHS) {
    It = RHS.It;
    return *this;
  }

  /// Equality operator.
  /// \param RHS an iterator.
  /// \return true iff this and RHS point to the same element.
  bool operator== (IntervalMapVectorConstIterator const &RHS) const {
    return It == RHS.It;
  }
  
  /// Equality operator.
  /// \param RHS an iterator.
  /// \return true iff this and RHS point to the same element.
  bool operator== (non_const_iterator const &RHS) const {
    return It == RHS.It;
  }

  /// Less-than operator.
  /// \param RHS an iterator.
  /// \return true iff this points to an earlier element than RHS.
  bool operator<(IntervalMapVectorConstIterator const &RHS) const {
    return It < RHS.It;
  }

  /// Greater-than operator.
  /// \param RHS an iterator.
  /// \return true iff this points to a later element than RHS.
  bool operator>(IntervalMapVectorConstIterator const &RHS) const {
    return It > RHS.It;
  }

  /// Less-than-or-equal operator.
  /// \param RHS an iterator.
  /// \return true iff this points to an earlier element than, or the same
  /// element as, RHS.
  bool operator<=(IntervalMapVectorConstIterator const &RHS) const {
    return It <= RHS.It;
  }

  /// Greater-than-or-equal operator.
  /// \param RHS an iterator.
  /// \return true iff this points to a later element than, or the same element
  /// as, RHS.
  bool operator>=(IntervalMapVectorConstIterator const &RHS) const {
    return It >= RHS.It;
  }

  /// Inequality operator.
  /// \param RHS an iterator.
  /// \return true iff this and RHS do not point to the same element.
  bool operator!= (IntervalMapVectorConstIterator const &RHS) const {
    return It != RHS.It;
  }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type const & operator*() { return *It; }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type const & operator*() const { return *It; }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type const * operator->() { return &*It; }

  /// Dereference operator.
  /// \return a reference to the IntervalMapItem pointed to by this iterator.
  item_type const * operator->() const { return &*It; }

  /// Move this iterator to the next item in the IntervalMapVector.
  /// \return a reference to this iterator.
  IntervalMapVectorConstIterator & operator++() { ++It; return *this; }

  /// Move this iterator to the next item in the IntervalMapVector.
  /// \return a copy of this iterator, taken before the increment.
  IntervalMapVectorConstIterator operator++(int) {
    IntervalMapVectorConstIterator tmp(*this);
    operator++();
    return tmp;
  }

  /// Move this iterator to the previous item in the IntervalMapVector.
  /// \return a reference to this iterator.
  IntervalMapVectorConstIterator & operator--() { --It; return *this; }

  /// Move this iterator to the previous item in the IntervalMapVector.
  /// \return a copy of this iterator, taken before the decrement.
  IntervalMapVectorConstIterator operator--(int) {
    IntervalMapVectorConstIterator tmp(*this);
    operator--();
    return tmp;
  }

  /// Make a copy of this iterator and move the copy forward.
  /// \param n the number of positions to move forward.
  /// \return the copy of the iterator.
  IntervalMapVectorConstIterator operator+(int n) {
    IntervalMapVectorConstIterator tmp(*this);
    tmp.It += n;
    return tmp;
  }

  /// Make a copy of this iterator and move the copy backward.
  /// \param n the number of positions to move backward.
  /// \return the copy of the iterator.
  IntervalMapVectorConstIterator operator-(int n) {
    IntervalMapVectorConstIterator tmp(*this);
    tmp.It -= n;
    return tmp;
  }

  /// Move this iterator forward.
  /// \param n the number of positions to move forward.
  /// \return a reference to this iterator.
  IntervalMapVectorConstIterator & operator+=(int n) {
    It += n;
    return *this;
  }

  /// Move this iterator backward.
  /// \param n the number of positions to move backward.
  /// \return a reference to this iterator.
  IntervalMapVectorConstIterator & operator-=(int n) {
    It -= n;
    return *this;
  }

  /// Indexed dereference.
  /// \param n a position relative to this iterator.
  /// \return a reference to the IntervalMapItem at position n relative to this
  /// iterator.
  item_type const & operator[](int n) {
    return It[n];
  }

  /// Indexed dereference.
  /// \param n a position relative to this iterator.
  /// \return a const reference to the IntervalMapItem at position n relative to
  /// this iterator.
  item_type const & operator[](int n) const {
    return It[n];
  }
};


///
template<typename Key, typename Data, typename Compare, typename Alloc>
class IntervalMapVector {
public:
  typedef Key key_type;
  typedef Data data_type;
  typedef IntervalMapItem<Key, Data> value_type;

  /// Implements comparison between two value_type objects.
  class ValueCompare {
    friend class IntervalMapVector;

  private:
    Compare Comparator;

    ValueCompare(Compare Comparator)
    : Comparator(Comparator)
    {}

  public:
    bool operator() (const value_type &LHS, const value_type &RHS) const {
      return Comparator(LHS.Begin, RHS.Begin);
    }

    bool operator() (const value_type &LHS, const key_type &RHS) const {
      return Comparator(LHS.Begin, RHS);
    }

    bool operator() (const key_type &LHS, const value_type &RHS) const {
      return Comparator(LHS, RHS.Begin);
    }
  };

  typedef Compare key_compare;
  typedef ValueCompare value_compare;

  typedef value_type * pointer;
  typedef value_type & reference;
  typedef value_type const & const_reference;

  typedef uintptr_t size_type;
  typedef ptrdiff_t difference_type;

  typedef IntervalMapVectorIterator<Key, Data> iterator;
  typedef IntervalMapVectorConstIterator<Key, Data> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

private:
  typedef typename std::vector<value_type, Alloc> container_type;
  typedef typename container_type::iterator container_iterator;
  typedef typename container_type::const_iterator container_const_iterator;

  /// Object used to compare keys.
  key_compare KeyComparator;

  /// Object used to compare values.
  value_compare ValueComparator;

  /// Allocator.
  Alloc Allocator;

  /// The internal container (a vector) containing the sorted list of values.
  container_type Items;

public:
  /// Default constructor.
  IntervalMapVector()
  : KeyComparator(),
    ValueComparator(KeyComparator),
    Allocator(),
    Items(Allocator)
  {}

  /// Copy constructor.
  IntervalMapVector(IntervalMapVector const & Other)
  : KeyComparator(Other.Comparator),
    ValueComparator(KeyComparator),
    Allocator(Other.Allocator),
    Items(Other.Items, Other.Allocator)
  {}

  /// Move constructor.
  IntervalMapVector(IntervalMapVector && Other)
  : KeyComparator(Other.Comparator),
    ValueComparator(KeyComparator),
    Allocator(Other.Allocator),
    Items()
  {
    Items.swap(Other.Items);
  }

  /// Range constructor.
  template<class InputIterator>
  IntervalMapVector(InputIterator first, InputIterator last,
                    const Compare& comp = Compare(),
                    const Alloc& alloc = Alloc())
  : KeyComparator(comp),
    ValueComparator(KeyComparator),
    Allocator(alloc),
    Items(Allocator)
  {
    // insert range
  }

  /// Destructor.
  ~IntervalMapVector() {}

  /// Assignment operator.
  IntervalMapVector & operator= (IntervalMapVector const & RHS) {
    Items = RHS.Items;
    return *this;
  }

  // Container

  //
  iterator begin() { return iterator(Items.begin()); }
  //
  const_iterator begin() const { return const_iterator(Items.begin()); }
  //
  iterator end() { return iterator(Items.end()); }
  //
  const_iterator end() const { return const_iterator(Items.end()); }
  //
  reverse_iterator rbegin() { return reverse_iterator(Items.end()); }
  //
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(Items.end());
  }
  //
  reverse_iterator rend() { return reverse_iterator(Items.begin()); }
  //
  const_reverse_iterator rend() const {
    return const_reverse_iterator(Items.begin());
  }
  //
  size_type size() const { return Items.size(); }
  //
  size_type max_size() const { return Items.max_size(); }
  //
  bool empty() const { return Items.empty(); }
  //
  void swap(IntervalMapVector & Other) { Items.swap(Other.Items); }

  // Forward Container

  //
  bool operator== (IntervalMapVector & Other) { return Items == Other.Items; }
  //
  bool operator!= (IntervalMapVector & Other) { return Items != Other.Items; }
  //
  bool operator< (IntervalMapVector & Other) { return Items < Other.Items; }
  //
  bool operator> (IntervalMapVector & Other) { return Items > Other.Items; }
  //
  bool operator<= (IntervalMapVector & Other) { return Items <= Other.Items; }
  //
  bool operator>= (IntervalMapVector & Other) { return Items >= Other.Items; }

  // Associative container

  //
  size_type erase(key_type const &key) {
    iterator It = find(key);
    if (It == end())
      return 0;

    erase(It);
    return 1;
  }

  //
  void erase(iterator Iter) { Items.erase(Iter.It); }

  //
  void erase(iterator Start, iterator End) { Items.erase(Start.It, End.It); }

  //
  void clear() { Items.clear(); }

  //
  iterator find(key_type const &key) {
    container_iterator It = std::upper_bound(Items.begin(), Items.end(), key,
                                             ValueComparator);
    if (It == Items.begin())
      return end();

    --It; // Now It->Begin <= key

    if (key <= It->End)
      return iterator(It);

    return end();
  }

  //
  const_iterator find(key_type const &key) const {
    container_const_iterator It = std::upper_bound(Items.begin(),
                                                   Items.end(),
                                                   key,
                                                   ValueComparator);
    if (It == Items.begin())
      return end();

    --It; // Now It->Begin <= key

    if (key <= It->End)
      return const_iterator(It);

    return end();
  }

  //
  size_type count(key_type const &key) const {
    if (find(key) != end())
      return 1;
    return 0;
  }

  /// Count the number of intervals that overlap with a range.
  /// \param range the range to count over. range.first <= range.second.
  /// \return the number of intervals that overlap range.
  size_type count(std::pair<key_type, key_type> const &range) {
    size_type num = 0;

    container_iterator Begin = Items.begin(), End = Items.end();

    // Find the lowest interval such that It->Begin > range.first
    container_iterator It = std::upper_bound(Begin, End, range.first,
                                                ValueComparator);

    // Check the previous interval, if any, to see if it overlaps range
    if (It != Begin) {
      --It; // Now It->Begin <= range.first

      if (range.first <= It->End)
        ++num;

      ++It;
    }

    while (It->Begin <= range.second) {
      ++num;

      if (++It == End)
        break;
    }

    return num;
  }

  //
  iterator lower_bound(key_type const &key) {

    container_iterator It = std::upper_bound(Items.begin(), Items.end(), key,
                                             ValueComparator);
    // Check the previous element
    if (It != Items.begin()) {
      --It; // Now It->Begin <= key
      if (key <= It->End)
        return iterator(It);
      ++It;
    }

    return iterator(It);
  }

  //
  const_iterator lower_bound(key_type const &key) const {
    container_iterator It = std::upper_bound(Items.begin(), Items.end(), key,
                                             ValueComparator);
    // Check the previous element
    if (It != Items.begin()) {
      --It; // Now It->Begin <= key
      if (key <= It->End)
        return const_iterator(It);
      ++It;
    }

    return const_iterator(It);
  }

  //
  iterator upper_bound(key_type const &key) {
    container_iterator It = std::upper_bound(Items.begin(), Items.end(), key,
                                             ValueComparator);
    return iterator(It);
  }

  //
  const_iterator upper_bound(key_type const &key) const {
    container_iterator It = std::upper_bound(Items.begin(), Items.end(), key,
                                             ValueComparator);
    return const_iterator(It);
  }

  //
  std::pair<iterator, iterator> equal_range(key_type const &key) {
    iterator It = lower_bound(key); // It->Begin >= key
    if (It.It != Items.end() && It->Begin <= key && key <= It->End)
      return std::make_pair(It, It + 1);
    return std::make_pair(It, It);
  }

  //
  std::pair<const_iterator, const_iterator>
  equal_range(key_type const &key) const {
    const_iterator It = lower_bound(key); // It->Begin >= key
    if (It.It != Items.end() && It->Begin <= key && key <= It->End)
      return std::make_pair(It, It + 1);
    return std::make_pair(It, It);
  }

  /// Return the object used to compare keys.
  key_compare key_comp() { return KeyComparator; }

  /// Return the object used to compare values.
  value_compare value_comp() { return ValueComparator; }

  /// Insert element.
  std::pair<iterator, bool> insert(value_type const &Element) {
    iterator It = upper_bound(Element.Begin);

    if (It.It != Items.begin()) {
      --It; // Now It->Begin <= Element.Begin
      if (It->End >= Element.Begin) {
        return std::make_pair(It, false);
      }
      ++It;
    }

    It = iterator(Items.insert(It.It, Element));
    return std::make_pair(It, true);
  }
  
  /// Insert element.
  std::pair<iterator, bool> insert(key_type const &Begin, key_type const &End,
                                   data_type const &Value) {
    iterator It = upper_bound(Begin);
    
    if (It.It != Items.begin()) {
      --It; // Now It->Begin <= Begin
      if (It->End >= Begin) {
        return std::make_pair(It, false);
      }
      ++It;
    }
    
    It = iterator(Items.insert(It.It, makeIntervalMapItem(Begin, End, Value)));
    return std::make_pair(It, true);
  }

  /// Insert range.
  template<class InputIterator>
  void insert(InputIterator first, InputIterator last) {
    iterator It;

    for (; first != last; ++first) {
      It = upper_bound(first->Begin);

      if (It.It != Items.begin()) {
        --It;
        if (It->End >= first->Begin) {
          continue;
        }
        ++It;
      }

      Items.insert(It.It, *first);
    }
  }
};


} // namespace seec

#endif // _SEEC_DSA_INTERVAL_MAP_VECTOR_HPP_
