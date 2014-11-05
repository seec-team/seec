//===- lib/ICU/Resources.cpp ----------------------------------------------===//
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

#include "seec/ICU/Resources.hpp"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

std::unique_ptr<ResourceBundle> getResourceBundle(char const *Package,
                                                  Locale const &GetLocale)
{
  UErrorCode Status = U_ZERO_ERROR;

  std::unique_ptr<ResourceBundle> Resource
    (new ResourceBundle(Package, GetLocale, Status));

  if (U_SUCCESS(Status)) {
    return Resource;
  }

  return nullptr;
}

seec::Maybe<ResourceBundle, UErrorCode>
getResource(char const *Package, llvm::ArrayRef<char const *> const &Keys)
{
  UErrorCode Status = U_ZERO_ERROR;
  ResourceBundle Bundle(Package, Locale{}, Status);
  if (U_FAILURE(Status))
    return Status;
  return getResource(Bundle, Keys);
}

seec::Maybe<ResourceBundle, UErrorCode>
getResource(ResourceBundle const &RB, llvm::ArrayRef<char const *> const &Keys)
{
  UErrorCode Status = U_ZERO_ERROR;
  ResourceBundle Bundle(RB);
  
  if (U_FAILURE(Status))
    return Status;
  
  for (auto const &Key : Keys) {
    Bundle = Bundle.getWithFallback(Key, Status);
    if (U_FAILURE(Status))
      return Status;
  }
  
  return Bundle;
}

seec::Maybe<UnicodeString, UErrorCode>
getString(char const *Package, llvm::ArrayRef<char const *> const &Keys)
{
  auto const MaybeBundle = getResource(Package, Keys);
  if (MaybeBundle.assigned<UErrorCode>())
    return MaybeBundle.get<UErrorCode>();
  
  UErrorCode Status = U_ZERO_ERROR;
  auto const &Bundle = MaybeBundle.get<ResourceBundle>();
  auto const String = Bundle.getString(Status);
  if (U_FAILURE(Status))
    return Status;
  
  return String;
}

seec::Maybe<UnicodeString, UErrorCode>
getString(ResourceBundle const &RB, llvm::ArrayRef<char const *> const &Keys)
{
  auto const MaybeBundle = getResource(RB, Keys);
  if (MaybeBundle.assigned<UErrorCode>())
    return MaybeBundle.get<UErrorCode>();
  
  UErrorCode Status = U_ZERO_ERROR;
  auto const &Bundle = MaybeBundle.get<ResourceBundle>();
  auto const String = Bundle.getString(Status);
  if (U_FAILURE(Status))
    return Status;
  
  return String;
}


//------------------------------------------------------------------------------
// Resource
//------------------------------------------------------------------------------

Resource Resource::operator[] (char const * const Key) const &
{
  Resource Ret = *this;

  if (!U_FAILURE(Ret.m_Status))
    Ret.m_Bundle = Ret.m_Bundle.getWithFallback(Key, Ret.m_Status);

  return Ret;
}

Resource Resource::get(llvm::ArrayRef<char const *> Keys) const &
{
  Resource Ret = *this;

  for (auto const Key : Keys)
    Ret = Ret[Key];

  return Ret;
}

std::pair<llvm::ArrayRef<uint8_t>, UErrorCode> Resource::getBinary() const &
{
  int32_t Length = -1;
  UErrorCode Status = m_Status;
  auto const Data = m_Bundle.getBinary(Length, Status);

  if (U_FAILURE(Status) || Length < 0)
    return std::make_pair(llvm::ArrayRef<uint8_t>(), Status);

  return std::make_pair(llvm::ArrayRef<uint8_t>
                                      (Data, static_cast<std::size_t>(Length)),
                        Status);
}

std::pair<UnicodeString, UErrorCode> Resource::getString() const &
{
  std::pair<UnicodeString, UErrorCode> Result { UnicodeString(), U_ZERO_ERROR };

  Result.first = m_Bundle.getString(Result.second);

  return Result;
}

std::pair<UnicodeString, UErrorCode>
Resource::getStringOrDefault(UnicodeString const &Default) const &
{
  auto Result = getString();

  if (U_FAILURE(Result.second))
    Result.first = Default;

  return Result;
}

std::pair<int32_t, UErrorCode> Resource::getInt() const &
{
  return getIntOrDefault(0);
}

std::pair<int32_t, UErrorCode>
Resource::getIntOrDefault(int32_t const Default) const &
{
  if (U_FAILURE(m_Status))
    return std::make_pair(Default, m_Status);

  UErrorCode Status = m_Status;
  auto const Value = m_Bundle.getInt(Status);

  return std::make_pair(U_SUCCESS(Status) ? Value : Default, Status);
}

llvm::ArrayRef<uint8_t> Resource::asBinary() const
{
  return getBinary().first;
}

UnicodeString Resource::asString() const &
{
  return getString().first;
}

UnicodeString Resource::asStringOrDefault(UnicodeString const &Default) const &
{
  return getStringOrDefault(Default).first;
}

int32_t Resource::asInt() const &
{
  return getInt().first;
}

int32_t Resource::asIntOrDefault(int32_t const Default) const &
{
  return getIntOrDefault(Default).first;
}


//------------------------------------------------------------------------------
// ResourceLoader
//------------------------------------------------------------------------------

ResourceLoader::ResourceLoader(llvm::StringRef ExecutablePath)
: ResourcesDirectory(ExecutablePath)
{}

bool ResourceLoader::loadResource(char const *Package)
{
  std::string PackageStr (Package);

  // check if we've already loaded the package
  if (Resources.count(PackageStr))
    return true;

  // find and load the package
  llvm::SmallString<256> PackagePath {ResourcesDirectory};
  llvm::sys::path::append(PackagePath, Package);
  PackagePath.append(".dat");

  llvm::OwningPtr<llvm::MemoryBuffer> Holder;
  llvm::MemoryBuffer::getFile(PackagePath.str(), Holder);

  if (!Holder) {
    return false;
  }

  // add to our resource map
  auto Insert = Resources.insert(
                  std::make_pair(
                    std::move(PackageStr),
                    std::unique_ptr<llvm::MemoryBuffer>(Holder.take())));

  // register the data with ICU
  UErrorCode Status = U_ZERO_ERROR;

  udata_setAppData(Package, // package name
                   Insert.first->second->getBufferStart(), // data
                   &Status);

  if (U_FAILURE(Status)) {
    Resources.erase(Insert.first);
    return false;
  }

  return true;
}

} // namespace seec
