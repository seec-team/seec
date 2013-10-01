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

#ifndef SEEC_TRACE_VIEW_INTERNATIONALIZEDBUTTON_HPP
#define SEEC_TRACE_VIEW_INTERNATIONALIZEDBUTTON_HPP

#include <unicode/resbund.h>
#include <wx/wx.h>

class wxButton;

wxButton *makeInternationalizedButton(wxWindow *Parent,
                                      wxWindowID const ID,
                                      ResourceBundle const &TextResource,
                                      char const *TextKey,
                                      ResourceBundle const &ImageResource,
                                      char const *ImageKey,
                                      wxSize const ImageSize);

#endif // SEEC_TRACE_VIEW_INTERNATIONALIZEDBUTTON_HPP
