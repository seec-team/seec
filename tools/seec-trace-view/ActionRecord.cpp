//===- tools/seec-trace-view/ActionRecord.cpp -----------------------------===//
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

#include <wx/xml/xml.h>

#include "ActionRecord.hpp"


ActionRecord::ActionRecord()
: Enabled(false),
  RecordDocument(new wxXmlDocument())
{}

ActionRecord::~ActionRecord() = default;

void ActionRecord::enable()
{
  
}

void ActionRecord::disable()
{
  
}

void ActionRecord::finish()
{
  
}
