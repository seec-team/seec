//===- tools/seec-trace-view/NotifyContext.hpp ----------------------------===//
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

#ifndef SEEC_TRACE_VIEW_NOTIFYCONTEXT_HPP
#define SEEC_TRACE_VIEW_NOTIFYCONTEXT_HPP

#include "llvm/Support/Casting.h"

#include "StateAccessToken.hpp"

#include <functional>
#include <list>
#include <memory>
#include <mutex>


namespace clang {
  class Decl;
  class Stmt;
}

namespace seec {
  namespace cm {
    class Value;
  }
}


/// \brief All possible kinds of context events.
///
enum class ContextEventKind {
  HighlightDecl,
  HighlightStmt,
  HighlightValue
};


/// \brief Base class for all context events.
///
class ContextEvent {
  /// The kind of this ContextEvent.
  ContextEventKind Kind;
  
protected:
  /// \brief Constructor.
  ///
  ContextEvent(ContextEventKind const WithKind)
  : Kind(WithKind)
  {}
  
public:
  /// \brief Get the kind of this ContextEvent.
  ///
  ContextEventKind getKind() const { return Kind; }
};


/// \brief Indicates that a Decl should be highlighted.
///
class ConEvHighlightDecl final : public ContextEvent {
  /// The Decl that should be highlighted.
  ::clang::Decl const *TheDecl;
  
public:
  /// \brief Constructor.
  ///
  ConEvHighlightDecl(::clang::Decl const *WithDecl)
  : ContextEvent(ContextEventKind::HighlightDecl),
    TheDecl(WithDecl)
  {}
  
  /// \brief Support LLVM's RTTI.
  ///
  static bool classof(ContextEvent const *Ev) {
    return Ev->getKind() == ContextEventKind::HighlightDecl;
  }
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the Decl that should be highlighted (may be nullptr).
  ///
  ::clang::Decl const *getDecl() const { return TheDecl; }
  
  /// @} (Accessors)
};


/// \brief Indicates that a Stmt should be highlighted.
///
class ConEvHighlightStmt final : public ContextEvent {
  /// The Stmt that should be highlighted.
  ::clang::Stmt const *TheStmt;
  
public:
  /// \brief Constructor.
  ///
  ConEvHighlightStmt(::clang::Stmt const *WithStmt)
  : ContextEvent(ContextEventKind::HighlightStmt),
    TheStmt(WithStmt)
  {}
  
  /// \brief Support LLVM's RTTI.
  ///
  static bool classof(ContextEvent const *Ev) {
    return Ev->getKind() == ContextEventKind::HighlightStmt;
  }
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the Stmt that should be highlighted (may be nullptr).
  ///
  ::clang::Stmt const *getStmt() const { return TheStmt; }
  
  /// @} (Accessors)
};


/// \brief Indicates that a \c seec::cm::Value should be highlighted.
///
class ConEvHighlightValue final : public ContextEvent {
  /// The \c seec::cm::Value that should be highlighted.
  seec::cm::Value const *TheValue;
  
  /// Access for the state that the \c seec::cm::Value belongs to.
  std::shared_ptr<StateAccessToken> Access;
  
public:
  /// \brief Constructor.
  ///
  /// \param WithValue The \c Value that should be highlighted.
  /// \param WithAccess The access token associates with this \c Value's state.
  ///                   The access must be locked while this event is raised.
  ///
  ConEvHighlightValue(seec::cm::Value const *WithValue,
                      std::shared_ptr<StateAccessToken> WithAccess)
  : ContextEvent(ContextEventKind::HighlightValue),
    TheValue(WithValue),
    Access(std::move(WithAccess))
  {}
  
  /// \brief Support LLVM's RTTI.
  ///
  static bool classof(ContextEvent const *Ev) {
    return Ev->getKind() == ContextEventKind::HighlightValue;
  }
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the Value that should be highlighted (may be nullptr).
  ///
  seec::cm::Value const *getValue() const { return TheValue; }
  
  /// \brief Get the access for the Value.
  ///
  std::shared_ptr<StateAccessToken> const &getAccess() const { return Access; }
  
  /// @} (Accessors)
};


/// \brief Interface for listening to all context notifications.
///
class ContextListener {
public:
  virtual void notifyContextEvent(ContextEvent const &) const;
};


