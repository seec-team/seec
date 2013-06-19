//===- lib/wxWidgets/ICUBundleFSHandler.cpp -------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file The implementation of a wxWidgets virtual file system handler for
///       reading files from ICU resource bundles.
///
//===----------------------------------------------------------------------===//

#include "seec/ICU/Resources.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/wxWidgets/ICUBundleFSHandler.hpp"

#include <wx/mstream.h>

namespace seec {

static seec::Maybe<ResourceBundle> getResourceAt(wxString const &Location)
{
  auto const FirstSlash = Location.find('/');
  if (FirstSlash == wxString::npos)
    return seec::Maybe<ResourceBundle>();
  
  UErrorCode Status = U_ZERO_ERROR;
  ResourceBundle Bundle(Location.substr(0, FirstSlash).utf8_str(),
                        Locale(),
                        Status);
  if (U_FAILURE(Status))
    return seec::Maybe<ResourceBundle>();
  
  auto SearchFrom = FirstSlash + 1;
  
  while (SearchFrom < Location.size()) {
    auto const NextSlash = Location.find('/', SearchFrom);
    auto const Length = NextSlash != wxString::npos
                      ? NextSlash - SearchFrom
                      : Location.size() - SearchFrom;
    if (Length == 0)
      break; // Allow paths to end with a slash.
    
    Bundle = Bundle.get(Location.substr(SearchFrom, Length).utf8_str(), Status);
    if (U_FAILURE(Status))
      return seec::Maybe<ResourceBundle>();
    
    if (NextSlash == wxString::npos)
      break;
    
    SearchFrom = NextSlash + 1;
  }
  
  return Bundle;
}

bool ICUBundleFSHandler::CanOpen(wxString const &Location)
{
  auto const Protocol = GetProtocol(Location);
  
  if (Protocol == wxString("icurb"))
    return true;
  
  return false;
}

wxFSFile *ICUBundleFSHandler::OpenFile(wxFileSystem &Parent,
                                       wxString const &Location)
{
  auto const Right = GetRightLocation(Location);
  
  auto const MaybeBundle = getResourceAt(Right);
  if (!MaybeBundle.assigned<ResourceBundle>())
    return nullptr;
  
  int32_t Length = 0;
  UErrorCode Status = U_ZERO_ERROR;
  
  auto const Data = MaybeBundle.get<ResourceBundle>().getBinary(Length, Status);
  if (U_FAILURE(Status))
    return nullptr;
  
  if (Length < 0 )
    return nullptr;
  
  auto const VoidData = reinterpret_cast<void const *>(Data);
  
  return new wxFSFile(new wxMemoryInputStream(VoidData,
                                              static_cast<size_t>(Length)),
                      Right,
                      wxString(),
                      wxString(),
                      wxDateTime::Now());
}

} // namespace seec
