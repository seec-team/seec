//===- tools/seec-trace-view/ActionReplay.hpp -----------------------------===//
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

#ifndef SEEC_TRACE_VIEW_ACTIONREPLAY_HPP
#define SEEC_TRACE_VIEW_ACTIONREPLAY_HPP

#include "seec/Util/Error.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/TemplateSequence.hpp"

#include "llvm/ADT/DenseMap.h"

#include <wx/wx.h>
#include <wx/timer.h>

#include "ActionRecord.hpp"

#include <array>
#include <memory>
#include <type_traits>


class ActionEventListCtrl;
class wxGauge;


/// \brief Interface for handling recorded events.
///
class IEventHandler
{
protected:
  virtual seec::Maybe<seec::Error> handle_impl(wxXmlNode &Event) =0;
  
  seec::Error error_attribute(std::string const &Name) const;

public:
  virtual ~IEventHandler() =0;
  
  virtual seec::Maybe<seec::Error> handle(wxXmlNode &Event) {
    return handle_impl(Event);
  }
};

/// \brief Generic implementation of IEventHandler using std::function callback.
///
template<typename... Ts>
class EventHandler : public IEventHandler
{
  /// Indices for accessing the Values and Attributes.
  typedef typename seec::ct::generate_sequence_int<0, sizeof...(Ts)>::type
          IndexSeqTy;
  
  // Check that all attributes would inherit from IAttributeReadWrite.
  static_assert(
    seec::ct::static_all_of<
      std::is_base_of<IAttributeReadWrite,
                      Attribute<typename std::add_lvalue_reference<Ts>::type>
                      >::value...>::value,
    "At least one attribute is not writable.");
  
  /// Value objects used when parsing information.
  std::tuple<typename std::remove_reference<Ts>::type...> Values;
  
  /// Attributes used when parsing (these use the above Values).
  std::array<std::unique_ptr<IAttributeReadWrite>, sizeof...(Ts)> Attributes;
  
  /// The callback function that handles this event.
  std::function<void (Ts...)> Callback;
  
private:
  /// \brief Call the Callback function using the values in Values.
  ///
  template<int... Idxs>
  void dispatch_from_tuple(seec::ct::sequence_int<Idxs...>)
  {
    Callback(std::get<Idxs>(Values)...);
  }
  
  /// \brief Internal constructor implementation.
  ///
  template<int... Idxs>
  EventHandler(std::array<std::string, sizeof...(Ts)> &AttributeNames,
               std::function<void (Ts...)> WithCallback,
               seec::ct::sequence_int<Idxs...>)
  : Values(),
    Attributes{{
      seec::makeUnique<Attribute<typename std::add_lvalue_reference<Ts>::type>>
                      (std::move(std::get<Idxs>(AttributeNames)),
                       std::get<Idxs>(Values))...}},
    Callback(std::move(WithCallback))
  {}
  
protected:
  /// \brief Attempt to parse and dispatch the given event.
  ///
  virtual seec::Maybe<seec::Error> handle_impl(wxXmlNode &Event)
  {
    // First parse all attributes into the Values tuple.
    wxString ValueString;
    
    for (auto &Attr : Attributes) {
      if (!Event.GetAttribute(Attr->get_name(), &ValueString))
        return error_attribute(Attr->get_name());
      
      if (!Attr->from_string(ValueString.ToStdString()))
        return error_attribute(Attr->get_name());
    }
    
    dispatch_from_tuple(IndexSeqTy{});
    
    return seec::Maybe<seec::Error>();
  }

public:
  /// \brief Constructor.
  ///
  EventHandler(std::array<std::string, sizeof...(Ts)> AttributeNames,
               std::function<void (Ts...)> WithCallback)
  : EventHandler(AttributeNames, std::move(WithCallback), IndexSeqTy{})
  {}
};

/// \brief Replays user interactions with the trace viewer.
///
class ActionReplayFrame : public wxFrame
{
  /// Automatically play the recording.
  wxButton *ButtonPlay;
  
  /// Stop automatically playing the recording.
  wxButton *ButtonPause;
  
  /// Step to the next event in the recording.
  wxButton *ButtonStep;
  
  /// Shows the progress through the recording.
  wxGauge *GaugeEventProgress;
  
  /// Shows a list of all events.
  ActionEventListCtrl *EventList;
  
  /// Event handler lookup.
  std::map<std::string, std::unique_ptr<IEventHandler>> Handlers;
  
  /// The loaded action record.
  std::unique_ptr<wxXmlDocument> RecordDocument;
  
  /// The next event to replay.
  wxXmlNode *NextEvent;
  
  /// The time of the most recently played event.
  uint64_t LastEventTime;
  
  /// Timer for automatically "playing" a recording.
  wxTimer EventTimer;
  
  /// \brief Replay the next event.
  ///
  void ReplayEvent();
  
  /// \brief Move to the next event.
  ///
  void MoveToNextEvent();
  
  /// \brief Called when the play button is pressed.
  ///
  void OnPlay(wxCommandEvent &);
  
  /// \brief Called when the pause button is pressed.
  ///
  void OnPause(wxCommandEvent &);
  
  /// \brief Called when the step button is pressed.
  ///
  void OnStep(wxCommandEvent &);
  
  /// \brief Set the play timer for the next event.
  ///
  void SetEventTimer();
  
  /// \brief Called when the play timer expires.
  ///
  void OnEventTimer(wxTimerEvent &);
  
public:
  /// \brief Constructor (without creation).
  ///
  ActionReplayFrame();
  
  /// \brief Constructor (with creation).
  ///
  ActionReplayFrame(wxWindow *Parent);
  
  /// \brief Destructor.
  ///
  ~ActionReplayFrame();
  
  /// \brief Create the frame (if it was default-constructed).
  ///
  bool Create(wxWindow *Parent);
  
  /// \brief Load the given XML recording.
  ///
  bool LoadRecording(wxXmlDocument const &Recording);
  
  /// \brief Register a handler.
  ///
  bool RegisterHandler(std::string Name, std::unique_ptr<IEventHandler> Handler)
  {
    auto const TheName = Name;
    auto const Result = Handlers.insert(std::make_pair(std::move(Name),
                                                       std::move(Handler)));
    if (!Result.second) {
      wxLogDebug("Failed to add handler %s.", TheName);
    }
    return Result.second;
  }
  
  /// \brief Create and register a handler from a std::function.
  ///
  template<typename... Ts>
  bool RegisterHandler(std::string Name,
                       std::array<std::string, sizeof...(Ts)> Attributes,
                       std::function<void (Ts...)> Callback)
  {
    return RegisterHandler(std::move(Name),
                           seec::makeUnique<EventHandler<Ts...>>
                                           (std::move(Attributes),
                                            std::move(Callback)));
  }
};


#endif // SEEC_TRACE_VIEW_ACTIONREPLAY_HPP
