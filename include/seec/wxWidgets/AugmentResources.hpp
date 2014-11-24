//===- include/seec/wxWidgets/AugmentResources.hpp ------------------ C++ -===//
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

#ifndef SEEC_WXWIDGETS_AUGMENTRESOURCES_HPP
#define SEEC_WXWIDGETS_AUGMENTRESOURCES_HPP

#include "seec/ICU/Augmenter.hpp"
#include "seec/Util/Maybe.hpp"

#include <unicode/locid.h>
#include <unicode/unistr.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class wxString;
class wxXmlDocument;

namespace seec {


/// \brief Checks if a \x wxXmlDocument is a valid augmentation.
///
bool isAugmentation(wxXmlDocument const &Doc);


/// \brief Represents a single augmentation file.
///
class Augmentation final {
  std::unique_ptr<wxXmlDocument> m_XmlDocument;

  Augmentation(std::unique_ptr<wxXmlDocument> Doc);

public:
  ~Augmentation();

  Augmentation(Augmentation &&) = default;

  Augmentation &operator=(Augmentation &&) = default;

  static Maybe<Augmentation> fromDoc(std::unique_ptr<wxXmlDocument> Doc);

  wxXmlDocument const &getXmlDocument() const { return *m_XmlDocument; }

  wxString getName() const;

  wxString getID() const;
};


/// \brief Holds augmentations for ICU resources.
///
class AugmentationCollection final {
  std::vector<Augmentation> m_Augmentations;

public:
  AugmentationCollection();

  ~AugmentationCollection();

  bool loadFromDoc(std::unique_ptr<wxXmlDocument> Doc);

  void loadFromFile(wxString const &Path);

  void loadFromDirectory(wxString const &Path);

  void loadFromResources(std::string const &ResourcePath);

  static wxString getUserLocalDataDirForAugmentations();

  void loadFromUserLocalDataDir();

  wxString getAugmentationFor(wxString const &Type,
                              wxString const &Identifier,
                              ::icu::Locale const &Loc) const;

  UnicodeString getAugmentationFor(UnicodeString const &Type,
                                   UnicodeString const &Identifier) const;

  seec::AugmentationCallbackFn getCallbackFn() const {
    return [this] (UnicodeString const &Type, UnicodeString const &ID) {
      return this->getAugmentationFor(Type, ID);
    };
  }
};


} // namespace seec

#endif // SEEC_WXWIDGETS_AUGMENTRESOURCES_HPP
