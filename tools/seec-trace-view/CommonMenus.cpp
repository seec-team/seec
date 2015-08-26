//===- tools/seec-trace-view/CommonMenus.cpp ------------------------------===//
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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeFunction.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/menu.h>

#include "ActionRecord.hpp"
#include "ActionRecordSettings.hpp"
#include "ActionReplay.hpp"
#include "AnnotationEditor.hpp"
#include "CommonMenus.hpp"
#include "LocaleSettings.hpp"
#include "ProcessMoveEvent.hpp"
#include "TraceViewerFrame.hpp"

#include <algorithm>

void BindMenuItem(wxMenuItem *Item,
                  std::function<void (wxEvent &)> Handler)
{
  if (!Item)
    return;
  
  auto const Menu = Item->GetMenu();
  if (!Menu)
    return;
  
  Menu->Bind(wxEVT_MENU, Handler, Item->GetId());
}

template<typename ContainerT, typename T>
bool contains(ContainerT const &Container, T const &Item) {
  return std::find(begin(Container), end(Container), Item) != end(Container);
}

namespace {

char const *getKeyForMovementResult(seec::cm::MovementResult const Result)
{
  switch (Result)
  {
    case seec::cm::MovementResult::Unmoved:
      return "Unmoved";
    case seec::cm::MovementResult::PredicateSatisfied:
      return "PredicateSatisfied";
    case seec::cm::MovementResult::ReachedBeginning:
      return "ReachedBeginning";
    case seec::cm::MovementResult::ReachedEnd:
      return "ReachedEnd";
  }

  llvm_unreachable("unexpected seec::cm::MovementResult");
}

void describeContextualNavigationResult(wxWindow &Control,
                                        char const * const NavigationKey,
                                        seec::cm::MovementResult const Result)
{
  if (Result == seec::cm::MovementResult::PredicateSatisfied)
    return;

  auto const Res =
    seec::Resource("TraceViewer")["ContextualNavigationFailure"]
                                 [NavigationKey]
                                 [getKeyForMovementResult(Result)];

  if (U_FAILURE(Res.status())) {
    wxLogDebug("Couldn't get message for contextual navigation failure with "
               "Key='%s' and Result='%s'",
               NavigationKey,
               getKeyForMovementResult(Result));
    return;
  }

  wxMessageBox(seec::towxString(Res));
}

} // anonymous namespace

std::pair<std::unique_ptr<wxMenu>, wxString>
createFileMenu(std::vector<wxStandardID> const &AdditionalIDs)
{
  auto const Title = seec::towxString(seec::Resource("TraceViewer")
                                      ["GUIText"]["Menu_File"]);
  
  auto Menu = seec::makeUnique<wxMenu>();
  
  Menu->Append(wxID_OPEN);
  Menu->Append(wxID_CLOSE);

  if (contains(AdditionalIDs, wxID_SAVEAS))
    Menu->Append(wxID_SAVEAS);

  Menu->AppendSeparator();
  Menu->Append(wxID_EXIT);
  
  return std::make_pair(std::move(Menu), Title);
}

std::pair<std::unique_ptr<wxMenu>, wxString> createFileMenu()
{
  return createFileMenu({});
}

std::pair<std::unique_ptr<wxMenu>, wxString> createEditMenu()
{
  auto const Res = seec::Resource("TraceViewer")["GUIText"]["MenuEdit"];
  auto const Title = seec::towxString(Res["Title"]);

  auto Menu = seec::makeUnique<wxMenu>();
  Menu->Append(wxID_PREFERENCES);

  return std::make_pair(std::move(Menu), Title);
}

std::pair<std::unique_ptr<wxMenu>, wxString>
createEditMenu(TraceViewerFrame &TheFrame)
{
  auto TheMenu = createEditMenu();

  if (TheMenu.first) {
    auto const Res = seec::Resource("TraceViewer")["GUIText"]["MenuEdit"];

    BindMenuItem(
      TheMenu.first->Append(wxID_ANY,
                            seec::towxString(Res["ThreadTimeAnnotation"])),
      [&TheFrame] (wxEvent &) -> void {
        TheFrame.editThreadTimeAnnotation();
      });
  }

  return TheMenu;
}

