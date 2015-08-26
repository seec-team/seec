//===- include/seec/wxWidgets/ConfigTracing.hpp --------------------- C++ -===//
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

#ifndef SEEC_WXWIDGETS_CONFIGTRACING_HPP
#define SEEC_WXWIDGETS_CONFIGTRACING_HPP

namespace seec {

/// \brief Get the limit of uncompressed thread event files (in MiB).
///
long getThreadEventLimit();

/// \brief Set the limit of uncompressed thread event files (in MiB).
///
bool setThreadEventLimit(long const Limit);

/// \brief Set the limit of archiving (in MiB).
///
long getArchiveLimit();

/// \brief Set the limit of archiving traces (in MiB).
///
bool setArchiveLimit(long const Limit);

} // namespace seec

#endif // SEEC_WXWIDGETS_CONFIGTRACING_HPP
