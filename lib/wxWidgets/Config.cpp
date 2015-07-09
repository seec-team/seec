//===- lib/wxWidgets/Config.cpp -------------------------------------------===//
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

#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/Config.hpp"

#include <wx/app.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>

#include <cstdlib>
#include <memory>

namespace seec {

namespace {

void shutdownDummyAppConsole()
{
  auto const App = wxAppConsole::GetInstance();
  if (!App)
    return;

  App->OnExit();
  wxEntryCleanup();

  wxAppConsole::SetInstance(nullptr);
}

} // anonymous namespace

void setupDummyAppConsole()
{
  wxAppConsole::SetInstance(new wxAppConsole());

  char progname[] = "seec";
  char *argv[2] = { progname, nullptr };
  int argc = 1;

  wxEntryStart(argc, argv);

  wxAppConsole::GetInstance()->OnInit();

  std::atexit(shutdownDummyAppConsole);
}

bool setupCommonConfig()
{
  assert(wxAppConsole::GetInstance());

  // Setup the configuration to use a file in the user's data directory. If we
  // don't do this ourselves then the default places the config file in the same
  // path as the directory would take, causing an unfortunate collision.
  wxFileName ConfigPath (getUserLocalDataPath());

  if (!wxDirExists(ConfigPath.GetFullPath())) {
    if (!wxMkdir(ConfigPath.GetFullPath())) {
      wxLogDebug("couldn't create config directory %s",
                 ConfigPath.GetFullPath());
      return false;
    }
  }

  ConfigPath.SetFullName("config");
  auto const Config =
    new wxFileConfig(wxEmptyString, wxEmptyString, ConfigPath.GetFullPath());

  if (!Config) {
    wxLogDebug("couldn't create config file");
    return false;
  }

  // Set our new config as the base, and delete the old base (if any).
  std::unique_ptr<wxConfigBase> Previous;
  Previous.reset(wxConfigBase::Set(Config));

  return true;
}

std::string getUserLocalDataPath()
{
  wxFileName ConfigPath;

#if defined(SEEC_CSSE_LAB_OSX)
  // Special override for the CSSE lab machines.
  ConfigPath.AssignDir("/Users");
  ConfigPath.AppendDir(wxGetUserId());
  ConfigPath.AppendDir("linuxNetworkHome");
  ConfigPath.AppendDir("linuxHome");

  if (ConfigPath.DirExists()) {
    ConfigPath.AppendDir(".seec-view");
    if (!ConfigPath.DirExists()) {
      ConfigPath.Mkdir();
    }
  }

  if (!ConfigPath.DirExists()) {
    ConfigPath.AssignDir(StdPaths.GetUserLocalDataDir());
  }
#else
  ConfigPath.AssignDir(wxStandardPaths::Get().GetUserLocalDataDir());
#endif

  return ConfigPath.GetFullPath().ToStdString();
}

}
