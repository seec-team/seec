//===- include/seec/wxWidgets/QueueEvent.hpp ------------------------ C++ -===//
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

#ifndef SEEC_WXWIDGETS_QUEUEEVENT_HPP
#define SEEC_WXWIDGETS_QUEUEEVENT_HPP

#include "llvm/ADT/STLExtras.h"
#include <type_safe/boolean.hpp>
#include <wx/wx.h>

template<typename EvT, typename... ArgTs>
type_safe::boolean queueEvent(wxEvtHandler &Handler,
                              wxEventType const Type,
                              int const WinID,
                              ArgTs &&...Args)
{
  auto Ev = llvm::make_unique<EvT>(Type, WinID,
                                   std::forward<ArgTs>(Args)...);
  Ev->SetEventObject(&Handler);
  Handler.QueueEvent(Ev.release());
  return true;
}

template<typename EvT, typename... ArgTs>
type_safe::boolean queueEvent(wxWindow &Control,
                              wxEventType const Type,
                              ArgTs &&...Args)
{
  if (auto const Handler = Control.GetEventHandler()) {
    return queueEvent<EvT>(*Handler, Type, Control.GetId(),
                           std::forward<ArgTs>(Args)...);
  }
  else {
    return false;
  }
}

#endif // SEEC_WXWIDGETS_QUEUEEVENT_HPP
