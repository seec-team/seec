//===- tools/seec-trace-view/HiddenExecuteAndWait_WIN32.cpp ---------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file Much of this file is adjusted from the LLVM files:
///   llvm/lib/Support/Windows/Program.inc
///   llvm/lib/Support/Windows/WindowsSupport.h
///
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"

#include "HiddenExecuteAndWait.hpp"

#include <windows.h>

#include <system_error>

template <typename HandleTraits>
class ScopedHandle {
  typedef typename HandleTraits::handle_type handle_type;
  handle_type Handle;

  ScopedHandle(const ScopedHandle &other); // = delete;
  void operator=(const ScopedHandle &other); // = delete;
public:
  ScopedHandle()
    : Handle(HandleTraits::GetInvalid()) {}

  explicit ScopedHandle(handle_type h)
    : Handle(h) {}

  ~ScopedHandle() {
    if (HandleTraits::IsValid(Handle))
      HandleTraits::Close(Handle);
  }

  handle_type take() {
    handle_type t = Handle;
    Handle = HandleTraits::GetInvalid();
    return t;
  }

  ScopedHandle &operator=(handle_type h) {
    if (HandleTraits::IsValid(Handle))
      HandleTraits::Close(Handle);
    Handle = h;
    return *this;
  }

  // True if Handle is valid.
  explicit operator bool() const {
    return HandleTraits::IsValid(Handle) ? true : false;
  }

  operator handle_type() const {
    return Handle;
  }
};

struct CommonHandleTraits {
  typedef HANDLE handle_type;

  static handle_type GetInvalid() {
    return INVALID_HANDLE_VALUE;
  }

  static void Close(handle_type h) {
    ::CloseHandle(h);
  }

  static bool IsValid(handle_type h) {
    return h != GetInvalid();
  }
};

struct JobHandleTraits : CommonHandleTraits {
  static handle_type GetInvalid() {
    return NULL;
  }
};

struct CryptContextTraits : CommonHandleTraits {
  typedef HCRYPTPROV handle_type;

  static handle_type GetInvalid() {
    return 0;
  }

  static void Close(handle_type h) {
    ::CryptReleaseContext(h, 0);
  }

  static bool IsValid(handle_type h) {
    return h != GetInvalid();
  }
};

struct FindHandleTraits : CommonHandleTraits {
  static void Close(handle_type h) {
    ::FindClose(h);
  }
};

struct FileHandleTraits : CommonHandleTraits {};

typedef ScopedHandle<CommonHandleTraits> ScopedCommonHandle;
typedef ScopedHandle<FileHandleTraits>   ScopedFileHandle;
typedef ScopedHandle<CryptContextTraits> ScopedCryptContext;
typedef ScopedHandle<FindHandleTraits>   ScopedFindHandle;
typedef ScopedHandle<JobHandleTraits>    ScopedJobHandle;

inline bool MakeErrMsg(std::string* ErrMsg, const std::string& prefix) {
  if (!ErrMsg)
    return true;
  TCHAR *buffer = NULL;
  DWORD LastError = GetLastError();
  DWORD R = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_MAX_WIDTH_MASK,
                          NULL, LastError, 0, (LPTSTR)&buffer, 1, NULL);
  if (R)
    *ErrMsg = prefix + ": " + buffer;
  else
    *ErrMsg = prefix + ": Unknown error";
  *ErrMsg += " (0x" + llvm::utohexstr(LastError) + ")";

  LocalFree(buffer);
  return R != 0;
}

namespace llvm {
namespace sys {
namespace path {
std::error_code widenPath(const Twine &Path8,
                          SmallVectorImpl<wchar_t> &Path16);
} // end namespace path
namespace windows {
std::error_code UTF8ToUTF16(StringRef utf8, SmallVectorImpl<wchar_t> &utf16);
std::error_code UTF16ToUTF8(const wchar_t *utf16, size_t utf16_len,
                            SmallVectorImpl<char> &utf8);
/// Convert from UTF16 to the current code page used in the system
std::error_code UTF16ToCurCP(const wchar_t *utf16, size_t utf16_len,
                             SmallVectorImpl<char> &utf8);
} // end namespace windows
} // end namespace sys
} // end namespace llvm.