std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(wxEvtHandler &EvtHandler)
{
#ifdef SEEC_USER_ACTION_RECORDING
  auto const Title = seec::getwxStringExOrEmpty("TraceViewer",
                        (char const * []){"GUIText", "MenuRecord", "Title"});
  
  auto Menu = seec::makeUnique<wxMenu>();
  
  // Item for opening the recording settings menu.
  BindMenuItem(
    Menu->Append(wxID_ANY,
                 seec::getwxStringExOrEmpty("TraceViewer",
                    (char const * []){"GUIText", "MenuRecord", "Settings"})),
    [] (wxEvent &) {
      showActionRecordSettings();
    });
  
  return std::make_pair(std::move(Menu), Title);
#else
  return std::make_pair(std::unique_ptr<wxMenu>(), wxEmptyString);
#endif
}

std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(TraceViewerFrame &Viewer)
{
  return createRecordingMenu(static_cast<wxEvtHandler &>(Viewer));
}

bool append(wxMenuBar *MenuBar,
            std::pair<std::unique_ptr<wxMenu>, wxString> MenuWithTitle)
{
  if (!MenuWithTitle.first)
    return false;
  
  return MenuBar->Append(MenuWithTitle.first.release(),
                         MenuWithTitle.second);
}

void addDeclAnnotationEdit(wxMenu &Menu,
                           wxWindow *Parent,
                           OpenTrace &Trace,
                           clang::Decl const *Declaration)
{
  auto Res = seec::Resource("TraceViewer")["GUIText"]["AnnotationEditor"];

  BindMenuItem(
    Menu.Append(wxID_ANY, seec::towxString(Res["EditDecl"])),
    [&, Parent, Declaration] (wxEvent &) -> void {
      showAnnotationEditorDialog(Parent, Trace, Declaration);
    });
}

void addStmtAnnotationEdit(wxMenu &Menu,
                           wxWindow *Parent,
                           OpenTrace &Trace,
                           clang::Stmt const *Statement)
{
  auto Res = seec::Resource("TraceViewer")["GUIText"]["AnnotationEditor"];

  BindMenuItem(
    Menu.Append(wxID_ANY, seec::towxString(Res["EditStmt"])),
    [&, Parent, Statement] (wxEvent &) -> void {
      showAnnotationEditorDialog(Parent, Trace, Statement);
    });
}

void addStmtNavigation(wxWindow &Control,
                       std::shared_ptr<StateAccessToken> &Access,
                       wxMenu &Menu,
                       std::size_t const ThreadIndex,
                       clang::Stmt const *Statement,
                       ActionRecord * const Recording)
{
  UErrorCode Status = U_ZERO_ERROR;
  auto const TextTable = seec::getResource("TraceViewer",
                                           getLocale(),
                                           Status,
                                           "ContextualNavigation");
  if (U_FAILURE(Status))
    return;
  
  BindMenuItem(
    Menu.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "StmtRewind")),
    [&, ThreadIndex, Statement, Recording] (wxEvent &Ev) -> void {
      if (Recording) {
        Recording->recordEventL("ContextualNavigation.StmtRewind",
                                make_attribute("thread", ThreadIndex),
                                make_attribute("stmt", Statement));
      }
      else {
        wxLogDebug("no recording.");
      }

      raiseMovementEvent(Control, Access,
        [=, &Control]
        (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
          auto &Thread = State.getThread(ThreadIndex);
          auto Ret = seec::cm::moveBackwardUntilEvaluated(Thread, Statement);
          describeContextualNavigationResult(Control, "StmtRewind", Ret);
          return Ret;
        });
    });
  
  BindMenuItem(
    Menu.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "StmtForward")),
    [&, ThreadIndex, Statement, Recording] (wxEvent &Ev) -> void {
      if (Recording) {
        Recording->recordEventL("ContextualNavigation.StmtForward",
                                make_attribute("thread", ThreadIndex),
                                make_attribute("stmt", Statement));
      }
      else {
        wxLogDebug("no recording.");
      }

      raiseMovementEvent(Control, Access,
        [=, &Control]
        (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
          auto &Thread = State.getThread(ThreadIndex);
          auto Ret = seec::cm::moveForwardUntilEvaluated(Thread, Statement);
          describeContextualNavigationResult(Control, "StmtForward", Ret);
          return Ret;
        });
    });
}

