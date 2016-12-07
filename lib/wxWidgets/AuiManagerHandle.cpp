//===- lib/wxWidgets/AuiManagerHandle.cpp ---------------------------------===//
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

#include "seec/wxWidgets/AuiManagerHandle.hpp"

#include <wx/aui/aui.h>

namespace seec {

wxAuiManagerHandle::~wxAuiManagerHandle()
{
  if (m_Manager) {
    m_Manager->UnInit();
  }
}

} // namespace seec
