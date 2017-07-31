//===- seec/Util/UpcomingStandardFeatures.hpp ----------------------- C++ -===//
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

#include <type_traits>

namespace std {

#if __cplusplus < 201402L
// C++14's enable_if_t:
template<bool B, class T = void>
using enable_if_t = typename std::enable_if<B,T>::type;
#endif

} // namespace std
