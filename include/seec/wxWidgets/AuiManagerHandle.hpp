//===- include/seec/wxWidgets/AuiManagerHandle.hpp ------------------ C++ -===//
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

#ifndef SEEC_WXWIDGETS_AUIMANAGERHANDLE_HPP
#define SEEC_WXWIDGETS_AUIMANAGERHANDLE_HPP

#include <utility>

class wxAuiManager;

namespace seec {

/// \brief Wraps a \c wxAuiManager pointer and uninitializes that
/// \c wxAuiManager when this handle is destroyed.
///
class wxAuiManagerHandle
{
  /// The underlying \c wxAuiManager.
  wxAuiManager *m_Manager;
  
  // don't allow copying
  wxAuiManagerHandle(wxAuiManagerHandle const &) = delete;
  wxAuiManagerHandle operator=(wxAuiManagerHandle const &) = delete;
  
public:
  /// \brief Construct an empty handle.
  ///
  wxAuiManagerHandle()
  : m_Manager(nullptr)
  {}
  
  /// \brief Construct a handle that takes ownership of \c Manager
  ///
  wxAuiManagerHandle(wxAuiManager * const Manager)
  : m_Manager(Manager)
  {}
  
  /// \brief Move ownership from \c Other to a new handle.
  ///
  wxAuiManagerHandle(wxAuiManagerHandle &&Other)
  : m_Manager(Other.m_Manager)
  {
    Other.m_Manager = nullptr;
  }
  
  /// \brief Move ownership from \c Other to this handle.
  ///
  wxAuiManagerHandle &operator=(wxAuiManagerHandle &&RHS)
  {
    std::swap(m_Manager, RHS.m_Manager);
    return *this;
  }
  
  /// \brief Destroy this handle.
  /// This will call \c UnInit() on the underlying \c wxAuiManager
  ///
  ~wxAuiManagerHandle();
  
  wxAuiManager *operator->() { return m_Manager; }
  wxAuiManager &operator*() { return *m_Manager; }
};

} // namespace seec

#endif // SEEC_WXWIDGETS_AUIMANAGERHANDLE_HPP
