//===- lib/wxWidgets/Config.cpp -------------------------------------------===//
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

#include "seec/wxWidgets/ConfigTracing.hpp"

#include <wx/config.h>
#include <wx/log.h>

namespace seec {

char const * const cConfigKeyForThreadEventLimit = "/Tracing/ThreadEventLimit";
char const * const cConfigKeyForArchiveLimit     = "/Tracing/ArchiveLimit";

static constexpr long getDefaultThreadEventLimit() {
  return 1024; // 1 GiB in MiB
}

static constexpr long getDefaultArchiveLimit() {
  return 512; // 0.5 GiB in MiB
}

long getThreadEventLimit()
{
  auto const Config = wxConfig::Get();
  return Config->ReadLong(cConfigKeyForThreadEventLimit,
                          getDefaultThreadEventLimit());
}

bool setThreadEventLimit(long const Limit)
{
  if (Limit < 0)
    return false;

  auto const Config = wxConfig::Get();

  if (!Config->Write(cConfigKeyForThreadEventLimit, Limit))
    return false;

  return Config->Flush();
}

long getArchiveLimit()
{
  auto const Config = wxConfig::Get();
  return Config->ReadLong(cConfigKeyForArchiveLimit, getDefaultArchiveLimit());
}

bool setArchiveLimit(long const Limit)
{
  if (Limit < 0)
    return false;

  auto const Config = wxConfig::Get();

  if (!Config->Write(cConfigKeyForArchiveLimit, Limit))
    return false;

  return Config->Flush();
}

} // namespace seec
