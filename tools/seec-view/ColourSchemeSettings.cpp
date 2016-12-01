//===- tools/seec-trace-view/ColourSchemeSettings.cpp ---------------------===//
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

#include <wx/wx.h>
#include <wx/clrpicker.h>
#include <wx/filename.h>
#include <wx/fontpicker.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/xml/xml.h>

#include "llvm/ADT/STLExtras.h"

#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeFunction.hpp"
#include "seec/wxWidgets/Config.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/wxWidgets/XmlNodeIterator.hpp"

#include <array>

#include "ColourSchemeSettings.hpp"

using namespace seec;

namespace {

Maybe<wxFontInfo, Error> FontInfoFromXML(wxXmlNode const &Node) {
  wxFontInfo TheFontInfo;

  auto const AttrPointSize = Node.GetAttribute("PointSize");
  if (!AttrPointSize.empty()) {
    long Value = 0;
    if (!AttrPointSize.ToLong(&Value) || Value <= 0) {
      return Error(LazyMessageByRef::create("TraceViewer",
                                            {"ColourSchemes",
                                             "FontPointSizeIncorrect"},
                                            std::make_pair("value",
                                                           toUnicodeString(
                                                             AttrPointSize))));
    }
    TheFontInfo = wxFontInfo(Value);
  }

  auto const Family = Node.GetAttribute("Family");
  if (!Family.empty()) {
    if (Family == "DEFAULT")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_DEFAULT);
    else if (Family == "DECORATIVE")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_DECORATIVE);
    else if (Family == "ROMAN")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_ROMAN);
    else if (Family == "SCRIPT")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_SCRIPT);
    else if (Family == "SWISS")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_SWISS);
    else if (Family == "MODERN")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_MODERN);
    else if (Family == "TELETYPE")
      TheFontInfo = TheFontInfo.Family(wxFONTFAMILY_TELETYPE);
  }

  auto const FaceName = Node.GetAttribute("FaceName");
  if (!FaceName.empty()) {
    TheFontInfo = TheFontInfo.FaceName(FaceName);
  }