/// \brief Handles registering for and dispatching context notifications.
///
class ContextNotifier {
public:
  typedef std::function<void (ContextEvent const &)> CallbackTy;
  
  typedef std::list<CallbackTy>::iterator CallbackIterTy;
  
  typedef std::list<ContextListener *>::iterator ListenerIterTy;
  
private:
  /// Holds all registered callbacks.
  std::list<CallbackTy> Callbacks;
  
  /// Controls access to Callbacks.
  std::mutex mutable CallbacksMutex;
  
  /// Holds all registered listeners.
  std::list<ContextListener *> Listeners;
  
  /// Controls access to Listeners.
  std::mutex mutable ListenersMutex;
  
  // No copying.
  ContextNotifier(ContextNotifier const &) = delete;
  ContextNotifier &operator=(ContextNotifier const &) = delete;
  
public:
  /// \brief Constructor.
  ///
  ContextNotifier()
  : Callbacks(),
    CallbacksMutex(),
    Listeners(),
    ListenersMutex()
  {}
  
  
  /// \name Callback registration.
  /// @{
  
  /// \brief Add a new callback.
  ///
  /// \return An iterator which can be used to remove the callback.
  ///
  CallbackIterTy callbackAdd(CallbackTy Callback);
  
  /// \brief Remove a callback using the iterator returned by callbackAdd().
  ///
  void callbackRemove(CallbackIterTy It);
  
  /// \brief Handles registration of a callback for its lifetime.
  ///
  class CallbackRegistrar {
    /// The notifier that this registrar added the callback to.
    ContextNotifier &Notifier;
    
    /// The iterator returned when the callback was added.
    CallbackIterTy const Iter;
    
  public:
    /// \brief Register ForCallback with ForNotifier for this object's lifetime.
    ///
    CallbackRegistrar(ContextNotifier &ForNotifier, CallbackTy ForCallback)
    : Notifier(ForNotifier),
      Iter(Notifier.callbackAdd(std::move(ForCallback)))
    {}
    
    /// \brief Destructor will deregister the callback.
    ///
    ~CallbackRegistrar() {
      Notifier.callbackRemove(Iter);
    }
  };
  
  /// \brief Create and return a CallbackRegistrar for a callback.
  ///
  CallbackRegistrar callbackRegister(CallbackTy Callback)
  {
    return CallbackRegistrar(*this, std::move(Callback));
  }
  
  /// @} (Callback registration)
  
  
  /// \name Listener registration.
  /// @{
  
  /// \brief Add a new listener.
  ///
  /// \return An iterator which can be used to remove the listener.
  ///
  ListenerIterTy listenerAdd(ContextListener *Listener);
  
  /// \brief Remove a listener using the iterator returned by listenerAdd().
  ///
  void listenerRemove(ListenerIterTy It);
  
  /// \brief Handles registration of a listener for its lifetime.
  ///
  class ListenerRegistrar {
    /// The notifier that this registrar added the callback to.
    ContextNotifier &Notifier;
    
    /// The iterator returned when the listener was added.
    ListenerIterTy const Iter;
    
  public:
    /// \brief Register ForListener with ForNotifier for this object's lifetime.
    ///
    ListenerRegistrar(ContextNotifier &ForNotifier,
                      ContextListener *ForListener)
    : Notifier(ForNotifier),
      Iter(Notifier.listenerAdd(std::move(ForListener)))
    {}
    
    /// \brief Destructor will deregister the callback.
    ///
    ~ListenerRegistrar() {
      Notifier.listenerRemove(Iter);
    }
  };
  
  /// \brief Create and return a ListenerRegistrar for a listener.
  ///
  ListenerRegistrar listenerRegister(ContextListener *Listener)
  {
    return ListenerRegistrar(*this, Listener);
  }
  
  /// @} (Listener registration)
  
  
  /// \name Dispatching.
  /// @{
  
  /// \brief Notify all callbacks and listeners of an event.
  ///
  void notify(ContextEvent const &Ev) const;
  
  /// \brief Create an event and notify all callbacks and listeners of it.
  ///
  template<typename EventT, typename... ArgTs>
  void createNotify(ArgTs&&... Args) const
  {
    EventT Event(std::forward<ArgTs>(Args)...);
    notify(Event);
  }
  
  /// @} (Dispatching)
};


#endif // SEEC_TRACE_VIEW_NOTIFYCONTEXT_HPP
