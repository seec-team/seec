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
