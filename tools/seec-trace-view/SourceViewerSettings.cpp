//===- SourceViewerSettings.cpp -------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/log.h>
#include <wx/tokenzr.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "SourceViewerSettings.hpp"


seec::util::Maybe<SciStyle> getDefaultStyle(char const *StyleName) {
  // Find the default setting for this style in our ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto Table = seec::getResource("TraceViewer",
                                 Locale::getDefault(),
                                 Status,
                                 "ScintillaStyles",
                                 StyleName);
  if (U_FAILURE(Status))
    return seec::util::Maybe<SciStyle>();
  
  // Get the individual values from the default setting table.
  auto Name = seec::getwxStringExOrEmpty(Table, "Name");
  auto ForegroundStr = seec::getwxStringExOrEmpty(Table, "Foreground");
  auto BackgroundStr = seec::getwxStringExOrEmpty(Table, "Background");
  auto FontNameStr = seec::getwxStringExOrEmpty(Table, "FontName");
  auto FontSize = seec::getIntEx(Table, "FontSize", Status);
  auto FontStyleStr = seec::getwxStringExOrEmpty(Table, "FontStyle");
  auto LetterCase = seec::getIntEx(Table, "LetterCase", Status);
  
  if (U_FAILURE(Status))
    return seec::util::Maybe<SciStyle>();
  
  auto FontStyle = wxFONTSTYLE_NORMAL;
  auto FontWeight = wxFONTWEIGHT_NORMAL;
  bool FontUnderline = false;
  
  wxStringTokenizer StyleTokenizer(FontStyleStr, " ");
  while (StyleTokenizer.HasMoreTokens()) {
    wxString Token = StyleTokenizer.GetNextToken();
    
    // Check against known tokens case-insensitively.
    if (Token.IsSameAs(wxString("Italic"), false))
      FontStyle = wxFONTSTYLE_ITALIC;
    else if (Token.IsSameAs(wxString("Slant"), false))
      FontStyle = wxFONTSTYLE_SLANT;
    else if (Token.IsSameAs(wxString("Light"), false))
      FontWeight = wxFONTWEIGHT_LIGHT;
    else if (Token.IsSameAs(wxString("Bold"), false))
      FontWeight = wxFONTWEIGHT_BOLD;
    else if (Token.IsSameAs(wxString("Max"), false))
      FontWeight = wxFONTWEIGHT_MAX;
    else if (Token.IsSameAs(wxString("Underline"), false))
      FontUnderline = true;
    else {
      wxLogError("While reading the default style for \"%s\", the style token "
                 "\"%s\" was encountered, but not recognized.",
                 StyleName,
                 Token.c_str());
    }
  }
  
  return SciStyle(std::move(Name),
                  wxColour(ForegroundStr),
                  wxColour(BackgroundStr),
                  wxFont(FontSize,
                         wxFONTFAMILY_MODERN,
                         FontStyle,
                         FontWeight,
                         FontUnderline),
                  LetterCase);
}


//===----------------------------------------------------------------------===//
// SciCommonType
//===----------------------------------------------------------------------===//

char const *getSciTypeName(SciCommonType Type) {
  switch (Type) {
#define SEEC_SCI_COMMON_TYPE(TYPE, ID) \
    case SciCommonType::TYPE: return #TYPE;
#include "SourceViewerSettingsTypes.def"
  }
  
  // Unreachable if Type is valid.
  return nullptr;
}

seec::util::Maybe<SciCommonType> getSciCommonTypeFromName(llvm::StringRef Name){
#define SEEC_SCI_COMMON_TYPE(TYPE, ID) \
  if (Name.equals(#TYPE))   \
    return SciCommonType::TYPE;
#include "SourceViewerSettingsTypes.def"

  return seec::util::Maybe<SciCommonType>();
}

llvm::ArrayRef<SciCommonType> getAllSciCommonTypes() {
  static SciCommonType Types[] = {
#define SEEC_SCI_COMMON_TYPE(TYPE, ID) \
    SciCommonType::TYPE,
#include "SourceViewerSettingsTypes.def"
  };
  
  return llvm::ArrayRef<SciCommonType>(Types);
}

seec::util::Maybe<SciStyle> getDefaultStyle(SciCommonType Type) {
  // First get the name of this style type.
  auto StyleName = getSciTypeName(Type);
  if (!StyleName)
    return seec::util::Maybe<SciStyle>();
  
  return getDefaultStyle(StyleName);
}


//===----------------------------------------------------------------------===//
// SciLexerType
//===----------------------------------------------------------------------===//

char const *getSciTypeName(SciLexerType Type) {
  switch (Type) {
#define SEEC_SCI_TYPE(TYPE, ID) \
    case SciLexerType::TYPE: return #TYPE;
#include "SourceViewerSettingsTypes.def"
  }
  
  // Unreachable if Type is valid.
  return nullptr;
}

seec::util::Maybe<SciLexerType> getSciLexerTypeFromName(llvm::StringRef Name) {
#define SEEC_SCI_TYPE(TYPE, ID) \
  if (Name.equals(#TYPE))   \
    return SciLexerType::TYPE;
#include "SourceViewerSettingsTypes.def"

  return seec::util::Maybe<SciLexerType>();
}

llvm::ArrayRef<SciLexerType> getAllSciLexerTypes() {
  static SciLexerType Types[] = {
#define SEEC_SCI_TYPE(TYPE, ID) \
    SciLexerType::TYPE,
#include "SourceViewerSettingsTypes.def"
  };
  
  return llvm::ArrayRef<SciLexerType>(Types);
}

seec::util::Maybe<SciStyle> getDefaultStyle(SciLexerType Type) {
  // First get the name of this style type.
  auto StyleName = getSciTypeName(Type);
  if (!StyleName)
    return seec::util::Maybe<SciStyle>();
  
  return getDefaultStyle(StyleName);
}
