//===- seec/Util/Observer.hpp --------------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_OBSERVER_HPP
#define SEEC_UTIL_OBSERVER_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <vector>
#include <utility>

namespace seec {

namespace observer {

using observer_id = unsigned;

/// \brief Shared base type for the deregistration proxy.
///
class deregistration_proxy {
public:
  virtual void deregisterObserver(observer_id const ID) = 0;
};

/// \brief Holds a listener's registration to a subject.
/// The registration can be moved, but not copied. When the registration is
/// destroyed, the listener's function will be removed from the subject.
///
class registration {
  std::weak_ptr<deregistration_proxy> m_Proxy;

  observer_id m_ID;

  void deregister()
  {
    if (auto Proxy = m_Proxy.lock()) {
      Proxy->deregisterObserver(m_ID);
    }
  }

public:
  registration()
  : m_Proxy(),
    m_ID(0)
  {}
  
  registration(std::weak_ptr<deregistration_proxy> Proxy,
               observer_id const ID)
  : m_Proxy(Proxy),
    m_ID(ID)
  {}
  
  registration(registration &&Other)
  : registration()
  {
    *this = std::move(Other);
  }
  
  registration &operator=(registration &&RHS)
  {
    deregister();
    std::swap(m_Proxy, RHS.m_Proxy);
    std::swap(m_ID, RHS.m_ID);
    return *this;
  }
  
  ~registration()
  {
    deregister();
  }
  
  registration(registration &) = delete;
  registration(registration const &) = delete;
  registration &operator=(registration &) = delete;
  registration &operator=(registration const &) = delete;
};

/// \brief Something that can be observed.
///
template<typename... NotificationTs>
class subject {
  /// \brief This specialization's implementation of the deregistration proxy.
  class deregistration_proxy_impl : public deregistration_proxy {
    subject &m_Subject;
    
  public:
    deregistration_proxy_impl(subject &Subject)
    : m_Subject(Subject)
    {}

    virtual ~deregistration_proxy_impl() {}

    virtual void deregisterObserver(observer_id const ID) override
    {
      m_Subject.deregisterObserver(ID);
    }
  };
  
  using FnT = std::function<void (NotificationTs...)>;
  
  std::shared_ptr<deregistration_proxy> m_Proxy;
  
  observer_id m_NextID;
  
  std::vector<std::pair<observer_id, FnT>> m_Observers;
  
  void deregisterObserver(observer_id const ID)
  {
    auto const It = std::find_if(m_Observers.begin(), m_Observers.end(),
      [=] (std::pair<observer_id, FnT> const &Entry) {
        return Entry.first == ID;
      });
    
    if (It != m_Observers.end()) {
      m_Observers.erase(It);
    }
  }
  
public:
  subject()
  : m_Proxy(std::make_shared<deregistration_proxy_impl>(*this)),
    m_NextID(0),
    m_Observers()
  {}
  
  ~subject()
  {
    // Destroy the proxy object so that registration objects will not attempt
    // to deregister with this subject hereafter.
    assert(m_Proxy && m_Proxy.use_count() == 1);
    m_Proxy.reset();
  }
  
  registration registerObserver(FnT F)
  {
    auto const ID = m_NextID++;
    m_Observers.emplace_back(ID, std::move(F));
    return registration(m_Proxy, ID);
  }
  
  void notifyObservers(NotificationTs const & ...Args) const
  {
    for (auto &P : m_Observers) {
      if (P.second) {
        P.second(Args...);
      }
    }
  }
};

} // namespace observer

} // namespace seec

#endif // define SEEC_UTIL_OBSERVER_HPP
