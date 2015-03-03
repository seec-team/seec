//===- include/seec/Clang/MappedFile.hpp ----------------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDFILE_HPP
#define SEEC_CLANG_MAPPEDFILE_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/StringRef.h"

#include <string>

namespace llvm {
  class Metadata;
  class Value;
}

namespace seec {

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {

/// \brief A source file used in the mapping.
class MappedFile {
  /// The file name.
  std::string FileName;
  
  /// The directory containing the file.
  std::string DirectoryName;
  
public:
  /// \name Constructors.
  /// @{
  
  /// \brief Construct a new MappedFile.
  MappedFile(llvm::StringRef File, llvm::StringRef Directory)
  : FileName(File),
    DirectoryName(Directory)
  {}
  
  /// \brief Copy constructor.
  MappedFile(MappedFile const &) = default;
  
  /// \brief Move constructor.
  MappedFile(MappedFile &&) = default;
  
  /// \brief Read a MappedFile from metadata.
  static seec::Maybe<MappedFile> fromMetadata(llvm::Metadata *);
  
  /// @}
  
  
  /// \name Operators.
  /// @{
  
  /// \brief Copy assignment.
  MappedFile &operator=(MappedFile const &) = default;
  
  /// \brief Move assignment.
  MappedFile &operator=(MappedFile &&) = default;
  
  /// @}
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the file name of this file.
  std::string const &getFileName() const { return FileName; }
  
  /// \brief Get the directory containing this file.
  std::string const &getDirectoryName() const { return DirectoryName; }
  
  /// @}
};

} // namespace seec_clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDFILE_HPP
