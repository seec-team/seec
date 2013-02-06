//===- tools/seec-trace-view/HighlightEvent.hpp ---------------------------===//
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

#ifndef SEEC_TRACE_VIEW_HIGHLIGHTEVENT_HPP
#define SEEC_TRACE_VIEW_HIGHLIGHTEVENT_HPP

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <cassert>


// Forward declarations.
namespace clang {
  class Decl;
  class Stmt;
}


/// \brief Event indicating that a particular item should be highlighted.
///
class HighlightEvent : public wxEvent
{
public:
  /// \brief Types of items that can be highlighted.
  ///
  enum class ItemType {
    Decl,
    Stmt
  };
  
private:
  /// The type of item to be highlighted.
  ItemType Type;
  
  /// The item to be highlighted.
  union {
    ::clang::Decl const *Decl;
    
    ::clang::Stmt const *Stmt;
  } Item;

public:
  // Make this class known to wxWidgets' class hierarchy.
  wxDECLARE_CLASS(HighlightEvent);

  /// Construct for Decl.
  HighlightEvent(wxEventType EventType,
                 int WinID,
                 ::clang::Decl const *Decl)
  : wxEvent(WinID, EventType),
    Type(ItemType::Decl)
  {
    Item.Decl = Decl;
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// Construct for Stmt.
  HighlightEvent(wxEventType EventType,
                 int WinID,
                 ::clang::Stmt const *Stmt)
  : wxEvent(WinID, EventType),
    Type(ItemType::Stmt)
  {
    Item.Stmt = Stmt;
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }
  
  /// Copy constructor.
  HighlightEvent(HighlightEvent const &Ev)
  : wxEvent(Ev),
    Type(Ev.Type)
  {
    switch (Type) {
      case ItemType::Decl:
        Item.Decl = Ev.Item.Decl;
        break;
      case ItemType::Stmt:
        Item.Stmt = Ev.Item.Stmt;
        break;
    }
    
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// wxEvent::Clone().
  virtual wxEvent *Clone() const;

  /// \name Accessors
  /// @{
  
  ItemType getType() const { return Type; }
  
  ::clang::Decl const *getDecl() const {
    assert(Type == ItemType::Decl);
    return Item.Decl;
  }
  
  ::clang::Stmt const *getStmt() const {
    assert(Type == ItemType::Stmt);
    return Item.Stmt;
  }

  /// @}
};

// Produced when an item should be highlighted.
wxDECLARE_EVENT(SEEC_EV_HIGHLIGHT_ON, HighlightEvent);

// Produced when an item's highlight should be cleared.
wxDECLARE_EVENT(SEEC_EV_HIGHLIGHT_OFF, HighlightEvent);

/// Used inside an event table to catch SEEC_EV_HIGHLIGHT_DECL.
#define SEEC_EVT_HIGHLIGHT_ON(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_HIGHLIGHT_DECL, id, (&func))

/// Used inside an event table to catch SEEC_EV_HIGHLIGHT_STMT.
#define SEEC_EVT_HIGHLIGHT_OFF(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_HIGHLIGHT_STMT, id, (&func))


#endif // SEEC_TRACE_VIEW_HIGHLIGHTEVENT_HPP
