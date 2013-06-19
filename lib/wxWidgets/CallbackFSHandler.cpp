//===- lib/wxWidgets/CallbackFSHandler.cpp --------------------------------===//
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

#include "seec/wxWidgets/CallbackFSHandler.hpp"

#include <wx/sstream.h>
#include <wx/tokenzr.h>

namespace seec {

namespace callbackfs {


//===----------------------------------------------------------------------===//
// CallbackBase
//===----------------------------------------------------------------------===//

CallbackBase::~CallbackBase() = default;

std::string CallbackBase::call(wxString const &Right) const
{
  wxStringTokenizer Tokenizer(Right, "/");
  std::vector<std::string> Split;
  
  while (Tokenizer.HasMoreTokens())
    Split.emplace_back(Tokenizer.GetNextToken().ToStdString());
  
  auto const ReceivedArgs = Split.size();
  
  if (ReceivedArgs < ArgCount)
    return "{ success: false, error: \"Insufficient arguments.\" }";
  else if (ReceivedArgs > ArgCount)
    return "{ success: false, error: \"Too many arguments.\" }";
  
  std::string Result = "{ success: true, result: ";
  Result += impl(Split);
  Result += " }";
  
  return Result;
}


} // namespace callbackfs (in seec)


//===----------------------------------------------------------------------===//
// CallbackFSHandler
//===----------------------------------------------------------------------===//

bool
CallbackFSHandler::
addCallback(wxString const &Identifier,
            std::unique_ptr<seec::callbackfs::CallbackBase> Callback)
{
  auto const Added = Callbacks.insert(std::make_pair(Identifier,
                                                     std::move(Callback)));
  
  return Added.second;
}

bool CallbackFSHandler::CanOpen(wxString const &Location)
{
  if (Protocol != GetProtocol(Location))
    return false;
  
  // Check the callback (first part of the path).
  auto const Right = GetRightLocation(Location);
  
  auto const SlashPos = Right.find('/');
  if (SlashPos == wxString::npos)
    return false;
  
  return Callbacks.count(Right.substr(0, SlashPos));
}

wxFSFile *CallbackFSHandler::OpenFile(wxFileSystem &Parent,
                                       wxString const &Location)
{
  auto const Right = GetRightLocation(Location).ToStdString();
  
  // Get the callback (first part of the path).
  auto const SlashPos = Right.find('/');
  if (SlashPos == std::string::npos)
    return nullptr;
  
  auto const CallbackIt = Callbacks.find(Right.substr(0, SlashPos));
  if (CallbackIt == Callbacks.end())
    return nullptr;
  
  auto const &Callback = CallbackIt->second;
  
  // Find the start of the query string (if any).
  auto const QueryPos = Right.find('?');
  
  // The argument string is everything after the slash, prior to the question
  // mark if there is one, or to the end of the string otherwise.
  auto const ArgsLength = (QueryPos == std::string::npos)
                        ? std::string::npos
                        : (QueryPos - (SlashPos + 1));
  
  auto Result = Callback->call(Right.substr(SlashPos + 1, ArgsLength));
  
  // If this was a jsonp request, add the padding now.
  auto const JSONPKey = std::string{"callback="};
  auto const KeyPos = Right.find(JSONPKey, QueryPos);
  
  if (KeyPos != std::string::npos) {
    // Find the value of the callback parameter.
    auto const ValuePos = KeyPos + JSONPKey.size();
    auto const ValueEnd = Right.find('&');
    auto const ValueLength = (ValueEnd == std::string::npos)
                           ? std::string::npos
                           : (ValueEnd - ValuePos);
    
    // Make the result string equal to "callback_value(result)".
    Result.insert(0, 1, '(');
    Result.insert(0, Right.substr(ValuePos, ValueLength));
    Result.push_back(')');
  }
  
  return new wxFSFile(new wxStringInputStream(Result),
                      Right,
                      wxString("application/javascript"),
                      wxString(),
                      wxDateTime::Now());
}


} // namespace seec
