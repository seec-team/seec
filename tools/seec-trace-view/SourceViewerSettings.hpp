//===- tools/seec-trace-view/SourceViewerSettings.hpp ---------------------===//
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

#ifndef SEEC_TRACE_VIEW_SOURCEVIEWERSETTINGS_HPP
#define SEEC_TRACE_VIEW_SOURCEVIEWERSETTINGS_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/stc/stc.h>


class ColourScheme;


//===----------------------------------------------------------------------===//
// SciCommonType
//===----------------------------------------------------------------------===//

/// \brief Specifies the common style types that we use with Scintilla. 
///
enum class SciCommonType : int {
#define SEEC_SCI_COMMON_TYPE(TYPE, ID) \
  TYPE = ID,
#include "SourceViewerSettingsTypes.def"
};

/// \brief Get the name of a SciCommonType.
///
/// \param Type the type to get the name of.
/// \return a pointer to a statically-allocated C string containing the name of
///         Type.
char const *getSciTypeName(SciCommonType Type);

/// \brief Get the SciCommonType with the given name (if any).
///
/// \param Name the name to search for.
/// \return a seec::Maybe<SciCommonType> which contains the SciCommonType
///         with name equal to Name, or is unassigned if no such SciCommonType
///         exists.
seec::Maybe<SciCommonType> getSciCommonTypeFromName(llvm::StringRef Name);


//===----------------------------------------------------------------------===//
// SciLexerType
//===----------------------------------------------------------------------===//

/// \brief Specifies the style types that we use with Scintilla lexers. 
///
enum class SciLexerType : int {
#define SEEC_SCI_TYPE(TYPE, ID) \
  TYPE = ID,
#include "SourceViewerSettingsTypes.def"
};

/// \brief Get the name of a SciLexerType.
///
/// \param Type the type to get the name of.
/// \return a pointer to a statically-allocated C string containing the name of
///         Type.
char const *getSciTypeName(SciLexerType Type);

/// \brief Get the SciLexerType with the given name (if any).
///
/// \param Name the name to search for.
/// \return a seec::Maybe<SciType> which contains the SciType with name
///         equal to Name, or is unassigned if no such SciType exists.
seec::Maybe<SciLexerType> getSciLexerTypeFromName(llvm::StringRef Name);


//===----------------------------------------------------------------------===//
// SciIndicatorStyle
//===----------------------------------------------------------------------===//

/// \brief Holds the details of a particular indicator style
///
struct SciIndicatorStyle {
  /// \brief The name of this indicator style.
  wxString const Name;
  
  /// \brief The style value for SCI_INDICSETSTYLE().
  int const Style;
  
  /// \brief The foreground colour for this style.
  wxColour const Foreground;
  
  /// \brief The alpha transparency for drawing fill colours.
  int const Alpha;
  
  /// \brief The alpha transparency for drawing outline colours.
  int const OutlineAlpha;
  
  /// \brief Set whether to draw under text.
  bool const Under;
  
  /// \brief Create a new SciIndicatorStyle object.
  ///
  SciIndicatorStyle(wxString const &TheName,
                    int TheStyle,
                    wxColour const &TheForeground,
                    int TheAlpha,
                    int TheOutlineAlpha,
                    bool TheUnder)
  : Name(TheName),
    Style(TheStyle),
    Foreground(TheForeground),
    Alpha(TheAlpha),
    OutlineAlpha(TheOutlineAlpha),
    Under(TheUnder)
  {}
};


//===----------------------------------------------------------------------===//
// SciIndicatorType
//===----------------------------------------------------------------------===//

/// \brief Specified the indicator types that we use.
///
enum class SciIndicatorType : int {
#define SEEC_SCI_INDICATOR(TYPE) \
  TYPE,
#include "SourceViewerSettingsTypes.def"
};

/// \brief Get the name of a SciIndicatorType.
///
char const *getSciIndicatorTypeName(SciIndicatorType Type);

/// \brief Get the SciIndicatorType with the given name (if any).
///
seec::Maybe<SciIndicatorType>
getSciIndicatorTypeFromName(llvm::StringRef Name);

/// \brief Get an array containing all valid SciIndicatorType values.
///
llvm::ArrayRef<SciIndicatorType> getAllSciIndicatorTypes();

/// \brief Get the default style settings for a given SciIndicatorType.
///
seec::Maybe<SciIndicatorStyle>
getDefaultIndicatorStyle(SciIndicatorType Type);

/// \brief Setup default style settings for all indicator types.
///
void setupAllSciIndicatorTypes(wxStyledTextCtrl &Text);


//===----------------------------------------------------------------------===//
// SciMargin
//===----------------------------------------------------------------------===//

enum class SciMargin : int {
  LineNumber = 1
};


//===----------------------------------------------------------------------===//
// ColourScheme support
//===----------------------------------------------------------------------===//

void setupStylesFromColourScheme(wxStyledTextCtrl &Text,
                                 ColourScheme const &Scheme);


#endif // SEEC_TRACE_VIEW_SOURCEVIEWERSETTINGS_HPP
