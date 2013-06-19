//===- include/seec/wxWidgets/ICUBundleFSHandler.hpp ---------------- C++ -===//
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

#ifndef SEEC_WXWIDGETS_ICUBUNDLEFSHANDLER_HPP
#define SEEC_WXWIDGETS_ICUBUNDLEFSHANDLER_HPP

#include <wx/filesys.h>

namespace seec {

class ICUBundleFSHandler final : public wxFileSystemHandler
{
public:
  virtual bool CanOpen(wxString const &Location) override;
  
  virtual wxFSFile *
  OpenFile(wxFileSystem &Parent, wxString const &Location) override;
};

} // namespace seec

#endif // SEEC_WXWIDGETS_ICUBUNDLEFSHANDLER_HPP
