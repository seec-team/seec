//===- IndexTypes.hpp ----------------------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_INDEXTYPES_HPP
#define SEEC_UTIL_INDEXTYPES_HPP

#include <type_safe/strong_typedef.hpp>

namespace seec {

/// \brief A zero-based thread ID.
struct ThreadIDTy
: type_safe::strong_typedef<ThreadIDTy, uint32_t>,
  type_safe::strong_typedef_op::equality_comparison<ThreadIDTy>,
  type_safe::strong_typedef_op::relational_comparison<ThreadIDTy>
{
  using strong_typedef::strong_typedef;
};

} // namespace seec

#endif // define SEEC_UTIL_INDEXTYPES_HPP
