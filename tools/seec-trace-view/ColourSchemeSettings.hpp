//===- tools/seec-trace-view/ColourSchemeSettings.hpp ---------------------===//
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

#ifndef SEEC_TRACE_VIEW_COLOURSCHEMESETTINGS_HPP
#define SEEC_TRACE_VIEW_COLOURSCHEMESETTINGS_HPP

#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "Preferences.hpp"

#include <memory>

class wxWindow;
class wxXmlNode;

/// \brief Defines a text style.
///
class TextStyle
{
private:
  wxColour m_Foreground;

  wxColour m_Background;

  wxFont m_Font;

public:
  TextStyle()
  : m_Foreground(*wxBLACK),
    m_Background(*wxWHITE),
    m_Font(wxFontInfo(12).Family(wxFontFamily::wxFONTFAMILY_MODERN))
  {}

  TextStyle(wxColour Foreground,
            wxColour Background,
            wxFont Font)
  : m_Foreground(Foreground),
    m_Background(Background),
    m_Font(Font)
  {}

  void SetForeground(wxColour Foreground) {
    m_Foreground = Foreground;
  }

  wxColour GetForeground() const { return m_Foreground; }

  void SetBackground(wxColour Background) {
    m_Background = Background;
  }

  wxColour GetBackground() const { return m_Background; }

  void SetFont(wxFont Font) {
    m_Font = Font;
  }

  wxFont GetFont() const { return m_Font; }

  static seec::Maybe<TextStyle, seec::Error> fromXML(wxXmlNode const &Node);
};

/// \brief Defines an indicator style.
///
class IndicatorStyle
{
public:
  enum class EKind {
    Plain,
    Box,
    StraightBox
  };
  
private:
  EKind m_Kind;
  
  wxColour m_Foreground;
  
  int m_Alpha;
  
  int m_OutlineAlpha;

public:
  IndicatorStyle()
  : m_Kind(EKind::Plain),
    m_Foreground(*wxBLACK),
    m_Alpha(255),
    m_OutlineAlpha(0)
  {}

  IndicatorStyle(EKind Kind,
                 wxColour Foreground,
                 int Alpha,
                 int OutlineAlpha)
  : m_Kind(Kind),
    m_Foreground(Foreground),
    m_Alpha(Alpha),
    m_OutlineAlpha(OutlineAlpha)
  {}
  
  void SetKind(EKind const Kind) {
    m_Kind = Kind;
  }
  
  EKind GetKind() const { return m_Kind; }

  void SetForeground(wxColour Foreground) {
    m_Foreground = Foreground;
  }

  wxColour GetForeground() const { return m_Foreground; }

  void SetAlpha(int const Alpha) {
    m_Alpha = std::max(std::min(Alpha, 255), 0);
  }
  
  int GetAlpha() const { return m_Alpha; }

  void SetOutlineAlpha(int const OutlineAlpha) {
    m_OutlineAlpha = std::max(std::min(OutlineAlpha, 255), 0);
  }
  
  int GetOutlineAlpha() const { return m_OutlineAlpha; }

  static seec::Maybe<IndicatorStyle, seec::Error> fromXML(wxXmlNode const &Node);
};

char const * const to_string(IndicatorStyle::EKind const Kind);


/// \brief Defines a complete colour scheme.
///
class ColourScheme
{
private:
  TextStyle m_Default;
  TextStyle m_LineNumber;;

  TextStyle m_RuntimeError;
  TextStyle m_RuntimeValue;
  TextStyle m_RuntimeInformation;

  TextStyle m_Comment;
  TextStyle m_CommentLine;
  TextStyle m_Number;
  TextStyle m_Keyword1;
  TextStyle m_String;
  TextStyle m_Character;
  TextStyle m_Preprocessor;
  TextStyle m_Operator;
  TextStyle m_Identifier;
  TextStyle m_StringEOL;
  TextStyle m_Keyword2;

  IndicatorStyle m_ActiveCode;
  IndicatorStyle m_ErrorCode;
  IndicatorStyle m_HighlightCode;
  IndicatorStyle m_InteractiveText;
  
public:
  ColourScheme();

  void setDefault(TextStyle const &Value) { m_Default = Value; }
  void setLineNumber(TextStyle const &Value) { m_LineNumber = Value; }

  void setRuntimeError(TextStyle const &Value) { m_RuntimeError = Value; }
  void setRuntimeValue(TextStyle const &Value) { m_RuntimeValue = Value; }
  void setRuntimeInformation(TextStyle const &Value) {
    m_RuntimeInformation = Value; }

