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

void PrintOnlinePythonTutor(seec::cm::ProcessTrace const &Trace);

#endif // SEEC_TRACE_PRINT_ONLINEPYTHONTUTOR_HPP
