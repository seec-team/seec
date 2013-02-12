//===- seec/Util/Fallthrough.hpp ------------------------------------ C++ -===//
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

#ifndef SEEC_UTIL_FALLTHROUGH_HPP
#define SEEC_UTIL_FALLTHROUGH_HPP

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if __has_feature(cxx_attributes)
#define SEEC_FALLTHROUGH [[clang::fallthrough]]
#else
#define SEEC_FALLTHROUGH
#endif

#endif
