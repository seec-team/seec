//===- IndexTypesForLLVMObjects.hpp --------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_INDEXTYPESFORLLVMOBJECTS_HPP
#define SEEC_UTIL_INDEXTYPESFORLLVMOBJECTS_HPP

#include <type_safe/strong_typedef.hpp>

namespace seec {

struct InstrIndexInFn
: type_safe::strong_typedef<InstrIndexInFn, uint32_t>,
  type_safe::strong_typedef_op::equality_comparison<InstrIndexInFn>,
  type_safe::strong_typedef_op::relational_comparison<InstrIndexInFn>,
  type_safe::strong_typedef_op::mixed_relational_comparison<InstrIndexInFn,
                                                            size_t>
{
  using strong_typedef::strong_typedef;
  
  uint32_t raw() const { return static_cast<uint32_t>(*this); }
};

} // namespace seec

#endif // define SEEC_UTIL_INDEXTYPESFORLLVMOBJECTS_HPP
