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

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(clang::fallthrough)
#define SEEC_FALLTHROUGH [[clang::fallthrough]]
#else
#define SEEC_FALLTHROUGH
#endif

#endif
