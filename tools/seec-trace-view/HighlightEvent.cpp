//===- tools/seec-trace-view/HighlightEvent.cpp ---------------------------===//
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

#include "HighlightEvent.hpp"


IMPLEMENT_CLASS(HighlightEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_HIGHLIGHT_ON, HighlightEvent);
wxDEFINE_EVENT(SEEC_EV_HIGHLIGHT_OFF, HighlightEvent);


wxEvent *HighlightEvent::Clone() const {
  return new HighlightEvent(*this);
}
