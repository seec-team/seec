//===- SourceViewerSettings.hpp -------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_SOURCEVIEWERSETTINGS_HPP
#define SEEC_TRACE_VIEW_SOURCEVIEWERSETTINGS_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/stc/stc.h>
#include "seec/wxWidgets/CleanPreprocessor.h"


//===----------------------------------------------------------------------===//
// SciStyle
//===----------------------------------------------------------------------===//

/// \brief Holds the details of a particular style.
///
struct SciStyle {
  /// \brief The name of this style.
  wxString const Name;
  
  /// \brief The foreground colour for this style (i.e. text colour).
  wxColour const Foreground;
  
  /// \brief The background colour for this style.
  wxColour const Background;
  
  /// \brief The font for this style.
  wxFont const Font;
  
  /// \brief Sets whether the font is mixed case, or forces uppercase or 
  ///        lowercase.
  int const CaseForce;
  
  /// \brief Constructor
  SciStyle(wxString const &Name,
           wxColour const &Foreground,
           wxColour const &Background,
           wxFont const &Font,
           int const CaseForce)
  : Name(Name),
    Foreground(Foreground),
    Background(Background),
    Font(Font),
    CaseForce(CaseForce)
  {}
  
  /// \brief Copy-constructor.
  SciStyle(SciStyle const &) = default;
};


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
/// \return a seec::util::Maybe<SciCommonType> which contains the SciCommonType
///         with name equal to Name, or is unassigned if no such SciCommonType
///         exists.
seec::util::Maybe<SciCommonType> getSciCommonTypeFromName(llvm::StringRef Name);

/// \brief Get an array containing all valid SciCommonType values.
llvm::ArrayRef<SciCommonType> getAllSciCommonTypes();

/// \brief Get the default style settings for a given SciCommonType.
///
seec::util::Maybe<SciStyle> getDefaultStyle(SciCommonType Type);



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
/// \return a seec::util::Maybe<SciType> which contains the SciType with name
///         equal to Name, or is unassigned if no such SciType exists.
seec::util::Maybe<SciLexerType> getSciLexerTypeFromName(llvm::StringRef Name);

/// \brief Get an array containing all valid SciType values.
llvm::ArrayRef<SciLexerType> getAllSciLexerTypes();

/// \brief Get the default style settings for a given SciType.
///
seec::util::Maybe<SciStyle> getDefaultStyle(SciLexerType Type);


//===----------------------------------------------------------------------===//
// SciMargin
//===----------------------------------------------------------------------===//

enum class SciMargin : int {
  LineNumber = 1
};


#endif // SEEC_TRACE_VIEW_SOURCEVIEWERSETTINGS_HPP
