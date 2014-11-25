//===- lib/wxWidgets/AugmentationCollectionDataViewModel.cpp --------------===//
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
#include "seec/wxWidgets/AugmentationCollectionDataViewModel.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/Util/MakeUnique.hpp"

#include <wx/log.h>
#include <wx/settings.h>

#include <string>

namespace seec {

//------------------------------------------------------------------------------
// AugmentationCollectionDataViewModel
//------------------------------------------------------------------------------

std::unique_ptr<wxDataViewColumn>
AugmentationCollectionDataViewModel::getIDColumn()
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];
  auto const Col = static_cast<unsigned>(EColumnKind::ID);

  return seec::makeUnique<wxDataViewColumn>
                         (towxString(Res["Columns"]["ID"]),
                          new wxDataViewTextRenderer(),
                          Col,
                          wxDVC_DEFAULT_WIDTH,
                          wxALIGN_LEFT);
}

std::unique_ptr<wxDataViewColumn>
AugmentationCollectionDataViewModel::getEnabledColumn()
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];
  auto const Col = static_cast<unsigned>(EColumnKind::Enabled);
  auto const Renderer =
    new wxDataViewToggleRenderer("bool", wxDATAVIEW_CELL_ACTIVATABLE);

  return seec::makeUnique<wxDataViewColumn>
                         (towxString(Res["Columns"]["Enabled"]), Renderer, Col);
}

std::unique_ptr<wxDataViewColumn>
AugmentationCollectionDataViewModel::getNameColumn()
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];
  auto const Col = static_cast<unsigned>(EColumnKind::Name);

  return seec::makeUnique<wxDataViewColumn>
                         (towxString(Res["Columns"]["Name"]),
                          new wxDataViewTextRenderer(),
                          Col,
                          wxDVC_DEFAULT_WIDTH,
                          wxALIGN_LEFT);
}

std::unique_ptr<wxDataViewColumn>
AugmentationCollectionDataViewModel::getSourceColumn()
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];
  auto const Col = static_cast<unsigned>(EColumnKind::Source);

  return seec::makeUnique<wxDataViewColumn>
                         (towxString(Res["Columns"]["Source"]),
                          new wxDataViewTextRenderer(),
                          Col,
                          wxDVC_DEFAULT_WIDTH,
                          wxALIGN_LEFT);
}

std::unique_ptr<wxDataViewColumn>
AugmentationCollectionDataViewModel::getVersionColumn()
{
  auto const Res = Resource("TraceViewer")["GUIText"]["AugmentationSettings"];
  auto const Col = static_cast<unsigned>(EColumnKind::Version);

  return seec::makeUnique<wxDataViewColumn>
                         (towxString(Res["Columns"]["Version"]),
                          new wxDataViewTextRenderer(),
                          Col,
                          wxDVC_DEFAULT_WIDTH,
                          wxALIGN_RIGHT);
}

AugmentationCollectionDataViewModel
::AugmentationCollectionDataViewModel(AugmentationCollection &Collection)
: wxDataViewVirtualListModel(Collection.getAugmentations().size()),
  m_Collection(Collection)
{
  m_Collection.addListener(this);
}

AugmentationCollectionDataViewModel
::~AugmentationCollectionDataViewModel()
{
  m_Collection.removeListener(this);
}

unsigned AugmentationCollectionDataViewModel::GetColumnCount() const
{
  return static_cast<unsigned>(EColumnKind::Last);
}

wxString
AugmentationCollectionDataViewModel::GetColumnType(unsigned const Column) const
{
  assert(Column < static_cast<unsigned>(EColumnKind::Last));

  switch (static_cast<EColumnKind>(Column)) {
    case EColumnKind::ID:         return "string";
    case EColumnKind::Enabled:    return "bool";
    case EColumnKind::Name:       return "string";
    case EColumnKind::Source:     return "string";
    case EColumnKind::Version:    return "string";
    case EColumnKind::Last:       return "null";
  }

  // Should be unreachable.
  return "null";
}

void AugmentationCollectionDataViewModel::GetValueByRow(wxVariant &Variant,
                                                        unsigned const Row,
                                                        unsigned const Column)
const
{
  assert(Column < static_cast<unsigned>(EColumnKind::Last));

  auto const &Augs = m_Collection.getAugmentations();
  if (Row >= Augs.size())
    return;

  auto const &Aug = Augs[Row];

  switch (static_cast<EColumnKind>(Column)) {
    case EColumnKind::ID:
      Variant = Aug.getID();
      break;
    case EColumnKind::Enabled:
      Variant = Aug.isEnabled();
      break;
    case EColumnKind::Name:
      Variant = Aug.getName();
      break;
    case EColumnKind::Source:
      Variant = Aug.getSource();
      break;
    case EColumnKind::Version:
      Variant = wxString(std::to_string(Aug.getVersion()));
      break;
    case EColumnKind::Last: break;
  }
}

bool
AugmentationCollectionDataViewModel::SetValueByRow(wxVariant const &Variant,
                                                   unsigned const Row,
                                                   unsigned const Column)
{
  assert(Column < static_cast<unsigned>(EColumnKind::Last));

  if (Row >= m_Collection.getAugmentations().size())
    return false;

  auto &Aug = m_Collection.getAugmentation(Row);

  switch (static_cast<EColumnKind>(Column)) {
    case EColumnKind::ID:      return false;
    case EColumnKind::Enabled:
    {
      auto const Value = Variant.GetBool();
      Aug.setEnabled(Value);

      if (Value)
        m_Collection.activate(Row);
      else
        m_Collection.deactivate(Row);

      return true;
    }
    case EColumnKind::Name:    return false;
    case EColumnKind::Source:  return false;
    case EColumnKind::Version: return false;
    case EColumnKind::Last:    return false;
  }

  // Should be unreachable.
  return false;
}

bool
AugmentationCollectionDataViewModel::GetAttrByRow(unsigned const Row,
                                                  unsigned const Column,
                                                  wxDataViewItemAttr &Attr)
const
{
  if (!m_Collection.isActive(Row)) {
    Attr.SetColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    return true;
  }

  return false;
}

void
AugmentationCollectionDataViewModel
::DocAppended(AugmentationCollection const &Collection)
{
  RowAppended();
}

void
AugmentationCollectionDataViewModel
::DocDeleted(AugmentationCollection const &Collection, unsigned const Index)
{
  RowDeleted(Index);
}

void
AugmentationCollectionDataViewModel
::DocChanged(AugmentationCollection const &Collection, unsigned const Index)
{
  RowChanged(Index);
}

} // namespace seec