  void setComment(TextStyle const &Value) { m_Comment = Value; }
  void setCommentLine(TextStyle const &Value) { m_CommentLine = Value; }
  void setNumber(TextStyle const &Value) { m_Number = Value; }
  void setKeyword1(TextStyle const &Value) { m_Keyword1 = Value; }
  void setString(TextStyle const &Value) { m_String = Value; }
  void setCharacter(TextStyle const &Value) { m_Character = Value; }
  void setPreprocessor(TextStyle const &Value) { m_Preprocessor = Value; }
  void setOperator(TextStyle const &Value) { m_Operator = Value; }
  void setIdentifier(TextStyle const &Value) { m_Identifier = Value; }
  void setStringEOL(TextStyle const &Value) { m_StringEOL = Value; }
  void setKeyword2(TextStyle const &Value) { m_Keyword2 = Value; }

  void setActiveCode(IndicatorStyle const &Value) { m_ActiveCode = Value; }
  void setErrorCode(IndicatorStyle const &Value) { m_ErrorCode = Value; }
  void setHighlightCode(IndicatorStyle const &Value) {
    m_HighlightCode = Value; }
  void setInteractiveText(IndicatorStyle const &Value) {
    m_InteractiveText = Value; }
  
  TextStyle const &getDefault() const { return m_Default; }
  TextStyle const &getLineNumber() const { return m_LineNumber; }

  TextStyle const &getRuntimeError() const { return m_RuntimeError; }
  TextStyle const &getRuntimeValue() const { return m_RuntimeValue; }
  TextStyle const &getRuntimeInformation() const {return m_RuntimeInformation;}

  TextStyle const &getComment() const { return m_Comment; }
  TextStyle const &getCommentLine() const { return m_CommentLine; }
  TextStyle const &getNumber() const { return m_Number; }
  TextStyle const &getKeyword1() const { return m_Keyword1; }
  TextStyle const &getString() const { return m_String; }
  TextStyle const &getCharacter() const { return m_Character; }
  TextStyle const &getPreprocessor() const { return m_Preprocessor; }
  TextStyle const &getOperator() const { return m_Operator; }
  TextStyle const &getIdentifier() const { return m_Identifier; }
  TextStyle const &getStringEOL() const { return m_StringEOL; }
  TextStyle const &getKeyword2() const { return m_Keyword2; }
  
  IndicatorStyle const &getActiveCode() const { return m_ActiveCode; }
  IndicatorStyle const &getErrorCode() const { return m_ErrorCode; }
  IndicatorStyle const &getHighlightCode() const { return m_HighlightCode; }
  IndicatorStyle const &getInteractiveText() const { return m_InteractiveText; }
};

/// \brief Holds the application's colour scheme settings.
///
class ColourSchemeSettings final
{
private:
  std::shared_ptr<ColourScheme> m_Scheme;

  std::vector<std::function<void (ColourSchemeSettings const &)>> m_Listeners;

public:
  ColourSchemeSettings();

  void addListener(std::function<void (ColourSchemeSettings const &)>);

  std::shared_ptr<ColourScheme> const &getColourScheme() const {
    return m_Scheme;
  }

  void setColourScheme(std::shared_ptr<ColourScheme> NewScheme);

  void loadUserScheme();
};

/// \brief Allows the user to configure colour schemes.
///
class ColourSchemeSettingsWindow final : public PreferenceWindow
{
private:
  ColourSchemeSettings *m_Settings;

  std::shared_ptr<ColourScheme> m_PreviousScheme;

  std::shared_ptr<ColourScheme> m_Scheme;

  void OnColourSchemeUpdated();

protected:
  /// \brief Save edited values back to the user's config file.
  ///
  virtual bool SaveValuesImpl() override;

  /// \brief Restore the original \c ColourScheme.
  ///
  virtual void CancelChangesImpl() override;

  /// \brief Get a string to describe this window.
  ///
  virtual wxString GetDisplayNameImpl() override;

public:
  /// \brief Constructor (without creation).
  ///
  ColourSchemeSettingsWindow();

  /// \brief Constructor (with creation).
  ///
  ColourSchemeSettingsWindow(wxWindow *Parent,
                             ColourSchemeSettings &ForSettings);

  /// \brief Destructor.
  ///
  virtual ~ColourSchemeSettingsWindow() override;

  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent,
              ColourSchemeSettings &ForSettings);
};

#endif // SEEC_TRACE_VIEW_COLOURSCHEMESETTINGS_HPP
