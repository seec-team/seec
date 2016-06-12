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

int IndicatorKindToSTCIndicatorStyle(IndicatorStyle::EKind const Kind)
{
  switch (Kind)
  {
    case IndicatorStyle::EKind::Plain:       return wxSTC_INDIC_PLAIN;
    case IndicatorStyle::EKind::Box:         return wxSTC_INDIC_BOX;
    case IndicatorStyle::EKind::StraightBox: return wxSTC_INDIC_STRAIGHTBOX;
    default:                                 return wxSTC_INDIC_PLAIN;
  }
}

/// \brief Setup a Scintilla indicator from an \c IndicatorStyle.
///
void setSTCIndicator(wxStyledTextCtrl &Text,
                     SciIndicatorType const Type,
                     IndicatorStyle const &Style)
{
  auto const Indicator = static_cast<int>(Type);
  
  Text.IndicatorSetStyle(Indicator,
                         IndicatorKindToSTCIndicatorStyle(Style.GetKind()));
  Text.IndicatorSetForeground(Indicator, Style.GetForeground());
  Text.IndicatorSetAlpha(Indicator, Style.GetAlpha());
  Text.IndicatorSetOutlineAlpha(Indicator, Style.GetOutlineAlpha());
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
  setSTCIndicator(Text, SciIndicatorType::CodeActive, Scheme.getActiveCode());
  setSTCIndicator(Text, SciIndicatorType::CodeError, Scheme.getErrorCode());
  setSTCIndicator(Text, SciIndicatorType::CodeHighlight,
                  Scheme.getHighlightCode());
  setSTCIndicator(Text, SciIndicatorType::TextInteractive,
                  Scheme.getInteractiveText());
}
