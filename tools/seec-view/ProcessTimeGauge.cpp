//===- tools/seec-trace-view/ProcessTimeGauge.cpp -------------------------===//
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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include <wx/gauge.h>
#include <wx/sizer.h>

#include "ProcessTimeGauge.hpp"

IMPLEMENT_DYNAMIC_CLASS(ProcessTimeGauge, wxPanel);

ProcessTimeGauge::ProcessTimeGauge() = default;

ProcessTimeGauge::~ProcessTimeGauge() = default;

bool ProcessTimeGauge::Create(wxWindow *Parent)
{
  if (!wxPanel::Create(Parent, wxID_ANY))
    return false;
  
  m_Gauge = new wxGauge(this, wxID_ANY, /* range */ 1,
                        wxDefaultPosition,
                        wxDefaultSize,
                        wxGA_HORIZONTAL | wxGA_SMOOTH);
  if (!m_Gauge)
    return false;

  m_Gauge->Pulse();
  
  auto const Sizer = new wxBoxSizer(wxVERTICAL);
  Sizer->Add(m_Gauge, wxSizerFlags().Expand());
  SetSizerAndFit(Sizer);

  return true;
}

void ProcessTimeGauge::show(std::shared_ptr<StateAccessToken> Access,
                            seec::cm::ProcessState const &Process,
                            seec::cm::ThreadState const &Thread,
                            size_t ThreadIndex)
{
  if (!m_Gauge)
    return;

  auto const &UnmappedProcess = Process.getUnmappedProcessState();
  auto const TimeEnd = UnmappedProcess.getTrace().getFinalProcessTime();
  auto const TimeNow = UnmappedProcess.getProcessTime();
  m_Gauge->SetRange(TimeEnd);
  m_Gauge->SetValue(TimeNow);
}
