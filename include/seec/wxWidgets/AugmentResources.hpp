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
public:
  enum class EKind {
    Resource, ///< From SeeC's shared resources directory.
    UserLocal ///< From the user's local data directory.
  };

private:
  /// The \x wxXmlDocument defining this augmentation.
  std::unique_ptr<wxXmlDocument> m_XmlDocument;

  /// Determine what kind of augmentation document this is.
  EKind m_Kind;

  /// Path to the augmentation document on disk.
  std::string m_Path;

  /// \brief Constructor.
  ///
  Augmentation(std::unique_ptr<wxXmlDocument> Doc,
               EKind const Kind,
               std::string Path);

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
  static Maybe<Augmentation> fromDoc(std::unique_ptr<wxXmlDocument> Doc,
                                     EKind const Kind,
                                     std::string Path);

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

  /// \brief Get the version of this augmentation.
  ///
  unsigned getVersion() const;

  /// \brief Get the \c EKind of this augmentation.
  ///
  EKind getKind() const { return m_Kind; }

  /// \brief Get the path to this augmentation document on disk.
  ///
  std::string const &getPath() const { return m_Path; }

  /// \brief Check if this augmentation is enabled.
  ///
  bool isEnabled() const;

  /// \brief Set whether this augmentation is enabled.
  ///
  void setEnabled(bool const Value);
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

    /// \brief Called when an \c Augmentation is removed.
    virtual void DocDeleted(AugmentationCollection const &, unsigned Index) =0;

    /// \brief Called when an \c Augmentation is updated.
    virtual void DocChanged(AugmentationCollection const &, unsigned Index) =0;
  };

private:
  /// Holds all \c Augmentation objects.
  std::vector<Augmentation> m_Augmentations;

  /// Holds indices of the active \c Augmentation objects (those which are not
  /// outdated by another \c Augmentation object).
  std::vector<unsigned> m_ActiveAugmentations;

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
  bool loadFromDoc(std::unique_ptr<wxXmlDocument> Doc,
                   Augmentation::EKind const Kind,
                   std::string Path);

  /// \brief Load an \c Augmentation from the given file path.
  ///
  void loadFromFile(wxString const &Path,
                    Augmentation::EKind const Kind);

  /// \brief Load all *.xml files in the given directory as \c Augmentation
  ///        objects.
  ///
  void loadFromDirectory(wxString const &Path,
                         Augmentation::EKind const Kind);

  /// \brief Load all augmentations from SeeC's resource directory.
  ///
  void loadFromResources(std::string const &ResourcePath);

  /// \brief Get the directory used for user-specific augmentations.
  ///
  static wxString getUserLocalDataDirForAugmentations();

  /// \brief Load all augmentations from the user-specific directory.
  ///
  void loadFromUserLocalDataDir();

  /// \brief Delete a user-local augmentation document, and remove it from this
  ///        collection.
  ///
  bool deleteUserLocalAugmentation(unsigned const Index);

  /// \brief Get all \c Augmentation objects in this collection.
  ///
  std::vector<Augmentation> const &getAugmentations() const {
    return m_Augmentations;
  }

  /// \brief Get the \c Augmentation at the given index.
  ///
  Augmentation &getAugmentation(unsigned const Index) {
    return m_Augmentations[Index];
  }

  /// \brief Active the \c Augmentation at the given index, if it is the best
  ///        candidate (highest enabled version).
  ///
  void activate(unsigned const Index);

  /// \brief Remove the \c Augmentation at the given index from activity (if it
  ///        is currently active).
  /// \return the index of the newly active \c Augmentation (if any).
  ///
  void deactivate(unsigned const Index);

  /// \brief Check if the \c Augmentation at a given index is active.
  ///
  bool isActive(unsigned const Index) const;

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
