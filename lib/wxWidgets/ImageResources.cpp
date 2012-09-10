#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/ImageResources.hpp"

#include <wx/mstream.h>

namespace seec {

wxImage getwxImageEx(ResourceBundle const &Bundle,
                     char const *Key,
                     UErrorCode &Status) {
  wxImage Image;

  auto Data = seec::getBinaryEx(Bundle, Key, Status);
  if (U_FAILURE(Status))
    return Image;

  wxMemoryInputStream Stream(Data.data(), Data.size());

  Image.LoadFile(Stream);

  return Image;
}

} // namespace seec
