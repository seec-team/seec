//===- tools/seec-trace-view/ProcessTimeGauge.hpp -------------------------===//
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

#ifndef SEEC_TRACE_VIEW_PROCESSTIMEGAUGE_HPP
#define SEEC_TRACE_VIEW_PROCESSTIMEGAUGE_HPP

#include <wx/wx.h>
#include <wx/panel.h>

#include <functional>
#include <memory>

class StateAccessToken;
class wxGauge;

namespace seec {
  namespace cm {
    class ProcessState;
    class ThreadState;
  }
}

class ProcessTimeGauge : public wxPanel
{
  wxGauge *m_Gauge;
  
public:
  DECLARE_DYNAMIC_CLASS(ProcessTimeGauge)
  
  ProcessTimeGauge();
  
  ProcessTimeGauge(wxWindow *Parent)
  : ProcessTimeGauge()
  {
    Create(Parent);
  }
  
  virtual ~ProcessTimeGauge();
  
  /// \brief Initialize the state of this object.
  ///
  bool Create(wxWindow *Parent);
  
  /// \brief Update this control to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread,
            size_t ThreadIndex);
};

#endif // define SEEC_TRACE_VIEW_PROCESSTIMEGAUGE_HPP
