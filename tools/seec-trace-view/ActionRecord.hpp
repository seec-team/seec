//===- tools/seec-trace-view/ActionRecord.hpp -----------------------------===//
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

#ifndef SEEC_TRACE_VIEW_ACTIONRECORD_HPP
#define SEEC_TRACE_VIEW_ACTIONRECORD_HPP

#include <memory>


class wxXmlDocument;


/// \brief Records user interactions with the trace viewer.
///
class ActionRecord
{
  /// Used to record user interactions.
  std::unique_ptr<wxXmlDocument> RecordDocument;
  
public:
  /// \brief 
  ///
  ActionRecord();
  
  /// \brief
  ///
  ~ActionRecord();
};

#endif // SEEC_TRACE_VIEW_ACTIONRECORD_HPP
