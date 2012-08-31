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


//===----------------------------------------------------------------------===//
// SciIndicatorType
//===----------------------------------------------------------------------===//

char const *getSciIndicatorTypeName(SciIndicatorType Type) {
  switch (Type) {
#define SEEC_SCI_INDICATOR(TYPE) \
    case SciIndicatorType::TYPE: return #TYPE;
#include "SourceViewerSettingsTypes.def"
  }
  
  // Unreachable if Type is valid.
  return nullptr;
}

seec::util::Maybe<SciIndicatorType>
getSciIndicatorTypeFromName(llvm::StringRef Name) {
#define SEEC_SCI_INDICATOR(TYPE) \
  if (Name.equals(#TYPE))   \
    return SciIndicatorType::TYPE;
#include "SourceViewerSettingsTypes.def"

  return seec::util::Maybe<SciIndicatorType>();
}

llvm::ArrayRef<SciIndicatorType> getAllSciIndicatorTypes() {
  static SciIndicatorType Types[] = {
#define SEEC_SCI_INDICATOR(TYPE) \
    SciIndicatorType::TYPE,
#include "SourceViewerSettingsTypes.def"
  };
  
  return llvm::ArrayRef<SciIndicatorType>(Types);
}

seec::util::Maybe<SciIndicatorStyle>
getDefaultIndicatorStyle(SciIndicatorType Type) {
  // First get the name of this indicator style type.
  auto StyleName = getSciIndicatorTypeName(Type);
  if (!StyleName)
    return seec::util::Maybe<SciIndicatorStyle>();
  
  // Find the default setting for this indicator style in our ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto Table = seec::getResource("TraceViewer",
                                 Locale::getDefault(),
                                 Status,
                                 "ScintillaIndicatorStyles",
                                 StyleName);
  if (U_FAILURE(Status))
    return seec::util::Maybe<SciIndicatorStyle>();
  
  // Get the individual values from the default setting table.
  auto Name = seec::getwxStringExOrEmpty(Table, "Name");
  auto StyleStr = seec::getwxStringExOrEmpty(Table, "Style");
  auto ForegroundStr = seec::getwxStringExOrEmpty(Table, "Foreground");
  auto Alpha = seec::getIntEx(Table, "Alpha", Status);
  auto OutlineAlpha = seec::getIntEx(Table, "OutlineAlpha", Status);
  auto UnderStr = seec::getwxStringExOrEmpty(Table, "Under");
  
  if (U_FAILURE(Status))
    return seec::util::Maybe<SciIndicatorStyle>();
  
  // Match the style string to a style.
  int Style = -1;
  
#define SEEC_MATCH_INDICATOR_STYLE(NAME)         \
  if (StyleStr.IsSameAs(wxString(#NAME), false)) \
    Style = wxSTC_INDIC_##NAME;

  SEEC_MATCH_INDICATOR_STYLE(PLAIN)
  SEEC_MATCH_INDICATOR_STYLE(SQUIGGLE)
  SEEC_MATCH_INDICATOR_STYLE(TT)
  SEEC_MATCH_INDICATOR_STYLE(DIAGONAL)
  SEEC_MATCH_INDICATOR_STYLE(STRIKE)
  SEEC_MATCH_INDICATOR_STYLE(HIDDEN)
  SEEC_MATCH_INDICATOR_STYLE(BOX)
  SEEC_MATCH_INDICATOR_STYLE(ROUNDBOX)
  SEEC_MATCH_INDICATOR_STYLE(STRAIGHTBOX)
  SEEC_MATCH_INDICATOR_STYLE(DASH)
  SEEC_MATCH_INDICATOR_STYLE(DOTS)
  SEEC_MATCH_INDICATOR_STYLE(SQUIGGLELOW)
  SEEC_MATCH_INDICATOR_STYLE(DOTBOX)
  
#undef SEEC_MATCH_INDICATOR_STYLE

  if (Style == -1)
    return seec::util::Maybe<SciIndicatorStyle>();
  
  // Ensure that the alpha values are within the acceptable range.
  if (Alpha < 0) Alpha = 0;
  if (Alpha > 255) Alpha = 255;
  if (OutlineAlpha < 0) OutlineAlpha = 0;
  if (OutlineAlpha > 255) OutlineAlpha = 255;
  
  // Get the Under value as a bool.
  bool Under = false;
  
  if (UnderStr.CmpNoCase("TRUE"))
    Under = true;
  else if (UnderStr.CmpNoCase("FALSE"))
    Under = false;
  else
    return seec::util::Maybe<SciIndicatorStyle>();
  
  // Return the complete style.
  return SciIndicatorStyle(wxString(StyleName),
                           Style,
                           wxColour(ForegroundStr),
                           Alpha,
                           OutlineAlpha,
                           Under);
}
