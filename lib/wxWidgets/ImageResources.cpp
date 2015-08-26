//===- lib/wxWidgets/ImageResources.cpp -----------------------------------===//
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

#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/ImageResources.hpp"

#include <wx/mstream.h>

namespace seec {

wxImage getwxImage(ResourceBundle const &Resource, UErrorCode &Status)
{
  wxImage Image;

  auto Data = seec::getBinary(Resource, Status);
  if (U_FAILURE(Status))
    return Image;

  wxMemoryInputStream Stream(Data.data(), Data.size());

  Image.LoadFile(Stream);

  return Image;
}

wxImage getwxImageEx(ResourceBundle const &Bundle,
                     char const *Key,
                     UErrorCode &Status)
{
  return getwxImage(Bundle.get(Key, Status), Status);
}

} // namespace seec
