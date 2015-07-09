//===- tools/seec-trace-view/SourceViewerSettings.cpp ---------------------===//
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
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/log.h>
#include <wx/tokenzr.h>

#include "ColourSchemeSettings.hpp"
#include "LocaleSettings.hpp"
#include "SourceViewerSettings.hpp"


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

seec::Maybe<SciCommonType> getSciCommonTypeFromName(llvm::StringRef Name){
#define SEEC_SCI_COMMON_TYPE(TYPE, ID) \
  if (Name.equals(#TYPE))   \
    return SciCommonType::TYPE;
#include "SourceViewerSettingsTypes.def"

  return seec::Maybe<SciCommonType>();
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

seec::Maybe<SciLexerType> getSciLexerTypeFromName(llvm::StringRef Name) {
#define SEEC_SCI_TYPE(TYPE, ID) \
  if (Name.equals(#TYPE))   \
    return SciLexerType::TYPE;
#include "SourceViewerSettingsTypes.def"

  return seec::Maybe<SciLexerType>();
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

seec::Maybe<SciIndicatorType>
getSciIndicatorTypeFromName(llvm::StringRef Name) {
#define SEEC_SCI_INDICATOR(TYPE) \
  if (Name.equals(#TYPE))   \
    return SciIndicatorType::TYPE;
#include "SourceViewerSettingsTypes.def"

  return seec::Maybe<SciIndicatorType>();
}

llvm::ArrayRef<SciIndicatorType> getAllSciIndicatorTypes() {
  static SciIndicatorType Types[] = {
#define SEEC_SCI_INDICATOR(TYPE) \
    SciIndicatorType::TYPE,
#include "SourceViewerSettingsTypes.def"
  };
  
  return llvm::ArrayRef<SciIndicatorType>(Types);
}

seec::Maybe<SciIndicatorStyle>
getDefaultIndicatorStyle(SciIndicatorType Type) {
  // First get the name of this indicator style type.
  auto StyleName = getSciIndicatorTypeName(Type);
  if (!StyleName)
    return seec::Maybe<SciIndicatorStyle>();
  
  // Find the default setting for this indicator style in our ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto Table = seec::getResource("TraceViewer",
                                 getLocale(),
                                 Status,
                                 "ScintillaIndicatorStyles",
                                 StyleName);
  if (U_FAILURE(Status))
    return seec::Maybe<SciIndicatorStyle>();
  
  // Get the individual values from the default setting table.
  auto Name = seec::getwxStringExOrEmpty(Table, "Name");
  auto StyleStr = seec::getwxStringExOrEmpty(Table, "Style");
  auto ForegroundStr = seec::getwxStringExOrEmpty(Table, "Foreground");
  auto Alpha = seec::getIntEx(Table, "Alpha", Status);
  auto OutlineAlpha = seec::getIntEx(Table, "OutlineAlpha", Status);
  auto UnderStr = seec::getwxStringExOrEmpty(Table, "Under");
  
  if (U_FAILURE(Status))
    return seec::Maybe<SciIndicatorStyle>();
  
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
    return seec::Maybe<SciIndicatorStyle>();
  
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
    return seec::Maybe<SciIndicatorStyle>();
  
  // Return the complete style.
  return SciIndicatorStyle(wxString(StyleName),
                           Style,
                           wxColour(ForegroundStr),
                           Alpha,
                           OutlineAlpha,
                           Under);
}

void setupAllSciIndicatorTypes(wxStyledTextCtrl &Text) {
  for (auto const Type : getAllSciIndicatorTypes()) {
    auto const MaybeStyle = getDefaultIndicatorStyle(Type);
    
    if (!MaybeStyle.assigned()) {
      wxLogDebug("Couldn't get default style for indicator %s",
                 getSciIndicatorTypeName(Type));
      
      continue;
    }
    
    auto const Indicator = static_cast<int>(Type);
    auto const &IndicatorStyle = MaybeStyle.get<0>();
    
    Text.IndicatorSetStyle(Indicator, IndicatorStyle.Style);
    Text.IndicatorSetForeground(Indicator, IndicatorStyle.Foreground);
    Text.IndicatorSetAlpha(Indicator, IndicatorStyle.Alpha);
    Text.IndicatorSetOutlineAlpha(Indicator, IndicatorStyle.OutlineAlpha);
    Text.IndicatorSetUnder(Indicator, IndicatorStyle.Under);
  }
}


//===----------------------------------------------------------------------===//
// ColourScheme support
//===----------------------------------------------------------------------===//

namespace {

/// \brief Setup a Scintilla style from a \c TextStyle.
///
void setSTCStyle(wxStyledTextCtrl &Text,
                 int const StyleNum,
                 TextStyle const &Style)
{
  Text.StyleSetForeground(StyleNum, Style.GetForeground());
  Text.StyleSetBackground(StyleNum, Style.GetBackground());

  // StyleSetFont requires a non-const lvalue.
  auto Font = Style.GetFont();
  Text.StyleSetFont(StyleNum, Font);
}

/// \brief Setup a SciCommonType style from a \c TextStyle.
///
void setSTCStyle(wxStyledTextCtrl &Text,
                 SciCommonType const Type,
                 TextStyle const &Style)
{
  setSTCStyle(Text, static_cast<int>(Type), Style);
}

/// \brief Setup a SciLexerType style from a \c TextStyle.
///
void setSTCStyle(wxStyledTextCtrl &Text,
                 SciLexerType const Type,
                 TextStyle const &Style)
{
  setSTCStyle(Text, static_cast<int>(Type), Style);
}

} // anonymous namespace

void setupStylesFromColourScheme(wxStyledTextCtrl &Text,
                                 ColourScheme const &Scheme)
{
  // Setup the common styles.
  setSTCStyle(Text, SciCommonType::Default,    Scheme.getDefault());
  setSTCStyle(Text, SciCommonType::LineNumber, Scheme.getLineNumber());

  // Setup the styles for the C/C++ lexer.
  setSTCStyle(Text, SciLexerType::Default,      Scheme.getDefault());
  setSTCStyle(Text, SciLexerType::Comment,      Scheme.getComment());
  setSTCStyle(Text, SciLexerType::CommentLine,  Scheme.getCommentLine());
  setSTCStyle(Text, SciLexerType::Number,       Scheme.getNumber());
  setSTCStyle(Text, SciLexerType::Keyword1,     Scheme.getKeyword1());
  setSTCStyle(Text, SciLexerType::String,       Scheme.getString());
  setSTCStyle(Text, SciLexerType::Character,    Scheme.getCharacter());
  setSTCStyle(Text, SciLexerType::Preprocessor, Scheme.getPreprocessor());
  setSTCStyle(Text, SciLexerType::Operator,     Scheme.getOperator());
  setSTCStyle(Text, SciLexerType::Identifier,   Scheme.getIdentifier());
  setSTCStyle(Text, SciLexerType::StringEOL,    Scheme.getStringEOL());
  setSTCStyle(Text, SciLexerType::Keyword2,     Scheme.getKeyword2());

  // Setup the SeeC-specific styles.
  setSTCStyle(Text, SciLexerType::SeeCRuntimeError, Scheme.getRuntimeError());
  setSTCStyle(Text, SciLexerType::SeeCRuntimeValue, Scheme.getRuntimeValue());
  setSTCStyle(Text, SciLexerType::SeeCRuntimeInformation,
              Scheme.getRuntimeInformation());

  // Setup the style settings for our indicators.
  setupAllSciIndicatorTypes(Text);
}
