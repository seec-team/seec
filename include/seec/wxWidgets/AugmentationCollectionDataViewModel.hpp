//===- include/seec/wxWidgets/AugmentationCollectionDataViewModel.hpp C++ -===//
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

#ifndef SEEC_WXWIDGETS_AUGMENTATIONCOLLECTIONDATAVIEWMODEL_HPP
#define SEEC_WXWIDGETS_AUGMENTATIONCOLLECTIONDATAVIEWMODEL_HPP

#include "seec/wxWidgets/AugmentResources.hpp"

#include <wx/dataview.h>

#include <memory>

class wxDataViewColumn;
class wxDataViewItemAttr;
class wxString;
class wxVariant;

namespace seec {

class AugmentationCollection;

/// \brief Support interacting with an \c AugmentationCollection through a
///        \c wxDataViewCtrl.
///
class AugmentationCollectionDataViewModel final
: public wxDataViewVirtualListModel, public AugmentationCollection::Listener
{
  /// The \c AugmentationCollection this model represents.
  AugmentationCollection &m_Collection;

public:
  /// \brief Defines the columns supported by this model.
  ///
  enum class EColumnKind : unsigned {
    ID = 0,
    Enabled,
    Name,
    Source,
    Version,
    Last
  };

  /// \brief Get a \c wxDataViewColumn for the ID column.
  ///
  static std::unique_ptr<wxDataViewColumn> getIDColumn();

  /// \brief Get a \c wxDataViewColumn for the Enabled column.
  ///
  static std::unique_ptr<wxDataViewColumn> getEnabledColumn();

  /// \brief Get a \c wxDataViewColumn for the Name column.
  ///
  static std::unique_ptr<wxDataViewColumn> getNameColumn();

  /// \brief Get a \c wxDataViewColumn for the Source column.
  ///
  static std::unique_ptr<wxDataViewColumn> getSourceColumn();

  /// \brief Get a \c wxDataViewColumn for the Version column.
  ///
  static std::unique_ptr<wxDataViewColumn> getVersionColumn();

  /// \brief Construct a new model for the given collection.
  ///
  AugmentationCollectionDataViewModel(AugmentationCollection &Collection);

  /// \brief Destructor.
  ///
  virtual ~AugmentationCollectionDataViewModel() override;

  /// \name wxDataViewVirtualListModel methods.
  /// @{

  /// \brief Get the number of columns supported by this model.
  ///
  virtual unsigned GetColumnCount() const override;

  /// \brief Get the type of the specified column.
  ///
  virtual wxString GetColumnType(unsigned const Column) const override;

  /// \brief Get the value of a cell.
  ///
  virtual void GetValueByRow(wxVariant &Variant,
                             unsigned const Row,
                             unsigned const Column) const override;

  /// \brief Set the value of a cell.
  ///
  virtual bool SetValueByRow(wxVariant const &, unsigned, unsigned) override;

  /// \brief Get special attributes for a cell.
  ///
  virtual bool GetAttrByRow(unsigned, unsigned, wxDataViewItemAttr &) const
    override;

  /// @}

  /// \name AugmentationCollection::Listener methods.
  /// @{

  /// \brief Called when a new \c Augmentation is added to the collection.
  ///
  virtual void DocAppended(AugmentationCollection const &Collection) override;

  /// \brief Called when an \c Augmentation is removed from the collection.
  ///
  virtual void DocDeleted(AugmentationCollection const &Collection,
                          unsigned const Index) override;

  /// \brief Called when an \c Augmentation is updated.
  ///
  virtual void DocChanged(AugmentationCollection const &Collection,
                          unsigned const Index) override;

  /// @}
};

}

#endif // SEEC_WXWIDGETS_AUGMENTATIONCOLLECTIONDATAVIEWMODEL_HPP
