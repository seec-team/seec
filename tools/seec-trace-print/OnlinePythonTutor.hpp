//===- tools/seec-trace-print/OnlinePythonTutor.hpp -----------------------===//
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

#ifndef SEEC_TRACE_PRINT_ONLINEPYTHONTUTOR_HPP
#define SEEC_TRACE_PRINT_ONLINEPYTHONTUTOR_HPP

namespace seec {
  namespace cm {
    class ProcessTrace;
  }
}

class OPTSettings {
  bool PyCrazyMode;

public:
  OPTSettings()
  : PyCrazyMode(false)
  {}

  bool getPyCrazyMode() const { return PyCrazyMode; }

  OPTSettings &setPyCrazyMode(bool const Value) {
    PyCrazyMode = Value;
    return *this;
  }
};

void PrintOnlinePythonTutor(seec::cm::ProcessTrace const &Trace,
                            OPTSettings const &Settings);

#endif // SEEC_TRACE_PRINT_ONLINEPYTHONTUTOR_HPP
