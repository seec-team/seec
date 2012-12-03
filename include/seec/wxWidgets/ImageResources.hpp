//===- include/seec/wxWidgets/ImageResources.hpp -------------------- C++ -===//
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

#ifndef SEEC_WXWIDGETS_IMAGERESOURCES_HPP
#define SEEC_WXWIDGETS_IMAGERESOURCES_HPP

#include <unicode/unistr.h>
#include <unicode/resbund.h>

#include <wx/image.h>

namespace seec {

/// \brief Extract binary data from a ResourceBundle and attempt to load it as
///        a wxImage.
wxImage getwxImageEx(ResourceBundle const &Bundle,
                     char const *Key,
                     UErrorCode &Status);

} // namespace seec

#endif // SEEC_WXWIDGETS_IMAGERESOURCES_HPP
