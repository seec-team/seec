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

class wxDateTime;
class wxString;
class wxXmlDocument;

namespace seec {

/// \brief Checks if a \x wxXmlDocument is a valid augmentation.
///
bool isAugmentation(wxXmlDocument const &Doc);


/// \brief Represents a single augmentation file.
///
class Augmentation final {
  /// The \x wxXmlDocument defining this augmentation.
  std::unique_ptr<wxXmlDocument> m_XmlDocument;

  /// \brief Constructor.
  ///
  Augmentation(std::unique_ptr<wxXmlDocument> Doc);

public:
  /// \brief Destructor.
  ///
  ~Augmentation();

  /// \brief Move constructor.
  ///
  Augmentation(Augmentation &&) = default;

  /// \brief Move assignment.
  ///
  Augmentation &operator=(Augmentation &&) = default;

  /// \brief Attempt to create an \c Augmentation from a \c wxXmlDocument.
  /// If the \c Doc is not an augmentation according to \c isAugmentation(),
  /// then this method returns an unassigned \c Maybe.
  ///
  static Maybe<Augmentation> fromDoc(std::unique_ptr<wxXmlDocument> Doc);

  /// \brief Get the underlying \c wxXmlDocument.
  ///
  wxXmlDocument const &getXmlDocument() const { return *m_XmlDocument; }

  /// \brief Get the name of this augmentation.
  ///
  wxString getName() const;

  /// \brief Get the ID of this augmentation.
  ///
  wxString getID() const;

  /// \brief Get the source of this augmentation.
  /// If this augmentation was downloaded through SeeC's trace viewer, this
  /// will be the URL it was downloaded from.
  ///
  wxString getSource() const;

  /// \brief Get the time that this augmentation was downloaded (if it was
  ///        downloaded through SeeC's trace viewer).
  ///
  wxDateTime getDownloaded() const;
};


/// \brief Holds augmentations for ICU resources.
///
class AugmentationCollection final {
public:
  /// \brief Interface for listening to changes to a \c AugmentationCollection.
  ///
  class Listener {
  public:
    /// \brief Called when a new \c Augmentation is added.
    virtual void DocAppended(AugmentationCollection const &) =0;
  };

private:
  /// Holds the \c Augmentation objects.
  std::vector<Augmentation> m_Augmentations;

  /// All active \c Listener pointers.
  std::vector<Listener *> m_Listeners;

public:
  /// \brief Constructor.
  ///
  AugmentationCollection();

  /// \brief Destructor.
  ///
  ~AugmentationCollection();

  /// \brief Load an \c Augmentation directly from a \c wxXmlDocument.
  ///
  bool loadFromDoc(std::unique_ptr<wxXmlDocument> Doc);

  /// \brief Load an \c Augmentation from the given file path.
  ///
  void loadFromFile(wxString const &Path);

  /// \brief Load all *.xml files in the given directory as \c Augmentation
  ///        objects.
  ///
  void loadFromDirectory(wxString const &Path);

  /// \brief Load all augmentations from SeeC's resource directory.
  ///
  void loadFromResources(std::string const &ResourcePath);

  /// \brief Get the directory used for user-specific augmentations.
  ///
  static wxString getUserLocalDataDirForAugmentations();

  /// \brief Load all augmentations from the user-specific directory.
  ///
  void loadFromUserLocalDataDir();

  /// \brief Get all \c Augmentation objects in this collection.
  ///
  std::vector<Augmentation> const &getAugmentations() const {
    return m_Augmentations;
  }

  wxString getAugmentationFor(wxString const &Type,
                              wxString const &Identifier,
                              ::icu::Locale const &Loc) const;

  UnicodeString getAugmentationFor(UnicodeString const &Type,
                                   UnicodeString const &Identifier) const;

  /// \brief Get a function that implements the \c AugmentationCallbackFn
  ///        interface for this collection.
  ///
  seec::AugmentationCallbackFn getCallbackFn() const {
    return [this] (UnicodeString const &Type, UnicodeString const &ID) {
      return this->getAugmentationFor(Type, ID);
    };
  }

  /// \brief Register a \c Listener with this collection.
  ///
  void addListener(Listener * const TheListener);

  /// \brief Remove a \c Listener from this collection.
  ///
  void removeListener(Listener * const TheListener);
};


} // namespace seec

#endif // SEEC_WXWIDGETS_AUGMENTRESOURCES_HPP