namespace {

using namespace llvm;
using namespace llvm::sys;

/// ArgNeedsQuotes - Check whether argument needs to be quoted when calling
/// CreateProcess.
static bool ArgNeedsQuotes(const char *Str) {
  return Str[0] == '\0' || strpbrk(Str, "\t \"&\'()*<>\\`^|") != 0;
}

/// CountPrecedingBackslashes - Returns the number of backslashes preceding Cur
/// in the C string Start.
static unsigned int CountPrecedingBackslashes(const char *Start,
                                              const char *Cur) {
  unsigned int Count = 0;
  --Cur;
  while (Cur >= Start && *Cur == '\\') {
    ++Count;
    --Cur;
  }
  return Count;
}

/// EscapePrecedingEscapes - Append a backslash to Dst for every backslash
/// preceding Cur in the Start string.  Assumes Dst has enough space.
static char *EscapePrecedingEscapes(char *Dst, const char *Start,
                                    const char *Cur) {
  unsigned PrecedingEscapes = CountPrecedingBackslashes(Start, Cur);
  while (PrecedingEscapes > 0) {
    *Dst++ = '\\';
    --PrecedingEscapes;
  }
  return Dst;
}

/// ArgLenWithQuotes - Check whether argument needs to be quoted when calling
/// CreateProcess and returns length of quoted arg with escaped quotes
static unsigned int ArgLenWithQuotes(const char *Str) {
  const char *Start = Str;
  bool Quoted = ArgNeedsQuotes(Str);
  unsigned int len = Quoted ? 2 : 0;

  while (*Str != '\0') {
    if (*Str == '\"') {
      // We need to add a backslash, but ensure that it isn't escaped.
      unsigned PrecedingEscapes = CountPrecedingBackslashes(Start, Str);
      len += PrecedingEscapes + 1;
    }
    // Note that we *don't* need to escape runs of backslashes that don't
    // precede a double quote!  See MSDN:
    // http://msdn.microsoft.com/en-us/library/17w5ykft%28v=vs.85%29.aspx

    ++len;
    ++Str;
  }

  if (Quoted) {
    // Make sure the closing quote doesn't get escaped by a trailing backslash.
    unsigned PrecedingEscapes = CountPrecedingBackslashes(Start, Str);
    len += PrecedingEscapes + 1;
  }

  return len;
}

static std::unique_ptr<char[]> flattenArgs(const char **args) {
  // First, determine the length of the command line.
  unsigned len = 0;
  for (unsigned i = 0; args[i]; i++) {
    len += ArgLenWithQuotes(args[i]) + 1;
  }

  // Now build the command line.
  std::unique_ptr<char[]> command(new char[len+1]);
  char *p = command.get();

  for (unsigned i = 0; args[i]; i++) {
    const char *arg = args[i];
    const char *start = arg;

    bool needsQuoting = ArgNeedsQuotes(arg);
    if (needsQuoting)
      *p++ = '"';

    while (*arg != '\0') {
      if (*arg == '\"') {
        // Escape all preceding escapes (if any), and then escape the quote.
        p = EscapePrecedingEscapes(p, start, arg);
        *p++ = '\\';
      }

      *p++ = *arg++;
    }

    if (needsQuoting) {
      // Make sure our quote doesn't get escaped by a trailing backslash.
      p = EscapePrecedingEscapes(p, start, arg);
      *p++ = '"';
    }
    *p++ = ' ';
  }

  *p = 0;
  return command;
}

static bool HideConsoleExecute(ProcessInfo &PI, StringRef Program,
                               const char **args, const char **envp,
                               std::string *ErrMsg)
{
  if (!sys::fs::can_execute(Program)) {
    if (ErrMsg)
      *ErrMsg = "program not executable";
    return false;
  }

  // can_execute may succeed by looking at Program + ".exe". CreateProcessW
  // will implicitly add the .exe if we provide a command line without an
  // executable path, but since we use an explicit executable, we have to add
  // ".exe" ourselves.
  SmallString<64> ProgramStorage;
  if (!sys::fs::exists(Program))
    Program = Twine(Program + ".exe").toStringRef(ProgramStorage);

  // Windows wants a command line, not an array of args, to pass to the new
  // process.  We have to concatenate them all, while quoting the args that
  // have embedded spaces (or are empty).
  std::unique_ptr<char[]> command = flattenArgs(args);

  // The pointer to the environment block for the new process.
  std::vector<wchar_t> EnvBlock;

  if (envp) {
    // An environment block consists of a null-terminated block of
    // null-terminated strings. Convert the array of environment variables to
    // an environment block by concatenating them.
    for (unsigned i = 0; envp[i]; ++i) {
      SmallVector<wchar_t, MAX_PATH> EnvString;
      if (std::error_code ec = windows::UTF8ToUTF16(envp[i], EnvString)) {
        SetLastError(ec.value());
        MakeErrMsg(ErrMsg, "Unable to convert environment variable to UTF-16");
        return false;
      }

      EnvBlock.insert(EnvBlock.end(), EnvString.begin(), EnvString.end());
      EnvBlock.push_back(0);
    }
    EnvBlock.push_back(0);
  }

  // Create a child process.
  STARTUPINFOW si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.hStdInput = INVALID_HANDLE_VALUE;
  si.hStdOutput = INVALID_HANDLE_VALUE;
  si.hStdError = INVALID_HANDLE_VALUE;

  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));

  fflush(stdout);
  fflush(stderr);

  SmallVector<wchar_t, MAX_PATH> ProgramUtf16;
  if (std::error_code ec = path::widenPath(Program, ProgramUtf16)) {
    SetLastError(ec.value());
    MakeErrMsg(ErrMsg,
               std::string("Unable to convert application name to UTF-16"));
    return false;
  }

  SmallVector<wchar_t, MAX_PATH> CommandUtf16;
  if (std::error_code ec = windows::UTF8ToUTF16(command.get(), CommandUtf16)) {
    SetLastError(ec.value());
    MakeErrMsg(ErrMsg,
               std::string("Unable to convert command-line to UTF-16"));
    return false;
  }

  BOOL rc = CreateProcessW(ProgramUtf16.data(), CommandUtf16.data(), 0, 0,
                           TRUE,
                           CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                           EnvBlock.empty() ? 0 : EnvBlock.data(), 0, &si,
                           &pi);
  DWORD err = GetLastError();

  // Regardless of whether the process got created or not, we are done with
  // the handles we created for it to inherit.
  CloseHandle(si.hStdInput);
  CloseHandle(si.hStdOutput);
  CloseHandle(si.hStdError);

  // Now return an error if the process didn't get created.
  if (!rc) {
    SetLastError(err);
    MakeErrMsg(ErrMsg, std::string("Couldn't execute program '") +
               Program.str() + "'");
    return false;
  }

  PI.Pid = pi.dwProcessId;
  PI.ProcessHandle = pi.hProcess;

  // Make sure these get closed no matter what.
  ScopedCommonHandle hThread(pi.hThread);

  // Assign the process to a job if a memory limit is defined.
  ScopedJobHandle hJob;

  return true;
}

}

int HiddenExecuteAndWait(llvm::StringRef Program,
                         const char **Args,
                         const char **EnvPtr,
                         std::string *ErrorMsg,
                         bool *ExecFailed)
{
  ProcessInfo PI;
  if (HideConsoleExecute(PI, Program, Args, EnvPtr, ErrorMsg)) {
    if (ExecFailed)
      *ExecFailed = false;
    ProcessInfo Result = Wait(
      PI, 0, /*WaitUntilTerminates=*/true, ErrorMsg);
    return Result.ReturnCode;
  }

  if (ExecFailed)
    *ExecFailed = true;

  return -1;
}
