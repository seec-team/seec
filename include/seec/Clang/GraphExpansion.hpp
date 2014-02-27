//===- include/seec/Clang/GraphExpansion.hpp ------------------------------===//
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

#ifndef SEEC_LIB_CLANG_GRAPHEXPANSION_HPP
#define SEEC_LIB_CLANG_GRAPHEXPANSION_HPP

#include "seec/Clang/MappedValue.hpp"

#include <memory>
#include <vector>


namespace seec {

namespace cm {

class ProcessState;

namespace graph {


/// \brief Represents a dereference of a pointer with a given index (e.g. p[i]).
///
class Dereference {
  /// The base pointer.
  std::shared_ptr<ValueOfPointer const> Pointer;
  
  /// The index dereferenced.
  unsigned Index;
  
public:
  /// \brief Constructor.
  ///
  Dereference(std::shared_ptr<ValueOfPointer const> OfPointer,
              unsigned WithIndex)
  : Pointer(std::move(OfPointer)),
    Index(WithIndex)
  {}
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the pointer that is the source of this reference.
  ///
  std::shared_ptr<ValueOfPointer const> const &getPointer() const {
    return Pointer;
  }
  
  /// \brief Get the index used with the pointer to achieve this reference.
  ///
  unsigned getIndex() const { return Index; }
  
  /// @} (Accessors.)
};


class ExpansionImpl;


/// \brief Stores information about a state that has been expanded for graphing.
///
class Expansion final {
  /// Internal implementation.
  std::unique_ptr<ExpansionImpl const> Impl;
  
  /// \brief Constructor.
  ///
  Expansion();
  
public:
  /// \brief Move constructor.
  ///
  Expansion(Expansion &&) = default;
  
  /// \brief Destructor.
  ///
  ~Expansion();
  
  /// \brief Create an \c Expansion for a \c seec::cm::ProcessState.
  ///
  static Expansion from(seec::cm::ProcessState const &State);
  
  
  /// \name Pointers.
  /// @{
  
  /// \brief Check if a \c Value is directly referenced by a pointer.
  ///
  bool isReferencedDirectly(Value const &Value) const;
  
  /// \brief Get all pointers that point directly into a memory area.
  ///
  std::vector<std::shared_ptr<ValueOfPointer const>>
  getReferencesOfArea(uintptr_t Start, uintptr_t End) const;
  
  /// \brief Check if any pointer points into a memory area.
  ///
  bool isAreaReferenced(uintptr_t Start, uintptr_t End) const;
  
  /// \brief Get all pointers.
  ///
  std::vector<std::shared_ptr<ValueOfPointer const>>
  getAllPointers() const;
  
  /// @} (Pointers.)
};


/// \brief Reduce a set of references to the most informative types.
///
void reduceReferences(std::vector<std::shared_ptr<ValueOfPointer const>> &Refs);


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_LIB_CLANG_GRAPHEXPANSION_HPP