static void recordValueNavigation(char const * const Navigation,
                                  seec::cm::Value const &Value,
                                  ActionRecord * const Recording)
{
  if (!Recording)
    return;

  std::vector<std::unique_ptr<IAttributeReadOnly>> Attributes;
  addAttributesForValue(Attributes, Value);
  Recording->recordEventV(Navigation, Attributes);
}

void addValueNavigation(wxWindow &Control,
                        std::shared_ptr<StateAccessToken> &Access,
                        wxMenu &Menu,
                        seec::cm::Value const &Value,
                        seec::cm::ProcessState const &State,
                        ActionRecord * const Recording)
{
  auto const TextTable = seec::Resource("TraceViewer")["ContextualNavigation"];

  if (U_FAILURE(TextTable.status()))
    return;

  // Contextual movement based on the Value's memory.
  if (Value.isInMemory()) {
    auto const Size = Value.getTypeSizeInChars().getQuantity();
    auto const Area = seec::MemoryArea(Value.getAddress(), Size);
    auto const IsStatic = State.isStaticallyAllocated(Area.address());

    if (!IsStatic) {
      BindMenuItem(
        Menu.Append(wxID_ANY,
                    seec::towxString(TextTable["ValueRewindAllocation"])),
        [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
          recordValueNavigation("ContextualNavigation.ValueRewindAllocation",
                                Value, Recording);

          raiseMovementEvent(Control, Access,
            [=, &Control]
            (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
              auto const Ret = seec::cm::moveToAllocation(State, Area.start());
              describeContextualNavigationResult(Control,
                                                 "ValueRewindAllocation",
                                                 Ret);
              return Ret;
            });
        });
    }

    BindMenuItem(
      Menu.Append(wxID_ANY,
                  seec::towxString(TextTable["ValueRewindModification"])),
      [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
        recordValueNavigation("ContextualNavigation.ValueRewindModification",
                              Value, Recording);

        raiseMovementEvent(Control, Access,
          [=, &Control]
          (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
            auto Ret = seec::cm::moveBackwardUntilMemoryChanges(State, Area);
            describeContextualNavigationResult(Control,
                                               "ValueRewindModification",
                                               Ret);
            return Ret;
          });
      });

    BindMenuItem(
      Menu.Append(wxID_ANY,
                  seec::towxString(TextTable["ValueForwardModification"])),
      [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
        recordValueNavigation("ContextualNavigation.ValueForwardModification",
                              Value, Recording);

        raiseMovementEvent(Control, Access,
          [=, &Control]
          (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
            auto Ret = seec::cm::moveForwardUntilMemoryChanges(State, Area);
            describeContextualNavigationResult(Control,
                                               "ValueForwardModification",
                                               Ret);
            return Ret;
          });
      });

    if (!IsStatic) {
      BindMenuItem(
        Menu.Append(wxID_ANY,
                    seec::towxString(TextTable["ValueForwardDeallocation"])),
        [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
          recordValueNavigation("ContextualNavigation.ValueForwardDeallocation",
                                Value, Recording);

          raiseMovementEvent(Control, Access,
            [=, &Control]
            (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
              auto const Ret = seec::cm::moveToDeallocation(State,
                                                            Area.start());
              describeContextualNavigationResult(Control,
                                                 "ValueForwardDeallocation",
                                                 Ret);
              return Ret;
            });
        });
    }

    // Pointer values.
    if (auto const PtrVal = llvm::dyn_cast<seec::cm::ValueOfPointer>(&Value)) {
      auto const Address = PtrVal->getRawValue();
      auto const Limit = PtrVal->getDereferenceIndexLimit();

      if (Address && Limit == 0 && !PtrVal->isValidOpaque()) {
        Menu.AppendSeparator();

        BindMenuItem(
          Menu.Append(wxID_ANY,
                      seec::towxString(TextTable["ValueOfPointerRewindValid"])),
          [=, &Control, &Access] (wxEvent &Ev) -> void {
            if (Recording) {
              Recording->recordEventL(
                "ContextualNavigation.ValueOfPointerRewindValid",
                make_attribute("raw_value", Address));
            }

            raiseMovementEvent(Control, Access,
              [=, &Control]
              (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
                auto const Ret = seec::cm::moveBackwardUntilAllocated(State,
                                                                      Address);
                describeContextualNavigationResult(Control,
                                                   "ValueOfPointerRewindValid",
                                                   Ret);
                return Ret;
              });
          });
      }
    }
  }
}

