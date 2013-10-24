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
  /// Whether or not recording is enabled for this record.
  bool Enabled;
  
  /// Used to record user interactions.
  std::unique_ptr<wxXmlDocument> RecordDocument;
  
public:
  /// \brief Create a new action record.
  ///
  ActionRecord();
  
  /// \brief Destroy this action record.
  ///
  ~ActionRecord();
  
  /// \brief Enable recording for this record.
  ///
  void enable();
  
  /// \brief Disable recording for this record.
  ///
  void disable();
  
  /// \brief Finish this action record and submit it to the server.
  ///
  void finish();
};

#endif // SEEC_TRACE_VIEW_ACTIONRECORD_HPP
