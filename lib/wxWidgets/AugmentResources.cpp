//===- lib/wxWidgets/AugmentResources.cpp ---------------------------------===//
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

#include "seec/wxWidgets/AugmentResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/wxWidgets/XmlNodeIterator.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Range.hpp"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/xml/xml.h>

#include <algorithm>

namespace seec {

AugmentationCollection::AugmentationCollection() = default;

AugmentationCollection::~AugmentationCollection() = default;

void AugmentationCollection::loadFromFile(wxString const &Path)
{
  auto Doc = seec::makeUnique<wxXmlDocument>(Path);
  if (!Doc)
    return;

  m_XmlDocuments.emplace_back(std::move(Doc));
}

void AugmentationCollection::loadFromDirectory(wxString const &DirPath)
{
  wxDir Dir(DirPath);
  if (!Dir.IsOpened())
    return;

  auto Path = wxFileName::DirName(DirPath);
  wxString File;
  bool GotFile = Dir.GetFirst(&File, "*.xml");

  while (GotFile) {
    Path.SetName(File);
    loadFromFile(Path.GetFullPath());
    GotFile = Dir.GetNext(&File);
  }
}

void AugmentationCollection::loadFromResources(std::string const &ResourcePath)
{
  auto Path = wxFileName::DirName(ResourcePath);
  Path.AppendDir("augment");
  loadFromDirectory(Path.GetFullPath());
}

void AugmentationCollection::loadFromUserLocalDataDir()
{
  auto Path = wxFileName::DirName(wxStandardPaths::Get().GetUserLocalDataDir());
  Path.AppendDir("augment");
  loadFromDirectory(Path.GetFullPath());
}

bool getStringsForAugFromPackageForLocale(wxXmlNode *Augmentations,
                                          wxString const &Name,
                                          wxString const &ID,
                                          icu::Locale const &Loc,
                                          std::vector<wxString> &Out)
{
  auto const Key = (Loc == icu::Locale::getRoot()) ? "root" : Loc.getBaseName();

  auto const It = std::find_if(seec::wxXmlNodeIterator(Augmentations),
                               seec::wxXmlNodeIterator(nullptr),
                                [&] (wxXmlNode const &Node) {
                                  return Node.GetName() == "augmentations"
                                      && Node.GetAttribute("locale") == Key;
                                });

  if (It == seec::wxXmlNodeIterator(nullptr))
    return false;

  for (wxXmlNode &Entry : *It) {
    if (Entry.GetName() == Name && Entry.GetAttribute("id") == ID) {
      Out.push_back(Entry.GetNodeContent());
      return true;
    }
  }

  return false;
}

void getStringsForAugFromDoc(wxXmlDocument const &Doc,
                             wxString const &Name,
                             wxString const &ID,
                             icu::Locale const &Loc,
                             std::vector<wxString> &Out)
{
  auto const RootNode = Doc.GetRoot();
  auto const Augs = RootNode->GetChildren();

  if (getStringsForAugFromPackageForLocale(Augs, Name, ID, Loc, Out))
    return;

  if (Loc.getVariant()) {
    auto Fallback = icu::Locale(Loc.getLanguage(), Loc.getCountry());
    if (getStringsForAugFromPackageForLocale(Augs, Name, ID, Fallback, Out))
      return;
  }

  // Try without country (language only).
  if (Loc.getCountry()) {
    auto Fallback = icu::Locale(Loc.getLanguage());
    if (getStringsForAugFromPackageForLocale(Augs, Name, ID, Fallback, Out))
      return;
  }

  // Try the root locale.
  auto Fallback = icu::Locale::getRoot();
  getStringsForAugFromPackageForLocale(Augs, Name, ID, Fallback, Out);
}

wxString AugmentationCollection::getAugmentationFor(wxString const &Type,
                                                    wxString const &Identifier,
                                                    ::icu::Locale const &Loc)
const
{
  std::vector<wxString> Strings;
  for (auto const &Doc : m_XmlDocuments)
    getStringsForAugFromDoc(*Doc, Type, Identifier, Loc, Strings);

  wxString Combined;
  for (auto &String : Strings)
    Combined += String + "\n";
  return Combined;
}

UnicodeString
AugmentationCollection::getAugmentationFor(UnicodeString const &Type,
                                           UnicodeString const &Identifier)
const
{
  return toUnicodeString(getAugmentationFor(towxString(Type),
                                            towxString(Identifier),
                                            icu::Locale::getDefault()));
}

} // namespace seec
