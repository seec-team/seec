//===- MallocViewer.cpp ---------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "MallocViewer.hpp"
#include "OpenTrace.hpp"

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/dataview.h>
#include "seec/wxWidgets/CleanPreprocessor.h"


//------------------------------------------------------------------------------
// MallocListModel
//------------------------------------------------------------------------------

///
class MallocListModel : public wxDataViewModel
{
  /// The ProcessState being modelled (if any).
  seec::trace::ProcessState *State;

public:
  enum class Columns : unsigned int {
    Time = 0,
    Address,
    Size,
    Highest
  };

  /// Constructor.
  MallocListModel()
  : State(nullptr)
  {}

  /// Destructor.
  virtual ~MallocListModel() {}

  virtual unsigned int GetChildren(wxDataViewItem const &Item,
                                   wxDataViewItemArray &Children) const {
    if (!State)
      return 0;

    if (Item.IsOk())
      return 0;

    // Get all items.
    for (auto &MallocPair : State->getMallocs()) {
      auto ConstVoidPtr = reinterpret_cast<void const *>(&(MallocPair.second));
      auto VoidPtr = const_cast<void *>(ConstVoidPtr);
      Children.Add(wxDataViewItem(VoidPtr));
    }

    return static_cast<unsigned int>(State->getMallocs().size());
  }

  virtual unsigned int GetColumnCount() const {
    return static_cast<unsigned int>(Columns::Highest);
  }

  virtual wxString GetColumnType(unsigned int Column) const {
    switch (static_cast<Columns>(Column)) {
      case Columns::Time: // Fall-through intentional.
      case Columns::Address: // Fall-through intentional.
      case Columns::Size:
        return "string";
      case Columns::Highest:
        break;
    }

    return "string";
  }

  virtual wxDataViewItem GetParent(wxDataViewItem const &Item) const {
    return wxDataViewItem(nullptr);
  }

  virtual void GetValue(wxVariant &Variant,
                        wxDataViewItem const &Item,
                        unsigned int Column) const {
    if (!State)
      return;

    // Get the MallocState from the Item.
    auto &Malloc
      = *reinterpret_cast<seec::trace::MallocState const *>(Item.GetID());

    switch (static_cast<Columns>(Column)) {
      case Columns::Time:
        Variant = wxString("-");
        return;
      case Columns::Address:
        Variant = (wxString() << Malloc.getAddress());
        return;
      case Columns::Size:
        Variant = (wxString() << Malloc.getSize());
        return;
      case Columns::Highest:
        break;
    }

    Variant = (wxString("Bad Column #") << Column);
  }

  virtual bool IsContainer(wxDataViewItem const &WXUNUSED(Item)) const {
    return false;
  }

  virtual bool SetValue(wxVariant const &WXUNUSED(Variant),
                        wxDataViewItem const &WXUNUSED(Item),
                        unsigned int WXUNUSED(Column)) {
    return false;
  }

  void setState(seec::trace::ProcessState &NewState) {
    State = &NewState;

    // Remove all existing items.
    Cleared();

    // Add all mallocs in the new state.
    wxDataViewItemArray Items;

    for (auto &MallocPair : State->getMallocs()) {
      auto ConstVoidPtr = reinterpret_cast<void const *>(&(MallocPair.second));
      auto VoidPtr = const_cast<void *>(ConstVoidPtr);
      Items.Add(wxDataViewItem(VoidPtr));
    }

    ItemsAdded(wxDataViewItem(nullptr), Items);
  }
};


//------------------------------------------------------------------------------
// MallocViewerPanel
//------------------------------------------------------------------------------

MallocViewerPanel::~MallocViewerPanel() {
  //
}

bool MallocViewerPanel::Create(wxWindow *Parent,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Create the data view.
  DataModel = new MallocListModel();
  DataView = new wxDataViewCtrl(this, wxID_ANY);

  DataView->AssociateModel(DataModel);
  DataModel->DecRef(); // Discount our reference to the data model.

  // Column 0 of the list (Time).
  auto TimeRenderer = new wxDataViewTextRenderer("string",
                                                 wxDATAVIEW_CELL_INERT);
  auto TimeTitle = seec::getwxStringExOrEmpty(TextTable,
                                              "MallocView_ColumnTime");
  auto TimeNumber = static_cast<unsigned int>(MallocListModel::Columns::Time);
  auto TimeColumn = new wxDataViewColumn(std::move(TimeTitle),
                                         TimeRenderer,
                                         TimeNumber,
                                         100,
                                         wxALIGN_LEFT,
                                         wxDATAVIEW_COL_RESIZABLE);
  DataView->AppendColumn(TimeColumn);

  // Column 1 of the list (Address).
  auto AddressRenderer = new wxDataViewTextRenderer("string",
                                                    wxDATAVIEW_CELL_INERT);
  auto AddressTitle = seec::getwxStringExOrEmpty(TextTable,
                                                 "MallocView_ColumnAddress");
  auto AddressNumber
    = static_cast<unsigned int>(MallocListModel::Columns::Address);
  auto AddressColumn = new wxDataViewColumn(std::move(AddressTitle),
                                         AddressRenderer,
                                         AddressNumber,
                                         100,
                                         wxALIGN_LEFT,
                                         wxDATAVIEW_COL_RESIZABLE);
  DataView->AppendColumn(AddressColumn);

  // Column 2 of the list (Size).
  auto SizeRenderer = new wxDataViewTextRenderer("string",
                                                 wxDATAVIEW_CELL_INERT);
  auto SizeTitle = seec::getwxStringExOrEmpty(TextTable,
                                              "MallocView_ColumnSize");
  auto SizeNumber = static_cast<unsigned int>(MallocListModel::Columns::Size);
  auto SizeColumn = new wxDataViewColumn(std::move(SizeTitle),
                                         SizeRenderer,
                                         SizeNumber,
                                         100,
                                         wxALIGN_LEFT,
                                         wxDATAVIEW_COL_RESIZABLE);
  DataView->AppendColumn(SizeColumn);

  // Make the DataView occupy this entire panel.
  auto Sizer = new wxBoxSizer(wxHORIZONTAL);
  Sizer->Add(DataView, wxSizerFlags().Proportion(1).Expand());
  SetSizerAndFit(Sizer);

  return true;
}

void MallocViewerPanel::show(OpenTrace &TraceInfo,
                             seec::trace::ProcessState &State) {
  DataModel->setState(State);
}

void MallocViewerPanel::clear() {
  //
}