#define SEEC_BOOL_PROPERTY(NAME)                                               \
  auto const Attr##NAME = Node.GetAttribute(#NAME);                            \
  if (!Attr##NAME.empty()) {                                                   \
    if (Attr##NAME.IsSameAs("true", /* caseSensitive */ false))                \
      TheFontInfo = TheFontInfo.NAME(true);                                    \
    else if (Attr##NAME.IsSameAs("false", /* caseSensitive */ false))          \
      TheFontInfo = TheFontInfo.NAME(false);                                   \
  }

  SEEC_BOOL_PROPERTY(Bold)
  SEEC_BOOL_PROPERTY(Light)
  SEEC_BOOL_PROPERTY(Italic)
  SEEC_BOOL_PROPERTY(Slant)
  SEEC_BOOL_PROPERTY(AntiAliased)
  SEEC_BOOL_PROPERTY(Underlined)
  SEEC_BOOL_PROPERTY(Strikethrough)

#undef SEEC_BOOL_PROPERTY

  return TheFontInfo;
}

char const * const to_string(wxFontFamily const Family)
{
  switch (Family)
  {
    case wxFONTFAMILY_DECORATIVE: return "DECORATIVE";
    case wxFONTFAMILY_ROMAN:      return "ROMAN";
    case wxFONTFAMILY_SCRIPT:     return "SCRIPT";
    case wxFONTFAMILY_SWISS:      return "SWISS";
    case wxFONTFAMILY_MODERN:     return "MODERN";
    case wxFONTFAMILY_TELETYPE:   return "TELETYPE";
    default:                      return "DEFAULT";
  }
}

std::unique_ptr<wxXmlNode> FontToXML(wxFont const &Info)
{
  std::unique_ptr<wxXmlNode> Node =
    llvm::make_unique<wxXmlNode>(wxXML_ELEMENT_NODE, "FontInfo");

  Node->AddAttribute("PointSize", std::to_string(Info.GetPointSize()));

  Node->AddAttribute("Family", to_string(Info.GetFamily()));

  auto const &FaceName = Info.GetFaceName();
  if (!FaceName.empty())
    Node->AddAttribute("FaceName", FaceName);

  switch (Info.GetWeight()) {
    case wxFontWeight::wxFONTWEIGHT_NORMAL:
      break;
    case wxFontWeight::wxFONTWEIGHT_LIGHT:
      Node->AddAttribute("Light", "true");
      break;
    case wxFontWeight::wxFONTWEIGHT_BOLD:
      Node->AddAttribute("Bold", "true");
      break;
    case wxFontWeight::wxFONTWEIGHT_MAX:
      break;
  }

  switch (Info.GetStyle()) {
    case wxFontStyle::wxFONTSTYLE_NORMAL:
      break;
    case wxFontStyle::wxFONTSTYLE_SLANT:
      Node->AddAttribute("Slant", "true");
      break;
    case wxFontStyle::wxFONTSTYLE_ITALIC:
      Node->AddAttribute("Italic", "true");
      break;
    case wxFontStyle::wxFONTSTYLE_MAX:
      break;
  }

  // Node->AddAttribute("AntiAliased", "true");

  if (Info.GetUnderlined())
    Node->AddAttribute("Underlined", "true");

  if (Info.GetStrikethrough())
    Node->AddAttribute("Strikethrough", "true");

  return Node;
}

std::unique_ptr<wxXmlNode> TextStyleToXML(TextStyle const &Style,
                                          wxString const &Name)
{
  auto Node = llvm::make_unique<wxXmlNode>(wxXML_ELEMENT_NODE, Name);

  auto FontInfo = FontToXML(Style.GetFont());
  if (FontInfo)
    Node->AddChild(FontInfo.release());

  auto Foreground = Style.GetForeground().GetAsString();
  Node->AddAttribute("Foreground", Foreground);

  auto Background = Style.GetBackground().GetAsString();
  Node->AddAttribute("Background", Background);

  return Node;
}

Maybe<IndicatorStyle::EKind, Error>
IndicatorStyleKindFromString(wxString const &String)
{
  if (String == "PLAIN")
    return IndicatorStyle::EKind::Plain;
  else if (String == "BOX")
    return IndicatorStyle::EKind::Box;
  else if (String == "STRAIGHTBOX")
    return IndicatorStyle::EKind::StraightBox;
  
  return Error(LazyMessageByRef::create("TraceViewer",
                                        {"ColourSchemes",
                                         "IndicatorKindIncorrect"},
                                        std::make_pair(
                                           "value",
                                           toUnicodeString(String))));
}

std::unique_ptr<wxXmlNode> IndicatorStyleToXML(IndicatorStyle const &Style,
                                               wxString const &Name)
{
  auto Node = llvm::make_unique<wxXmlNode>(wxXML_ELEMENT_NODE, Name);
  
  Node->AddAttribute("Kind", to_string(Style.GetKind()));
  
  auto Foreground = Style.GetForeground().GetAsString();
  Node->AddAttribute("Foreground", Foreground);
  
  Node->AddAttribute("Alpha", std::to_string(Style.GetAlpha()));
  Node->AddAttribute("OutlineAlpha", std::to_string(Style.GetOutlineAlpha()));
  
  return Node;
}

std::unique_ptr<wxXmlNode> ColourSchemeToXml(ColourScheme const &Scheme,
                                             wxString const &Name)
{
  auto Node = llvm::make_unique<wxXmlNode>(wxXML_ELEMENT_NODE, Name);
  auto TextStyles = llvm::make_unique<wxXmlNode>(wxXML_ELEMENT_NODE,
                                                 "TextStyles");
  auto IndicatorStyles = llvm::make_unique<wxXmlNode>(wxXML_ELEMENT_NODE,
                                                      "IndicatorStyles");

  // Create nodes for all TextStyles.
#define SEEC_SERIALIZE_TEXTSTYLE(NAME)                                         \
  if (auto Child = TextStyleToXML(Scheme.get##NAME(), #NAME))                  \
    TextStyles->AddChild(Child.release());

  SEEC_SERIALIZE_TEXTSTYLE(Default)
  SEEC_SERIALIZE_TEXTSTYLE(LineNumber)

  SEEC_SERIALIZE_TEXTSTYLE(RuntimeError)
  SEEC_SERIALIZE_TEXTSTYLE(RuntimeValue)
  SEEC_SERIALIZE_TEXTSTYLE(RuntimeInformation)

  SEEC_SERIALIZE_TEXTSTYLE(Comment)
  SEEC_SERIALIZE_TEXTSTYLE(CommentLine)
  SEEC_SERIALIZE_TEXTSTYLE(Number)
  SEEC_SERIALIZE_TEXTSTYLE(Keyword1)
  SEEC_SERIALIZE_TEXTSTYLE(String)
  SEEC_SERIALIZE_TEXTSTYLE(Character)
  SEEC_SERIALIZE_TEXTSTYLE(Preprocessor)
  SEEC_SERIALIZE_TEXTSTYLE(Operator)
  SEEC_SERIALIZE_TEXTSTYLE(Identifier)
  SEEC_SERIALIZE_TEXTSTYLE(StringEOL)
  SEEC_SERIALIZE_TEXTSTYLE(Keyword2)

#undef SEEC_SERIALIZE_TEXTSTYLE

  // Create nodes for all IndicatorStyles.
#define SEEC_SERIALIZE_INDICATORSTYLE(NAME)                        \
  if (auto Child = IndicatorStyleToXML(Scheme.get##NAME(), #NAME)) \
    IndicatorStyles->AddChild(Child.release());
  
  SEEC_SERIALIZE_INDICATORSTYLE(ActiveCode)
  SEEC_SERIALIZE_INDICATORSTYLE(ErrorCode)
  SEEC_SERIALIZE_INDICATORSTYLE(HighlightCode)
  SEEC_SERIALIZE_INDICATORSTYLE(InteractiveText)
  
#undef SEEC_SERIALIZE_INDICATORSTYLE

  Node->AddChild(TextStyles.release());
  Node->AddChild(IndicatorStyles.release());
  return Node;
}

wxXmlNode const *GetChildNamed(wxXmlNode const &Node, wxString const &Name)
{
  for (auto const &Child : Node)
    if (Child.GetName() == Name)
      return &Child;

  return nullptr;
}

} // anonymous namespace

Maybe<TextStyle, Error> TextStyle::fromXML(wxXmlNode const &Node)
{
  wxXmlNode const *FontInfoNode = GetChildNamed(Node, "FontInfo");
  if (!FontInfoNode) {
    return Error(LazyMessageByRef::create("TraceViewer",
                   {"ColourSchemes", "FontInfoNodeMissing"},
                   std::make_pair("value", toUnicodeString(Node.GetName()))));
  }

  auto MaybeFontInfo = FontInfoFromXML(*FontInfoNode);
  if (MaybeFontInfo.assigned<Error>())
    return MaybeFontInfo.move<Error>();

  auto const ForegroundString = Node.GetAttribute("Foreground", wxEmptyString);

  auto const BackgroundString = Node.GetAttribute("Background", wxEmptyString);

  return TextStyle(wxColour(ForegroundString),
                   wxColour(BackgroundString),
                   MaybeFontInfo.get<wxFontInfo>());
}

Maybe<IndicatorStyle, Error> IndicatorStyle::fromXML(wxXmlNode const &Node)
{
  auto const KindString = Node.GetAttribute("Kind", "PLAIN");
  auto Kind = IndicatorStyleKindFromString(KindString);
  if (Kind.assigned<Error>())
    return Kind.move<Error>();
  
  auto const ForegroundString = Node.GetAttribute("Foreground", wxEmptyString);
  
  auto const AlphaString = Node.GetAttribute("Alpha", "255");
  long Alpha = 0;
  AlphaString.ToLong(&Alpha);
  
  auto const OutlineAlphaString = Node.GetAttribute("OutlineAlpha", "0");
  long OutlineAlpha = 0;
  OutlineAlphaString.ToLong(&OutlineAlpha);
  
  return IndicatorStyle(Kind.get<IndicatorStyle::EKind>(),
                        wxColour(ForegroundString),
                        static_cast<int>(Alpha),
                        static_cast<int>(OutlineAlpha));
}

char const * const to_string(IndicatorStyle::EKind const Kind)
{
  switch (Kind)
  {
    case IndicatorStyle::EKind::Plain:       return "PLAIN";
    case IndicatorStyle::EKind::Box:         return "BOX";
    case IndicatorStyle::EKind::StraightBox: return "STRAIGHTBOX";
    default:                                 return "PLAIN";
  }
}

Maybe<std::shared_ptr<ColourScheme>, Error>
ColourSchemeFromXML(wxXmlDocument const &Doc)
{
  auto const Root = Doc.GetRoot();
  if (Root->GetName() != "ColourScheme")
    return Error(
      LazyMessageByRef::create(
        "TraceViewer",
        {"ColourSchemes", "SchemeInvalidError"}));

  // Create a ColourScheme to read into.
  auto Scheme = std::make_shared<ColourScheme>();

  // Find TextStyles child.
  auto const TextStyles = GetChildNamed(*Root, "TextStyles");
  if (!TextStyles)
    return Error(
      LazyMessageByRef::create(
        "TraceViewer",
        {"ColourSchemes", "TextStylesMissing"}));

  // TODO handle the individual TextStyle nodes.
#define SEEC_READ_TEXTSTYLE(TEXTSTYLE)                                         \
  if (auto const StyleNode = GetChildNamed(*TextStyles, #TEXTSTYLE)) {         \
    auto MaybeStyle = TextStyle::fromXML(*StyleNode);                          \
    if (MaybeStyle.assigned<Error>())                                          \
      return MaybeStyle.move<Error>();                                         \
    Scheme->set##TEXTSTYLE(MaybeStyle.move<TextStyle>());                      \
  }

  SEEC_READ_TEXTSTYLE(Default)
  SEEC_READ_TEXTSTYLE(LineNumber)

  SEEC_READ_TEXTSTYLE(RuntimeError)
  SEEC_READ_TEXTSTYLE(RuntimeValue)
  SEEC_READ_TEXTSTYLE(RuntimeInformation)

  SEEC_READ_TEXTSTYLE(Comment)
  SEEC_READ_TEXTSTYLE(CommentLine)
  SEEC_READ_TEXTSTYLE(Number)
  SEEC_READ_TEXTSTYLE(Keyword1)
  SEEC_READ_TEXTSTYLE(String)
  SEEC_READ_TEXTSTYLE(Character)
  SEEC_READ_TEXTSTYLE(Preprocessor)
  SEEC_READ_TEXTSTYLE(Operator)
  SEEC_READ_TEXTSTYLE(Identifier)
  SEEC_READ_TEXTSTYLE(StringEOL)
  SEEC_READ_TEXTSTYLE(Keyword2)

#undef SEEC_READ_TEXTSTYLE

  // Find IndicatorStyles child.
  auto const IndicatorStyles = GetChildNamed(*Root, "IndicatorStyles");
  if (!IndicatorStyles)
    return Error(
      LazyMessageByRef::create(
        "TraceViewer",
        {"ColourSchemes", "IndicatorStylesMissing"}));
  
#define SEEC_READ_INDICATORSTYLE(INDICATORSTYLE)                               \
  if (auto const StyleNode = GetChildNamed(*IndicatorStyles, #INDICATORSTYLE)){\
    auto MaybeStyle = IndicatorStyle::fromXML(*StyleNode);                     \
    if (MaybeStyle.assigned<Error>())                                          \
      return MaybeStyle.move<Error>();                                         \
    Scheme->set##INDICATORSTYLE(MaybeStyle.move<IndicatorStyle>());            \
  }

  SEEC_READ_INDICATORSTYLE(ActiveCode)
  SEEC_READ_INDICATORSTYLE(ErrorCode)
  SEEC_READ_INDICATORSTYLE(HighlightCode)
  SEEC_READ_INDICATORSTYLE(InteractiveText)
  
#undef SEEC_READ_INDICATORSTYLE

  return Scheme;
}

Maybe<std::shared_ptr<ColourScheme>, Error>
ColourSchemeFromXML(wxString const &Filename)
{
  wxXmlDocument Doc;
  if (!Doc.Load(Filename))
    return Error(
      LazyMessageByRef::create(
        "TraceViewer",
        {"ColourSchemes", "XMLLoadError"},
        std::make_pair("filename", toUnicodeString(Filename))));

  return ColourSchemeFromXML(Doc);
}

/// \brief Emitted when a \c TextStyle is modified.
///
class TextStyleModifiedEvent : public wxEvent
{
public:
  // Make this class known to wxWidgets' class hierarchy.
wxDECLARE_CLASS(TextStyleModifiedEvent);

  /// \brief Constructor.
  ///
  TextStyleModifiedEvent(wxEventType EventType, int WinID)
  : wxEvent(WinID, EventType)
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// \brief Copy constructor.
  ///
  TextStyleModifiedEvent(TextStyleModifiedEvent const &Ev)
    : wxEvent(Ev)
  {
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new TextStyleModifiedEvent(*this);
  }
};

IMPLEMENT_CLASS(TextStyleModifiedEvent, wxEvent)

// Produced when the user changes the text style.
wxDEFINE_EVENT(SEEC_EV_TEXTSTYLE_MODIFIED, TextStyleModifiedEvent);

/// \brief Allow user to edit a \c TextStyle.
///
class TextStyleEditControl : public wxPanel
{
private:
  TextStyle m_Style;

  void raiseTextStyleModifiedEvent()
  {
    TextStyleModifiedEvent Ev { SEEC_EV_TEXTSTYLE_MODIFIED,  GetId() };
    Ev.SetEventObject(this);
    if (auto const Handler = GetEventHandler())
      Handler->AddPendingEvent(Ev);
  }

public:
  TextStyleEditControl(wxWindow *Parent,
                       TextStyle const &WithStyle,
                       wxString const &DisplayName)
  : wxPanel(Parent),
    m_Style(WithStyle)
  {
    auto const TextTable = Resource("TraceViewer")["ColourSchemes"]
                            ["SettingsPanel"]["TextStylePicker"];

    std::unique_ptr<wxBoxSizer> TheSizer (new wxBoxSizer(wxHORIZONTAL));

    auto const DefaultLabel = new wxStaticText(this, wxID_ANY, DisplayName);
    DefaultLabel->SetBackgroundStyle(wxBG_STYLE_COLOUR);

    auto const DefaultFontPicker = new wxFontPickerCtrl(this, wxID_ANY);
    DefaultFontPicker->Bind(wxEVT_FONTPICKER_CHANGED,
                            make_function([=](wxFontPickerEvent &Ev){
                              m_Style.SetFont(Ev.GetFont());
                              raiseTextStyleModifiedEvent();
                              Ev.Skip();
                            }));

    DefaultFontPicker->SetSelectedFont(m_Style.GetFont());
    DefaultFontPicker->SetToolTip(towxString(TextTable["FontPickerToolTip"]));

    auto const FGLabel =
      new wxStaticText(this, wxID_ANY,
                       towxString(TextTable["ForegroundPickerLabel"]));
    FGLabel->SetBackgroundStyle(wxBG_STYLE_COLOUR);

    auto const DefaultFGColourPicker = new wxColourPickerCtrl(this, wxID_ANY);
    DefaultFGColourPicker->Bind(wxEVT_COLOURPICKER_CHANGED,
                                make_function([=](wxColourPickerEvent &Ev){
                                  m_Style.SetForeground(Ev.GetColour());
                                  raiseTextStyleModifiedEvent();
                                  Ev.Skip();
                                }));

    DefaultFGColourPicker->SetColour(m_Style.GetForeground());
    DefaultFGColourPicker->SetToolTip(
      towxString(TextTable["ForegroundPickerToolTip"]));

    auto const BGLabel =
      new wxStaticText(this, wxID_ANY,
                       towxString(TextTable["BackgroundPickerLabel"]));
    BGLabel->SetBackgroundStyle(wxBG_STYLE_COLOUR);

    auto const DefaultBGColourPicker = new wxColourPickerCtrl(this, wxID_ANY);
    DefaultBGColourPicker->Bind(wxEVT_COLOURPICKER_CHANGED,
                                make_function([=](wxColourPickerEvent &Ev){
                                  m_Style.SetBackground(Ev.GetColour());
                                  raiseTextStyleModifiedEvent();
                                  Ev.Skip();
                                }));

    DefaultBGColourPicker->SetColour(m_Style.GetBackground());
    DefaultBGColourPicker->SetToolTip(
      towxString(TextTable["BackgroundPickerToolTip"]));

    TheSizer->Add(DefaultLabel,
                  wxSizerFlags().Proportion(1)
                    .Align(wxALIGN_CENTRE_VERTICAL));

    TheSizer->Add(DefaultFontPicker,
                  wxSizerFlags().Proportion(1)
                    .Align(wxALIGN_CENTRE_VERTICAL));

    TheSizer->AddSpacer(15);
    TheSizer->Add(FGLabel,
                  wxSizerFlags().Align(wxALIGN_CENTRE_VERTICAL));
    TheSizer->Add(DefaultFGColourPicker,
                  wxSizerFlags().Expand());

    TheSizer->AddSpacer(15);
    TheSizer->Add(BGLabel,
                  wxSizerFlags().Align(wxALIGN_CENTRE_VERTICAL));
    TheSizer->Add(DefaultBGColourPicker,
                  wxSizerFlags().Expand());

    SetSizerAndFit(TheSizer.release());
  }

  TextStyle const &getStyle() const { return m_Style; }
};

/// \brief Emitted when an \c IndicatorStyle is modified.
///
class IndicatorStyleModifiedEvent : public wxEvent
{
public:
  // Make this class known to wxWidgets' class hierarchy.
wxDECLARE_CLASS(IndicatorStyleModifiedEvent);

  /// \brief Constructor.
  ///
  IndicatorStyleModifiedEvent(wxEventType EventType, int WinID)
  : wxEvent(WinID, EventType)
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// \brief Copy constructor.
  ///
  IndicatorStyleModifiedEvent(IndicatorStyleModifiedEvent const &Ev)
    : wxEvent(Ev)
  {
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new IndicatorStyleModifiedEvent(*this);
  }
};

IMPLEMENT_CLASS(IndicatorStyleModifiedEvent, wxEvent)

// Produced when the user changes the indicator style.
wxDEFINE_EVENT(SEEC_EV_INDICATORSTYLE_MODIFIED, IndicatorStyleModifiedEvent);

/// \brief Allow user to edit an \c IndicatorStyle.
///
class IndicatorStyleEditControl : public wxPanel
{
private:
  IndicatorStyle m_Style;

  void raiseIndicatorStyleModifiedEvent()
  {
    IndicatorStyleModifiedEvent Ev {SEEC_EV_INDICATORSTYLE_MODIFIED, GetId()};
    Ev.SetEventObject(this);
    if (auto const Handler = GetEventHandler())
      Handler->AddPendingEvent(Ev);
  }

public:
  IndicatorStyleEditControl(wxWindow *Parent,
                            IndicatorStyle const &WithStyle,
                            wxString const &DisplayName)
  : wxPanel(Parent),
    m_Style(WithStyle)
  {
    std::unique_ptr<wxBoxSizer> TheSizer (new wxBoxSizer(wxHORIZONTAL));
    
    auto const ColourSchemesTable = Resource("TraceViewer")["ColourSchemes"];
    auto const IndicKindTable = ColourSchemesTable["IndicatorKindNames"];
    auto const TextTable = ColourSchemesTable["SettingsPanel"]
                            ["IndicatorStylePicker"];

    auto const DefaultLabel = new wxStaticText(this, wxID_ANY, DisplayName);
    DefaultLabel->SetBackgroundStyle(wxBG_STYLE_COLOUR);

    std::array<bool const, 3> KindHasOutlineOpacity = {
      false, // Plain
      false, // Box
      true   // StraightBox
    };

    std::array<wxString const, 3> KindNames = {
      towxString(IndicKindTable[to_string(IndicatorStyle::EKind::Plain)]),
      towxString(IndicKindTable[to_string(IndicatorStyle::EKind::Box)]),
      towxString(IndicKindTable[to_string(IndicatorStyle::EKind::StraightBox)])
    };
    
    auto const DefaultFGColourPicker = new wxColourPickerCtrl(this, wxID_ANY);
    DefaultFGColourPicker->
      SetToolTip(towxString(TextTable["ForegroundPickerToolTip"]));
    DefaultFGColourPicker->Bind(wxEVT_COLOURPICKER_CHANGED,
                                make_function([=](wxColourPickerEvent &Ev){
                                  m_Style.SetForeground(Ev.GetColour());
                                  raiseIndicatorStyleModifiedEvent();
                                  Ev.Skip();
                                }));

    DefaultFGColourPicker->SetColour(m_Style.GetForeground());
    
    auto const AlphaLabel =
      new wxStaticText(this, wxID_ANY,
                       towxString(TextTable["OpacityPickerLabel"]));
    AlphaLabel->SetBackgroundStyle(wxBG_STYLE_COLOUR);
    
    auto const AlphaSpin =
      new wxSpinCtrl(this, wxID_ANY,
                     std::to_string(m_Style.GetAlpha()),
                     wxDefaultPosition,
                     wxDefaultSize,
                     wxSP_ARROW_KEYS,
                     /* min */ 0,
                     /* max */ 255,
                     m_Style.GetAlpha(),
                     /* name */ "blah");
    
    AlphaSpin->SetToolTip(towxString(TextTable["OpacityPickerToolTip"]));
    AlphaSpin->Bind(wxEVT_SPINCTRL,
                    make_function([=](wxSpinEvent &Ev){
                      m_Style.SetAlpha(Ev.GetPosition());
                      raiseIndicatorStyleModifiedEvent();
                      Ev.Skip();
                    }));

    auto const OutlineAlphaLabel =
      new wxStaticText(this, wxID_ANY,
                       towxString(TextTable["OutlineOpacityPickerLabel"]));
    OutlineAlphaLabel->SetBackgroundStyle(wxBG_STYLE_COLOUR);

    auto const OutlineAlphaSpin =
      new wxSpinCtrl(this, wxID_ANY,
                     std::to_string(m_Style.GetOutlineAlpha()),
                     wxDefaultPosition,
                     wxDefaultSize,
                     wxSP_ARROW_KEYS,
                     /* min */ 0,
                     /* max */ 255,
                     m_Style.GetOutlineAlpha(),
                     /* name */ "blah");
    
    OutlineAlphaSpin->Enable(
      KindHasOutlineOpacity[static_cast<int>(m_Style.GetKind())]);
    OutlineAlphaSpin->
      SetToolTip(towxString(TextTable["OutlineOpacityPickerToolTip"]));
    OutlineAlphaSpin->Bind(wxEVT_SPINCTRL,
                            make_function([=](wxSpinEvent &Ev){
                              m_Style.SetOutlineAlpha(Ev.GetPosition());
                              raiseIndicatorStyleModifiedEvent();
                              Ev.Skip();
                            }));
    
    auto const KindChoice = new wxChoice(this, wxID_ANY,
                                         wxDefaultPosition,
                                         wxDefaultSize,
                                         KindNames.size(),
                                         KindNames.data());
    
    KindChoice->SetToolTip(towxString(TextTable["KindPickerToolTip"]));
    KindChoice->SetSelection(static_cast<int>(m_Style.GetKind()));
    KindChoice->Bind(wxEVT_CHOICE,
       make_function([=](wxCommandEvent &Ev){
         auto const Value = Ev.GetInt();
         if (Value < 0 ||
             Value > static_cast<int>(IndicatorStyle::EKind::StraightBox)) {
           wxLogDebug("Invalid indicator kind choice.");
           return;
         }
         
         m_Style.SetKind(static_cast<IndicatorStyle::EKind>(Value));

         OutlineAlphaSpin->Enable(KindHasOutlineOpacity[Value]);

         raiseIndicatorStyleModifiedEvent();
         Ev.Skip();
       }));
    
    TheSizer->Add(DefaultLabel,
                  wxSizerFlags().Proportion(1)
                    .Align(wxALIGN_CENTRE_VERTICAL));

    TheSizer->Add(KindChoice,
                  wxSizerFlags().Expand());
    
    TheSizer->AddSpacer(15);
    TheSizer->Add(DefaultFGColourPicker,
                  wxSizerFlags().Expand());
    
    TheSizer->AddSpacer(15);
    TheSizer->Add(AlphaLabel,
                  wxSizerFlags().Align(wxALIGN_CENTRE_VERTICAL));
    TheSizer->Add(AlphaSpin,
                  wxSizerFlags().Align(wxALIGN_CENTRE_VERTICAL));
    
    TheSizer->AddSpacer(15);
    TheSizer->Add(OutlineAlphaLabel,
                  wxSizerFlags().Align(wxALIGN_CENTRE_VERTICAL));
    TheSizer->Add(OutlineAlphaSpin,
                  wxSizerFlags().Align(wxALIGN_CENTRE_VERTICAL));

    SetSizerAndFit(TheSizer.release());
  }

  IndicatorStyle const &getStyle() const { return m_Style; }
};

ColourScheme::ColourScheme()
: m_Default(           wxColour(101,123,131), wxColour(253,246,227),
                       wxFontInfo(12).Family(wxFONTFAMILY_MODERN)),
  m_LineNumber(        wxColour(147,161,161), wxColour(238,232,213),
                       m_Default.GetFont()),
  m_RuntimeError(      wxColour(220, 50, 47), wxColour(238,232,213),
                       m_Default.GetFont()),
  m_RuntimeValue(      wxColour(133,153,  0), wxColour(238,232,213),
                       m_Default.GetFont()),
  m_RuntimeInformation(wxColour(181,137,  0), wxColour(238,232,213),
                       m_Default.GetFont()),
  m_Comment(           wxColour(147,161,161), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_CommentLine(       wxColour(147,161,161), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Number(            wxColour(203, 75, 22), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Keyword1(          wxColour( 88,110,117), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_String(            wxColour( 38,139,210), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Character(         wxColour( 42,161,152), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Preprocessor(      wxColour(211, 54,130), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Operator(          wxColour( 88,110,117), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Identifier(        wxColour( 88,110,117), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_StringEOL(         wxColour( 38,139,210), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_Keyword2(          wxColour( 88,110,117), wxColour(253,246,227),
                       m_Default.GetFont()),
  m_ActiveCode(IndicatorStyle::EKind::Plain, wxColour(181,137,  0), 100, 0),
  m_ErrorCode(IndicatorStyle::EKind::Box, wxColour(220, 50, 47), 100, 0),
  m_HighlightCode(IndicatorStyle::EKind::Box, wxColour(108,113,196), 100, 0),
  m_InteractiveText(IndicatorStyle::EKind::Plain, wxColour( 38,139,210), 100, 0)
{}

ColourSchemeSettings::ColourSchemeSettings()
: m_Scheme(std::make_shared<ColourScheme>()),
  m_Subject()
{}

void
ColourSchemeSettings
::setColourScheme(std::shared_ptr<ColourScheme> NewScheme)
{
  m_Scheme = std::move(NewScheme);
  m_Subject.notifyObservers(*this);
}

void ColourSchemeSettings::loadUserScheme()
{
  wxFileName ThePath (getUserLocalDataPath());
  ThePath.SetFullName("scheme.xml");

  if (!ThePath.FileExists())
    return;

  auto MaybeScheme = ColourSchemeFromXML(ThePath.GetFullPath());
  if (MaybeScheme.assigned<Error>()) {
    auto const ErrStr = getOrDescribe(MaybeScheme.get<Error>());
    wxMessageDialog Dlg(nullptr,
                        towxString(Resource("TraceViewer")
                                   ["ColourSchemes"]["ReadErrorTitle"]),
                        towxString(ErrStr));
    Dlg.ShowModal();
    return;
  }

  setColourScheme(MaybeScheme.move<std::shared_ptr<ColourScheme>>());
}

void ColourSchemeSettingsWindow::OnColourSchemeUpdated()
{
  m_Settings->setColourScheme(m_Scheme);
}

bool ColourSchemeSettingsWindow::SaveValuesImpl()
{
  auto SchemeNode = ColourSchemeToXml(*m_Scheme, "ColourScheme");
  if (!SchemeNode) {
    return false;
  }

  // Filename to save the configuration in.
  wxFileName ThePath (getUserLocalDataPath());
  ThePath.SetFullName("scheme.xml");

  wxXmlDocument SchemeDocument;
  SchemeDocument.SetRoot(SchemeNode.release());
  auto const Result = SchemeDocument.Save(ThePath.GetFullPath());

  if (!Result) {
    auto Res = Resource("TraceViewer")["ColourSchemes"]["SettingsPanel"];
    wxMessageDialog Dlg(this,
                        towxString(Res["SaveErrorTitle"]),
                        towxString(Res["SaveErrorMessage"]));
    Dlg.ShowModal();
  }

  return Result;
}

void ColourSchemeSettingsWindow::CancelChangesImpl()
{
  m_Settings->setColourScheme(m_PreviousScheme);
}

wxString ColourSchemeSettingsWindow::GetDisplayNameImpl()
{
  return towxString(Resource("TraceViewer")
                    ["ColourSchemes"]["SettingsPanel"]["Title"]);
}

ColourSchemeSettingsWindow::ColourSchemeSettingsWindow()
: m_Settings(nullptr),
  m_PreviousScheme(),
  m_Scheme()
{}

ColourSchemeSettingsWindow
::ColourSchemeSettingsWindow(wxWindow *Parent,
                             ColourSchemeSettings &ForSettings)
: m_Settings(nullptr),
  m_PreviousScheme(),
  m_Scheme()
{
  Create(Parent, ForSettings);
}

ColourSchemeSettingsWindow::~ColourSchemeSettingsWindow() = default;

bool ColourSchemeSettingsWindow::Create(wxWindow *Parent,
                                        ColourSchemeSettings &ForSettings)
{
  if (!wxWindow::Create(Parent, wxID_ANY))
    return false;

  m_PreviousScheme = ForSettings.getColourScheme();
  m_Scheme = std::make_shared<ColourScheme>(*m_PreviousScheme);
  m_Settings = &ForSettings;

  auto const ColourSchemesTable = Resource("TraceViewer")["ColourSchemes"];
  auto const TextStyleNameTable = ColourSchemesTable["TextStyleNames"];
  auto const IndicatorStyleNameTable =
    ColourSchemesTable["IndicatorStyleNames"];

  auto const ScrolledControlPanel = new wxScrolled<wxPanel>(this);

  auto const TextStyleListSizer = new wxBoxSizer(wxVERTICAL);

#define SEEC_ADD_EDIT_CONTROL(STYLE_NAME)                                      \
  auto const STYLE_NAME##Control =                                             \
    new TextStyleEditControl(ScrolledControlPanel, m_Scheme->get##STYLE_NAME(),\
                             towxString(TextStyleNameTable[#STYLE_NAME]));     \
  STYLE_NAME##Control->Bind(SEEC_EV_TEXTSTYLE_MODIFIED,                        \
    make_function([=](TextStyleModifiedEvent &Ev){                             \
      m_Scheme->set##STYLE_NAME(STYLE_NAME##Control->getStyle());              \
      OnColourSchemeUpdated();                                                 \
      Ev.Skip(); }));                                                          \
  TextStyleListSizer->Add(STYLE_NAME##Control,                                 \
                          wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 5));\
  TextStyleListSizer->AddSpacer(3);

  SEEC_ADD_EDIT_CONTROL(Default)
  SEEC_ADD_EDIT_CONTROL(LineNumber)

  SEEC_ADD_EDIT_CONTROL(RuntimeError)
  SEEC_ADD_EDIT_CONTROL(RuntimeValue)
  SEEC_ADD_EDIT_CONTROL(RuntimeInformation)

  SEEC_ADD_EDIT_CONTROL(Comment)
  SEEC_ADD_EDIT_CONTROL(CommentLine)
  SEEC_ADD_EDIT_CONTROL(Number)
  SEEC_ADD_EDIT_CONTROL(Keyword1)
  SEEC_ADD_EDIT_CONTROL(String)
  SEEC_ADD_EDIT_CONTROL(Character)
  SEEC_ADD_EDIT_CONTROL(Preprocessor)
  SEEC_ADD_EDIT_CONTROL(Operator)
  SEEC_ADD_EDIT_CONTROL(Identifier)
  SEEC_ADD_EDIT_CONTROL(StringEOL)
  SEEC_ADD_EDIT_CONTROL(Keyword2)

#undef SEEC_ADD_EDIT_CONTROL

  // Now add IndicatorStyle editing controls.
#define SEEC_ADD_EDIT_CONTROL(INDIC_NAME)                                      \
  auto const INDIC_NAME##Control =                                             \
    new IndicatorStyleEditControl(ScrolledControlPanel,                        \
                                  m_Scheme->get##INDIC_NAME(),                 \
           towxString(IndicatorStyleNameTable[#INDIC_NAME]));                  \
  INDIC_NAME##Control->Bind(SEEC_EV_INDICATORSTYLE_MODIFIED,                   \
    make_function([=](IndicatorStyleModifiedEvent &Ev){                        \
      m_Scheme->set##INDIC_NAME(INDIC_NAME##Control->getStyle());              \
      OnColourSchemeUpdated();                                                 \
      Ev.Skip(); }));                                                          \
  TextStyleListSizer->Add(INDIC_NAME##Control,                                 \
                          wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 5));\
  TextStyleListSizer->AddSpacer(3);

  SEEC_ADD_EDIT_CONTROL(ActiveCode)
  SEEC_ADD_EDIT_CONTROL(ErrorCode)
  SEEC_ADD_EDIT_CONTROL(HighlightCode)
  SEEC_ADD_EDIT_CONTROL(InteractiveText)
  
#undef SEEC_ADD_EDIT_CONTROL

  ScrolledControlPanel->SetScrollRate(5,5);
  ScrolledControlPanel->SetSizer(TextStyleListSizer);

  auto const ParentSizer = new wxBoxSizer(wxVERTICAL);
  ParentSizer->Add(ScrolledControlPanel,
                   wxSizerFlags().Proportion(1).Expand().Border(wxALL, 5));
  SetSizerAndFit(ParentSizer);

  return true;
}