void registerNavigationReplay(wxWindow &Control,
                              std::shared_ptr<StateAccessToken> &Access,
                              ActionReplayFrame &Replay)
{
  Replay.RegisterHandler("ContextualNavigation.StmtRewind",
                         {{"thread", "stmt"}}, seec::make_function(
    [&] (std::size_t const ThreadIdx, clang::Stmt const * const Stmt) -> void {
      raiseMovementEvent(Control, Access,
        [ThreadIdx, Stmt]
        (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
          auto &Thread = State.getThread(ThreadIdx);
          return seec::cm::moveBackwardUntilEvaluated(Thread, Stmt);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.StmtForward",
                         {{"thread", "stmt"}}, seec::make_function(
    [&] (std::size_t const ThreadIdx, clang::Stmt const * const Stmt) -> void {
      raiseMovementEvent(Control, Access,
        [ThreadIdx, Stmt]
        (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
          auto &Thread = State.getThread(ThreadIdx);
          return seec::cm::moveForwardUntilEvaluated(Thread, Stmt);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.ValueRewindAllocation",
                         {{"address"}}, seec::make_function(
    [&] (std::uintptr_t const Address) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) {
          return seec::cm::moveToAllocation(State, Address);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.ValueRewindModification",
                         {{"address", "size"}}, seec::make_function(
    [&] (std::uintptr_t const Address, std::size_t const Size) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) {
          auto const Area = seec::MemoryArea(Address, Size);
          return seec::cm::moveBackwardUntilMemoryChanges(State, Area);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.ValueForwardModification",
                         {{"address", "size"}}, seec::make_function(
    [&] (std::uintptr_t const Address, std::size_t const Size) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) {
          auto const Area = seec::MemoryArea(Address, Size);
          return seec::cm::moveForwardUntilMemoryChanges(State, Area);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.ValueForwardDeallocation",
                         {{"address"}}, seec::make_function(
    [&] (std::uintptr_t const Address) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) {
          return seec::cm::moveToDeallocation(State, Address);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.ValueOfPointerRewindValid",
                         {{"raw_value"}}, seec::make_function(
    [&] (std::uintptr_t const RawValue) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) {
          return seec::cm::moveBackwardUntilAllocated(State, RawValue);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.StreamRewindToWrite",
                         {{"address", "position"}}, seec::make_function(
    [&] (std::uintptr_t const Address, std::size_t const Position) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> seec::cm::MovementResult {
          auto const Stream = State.getStream(Address);
          if (!Stream)
            return seec::cm::MovementResult::Unmoved;

          return seec::cm::moveBackwardToStreamWriteAt(State,
                                                       *Stream,
                                                       Position);
        });
    }));
}
