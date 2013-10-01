//===- tools/seec-trace-view/InternationalizedButton.hpp ------------------===//
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

#include "seec/wxWidgets/ImageResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/bmpbuttn.h>

#include "InternationalizedButton.hpp"

wxButton *makeInternationalizedButton(wxWindow *Parent,
                                      wxWindowID const ID,
                                      ResourceBundle const &TextResource,
                                      char const *TextKey,
                                      ResourceBundle const &ImageResource,
                                      char const *ImageKey,
                                      wxSize const ImageSize)
{
  UErrorCode Status = U_ZERO_ERROR;
  
  auto const Text = seec::getwxStringExOrEmpty(TextResource, TextKey);
  auto Image = seec::getwxImageEx(ImageResource, ImageKey, Status);
  
  if (Image.IsOk()) {
    Image.Rescale(ImageSize.GetWidth(),
                  ImageSize.GetHeight(),
                  wxIMAGE_QUALITY_HIGH);
    
    return new wxBitmapButton(Parent, ID, Image);
  }
  else {
    return new wxButton(Parent, ID, Text);
  }
}
