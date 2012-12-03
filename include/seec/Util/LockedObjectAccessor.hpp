//===- include/seec/Util/LockedObjectAccessor.hpp ------------------- C++ -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements a convenience class template for coupling a reference
/// to an object with a std::unique_lock that holds ownership to access that
/// object.
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_LOCKEDOBJECTACCESSOR_HPP
#define SEEC_UTIL_LOCKEDOBJECTACCESSOR_HPP

#include <mutex>

namespace seec {

template<typename ObjectT, typename MutexT>
class LockedObjectAccessor {
  std::unique_lock<MutexT> Lock;
  
  ObjectT &Object;

public:
  LockedObjectAccessor(MutexT &Mutex, ObjectT &Object)
  : Lock(Mutex),
    Object(Object)
  {}
  
  LockedObjectAccessor(LockedObjectAccessor &&) = default;
  
  std::unique_lock<MutexT> const &getLock() const { return Lock; }
  
  ObjectT &getObject() const { return Object; }
  
  ObjectT &operator*() const { return Object; }
  
  ObjectT *operator->() const { return &Object; }
};

template<typename MutexT, typename ObjectT>
LockedObjectAccessor<ObjectT, MutexT>
makeLockedObjectAccessor(MutexT &Mutex, ObjectT &Object) {
  return LockedObjectAccessor<ObjectT, MutexT>(Mutex, Object);
}

} // namespace seec

#endif // SEEC_UTIL_LOCKEDOBJECTACCESSOR
