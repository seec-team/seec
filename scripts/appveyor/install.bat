@echo off

cd %APPVEYOR_BUILD_FOLDER%

echo SeeC %SEEC_VERSION_STRING%
echo LLVM %SEEC_LLVM_VERSION%
echo wxWidgets %WXWIDGETS_REPOSITORY% %WXWIDGETS_VERSION_TAG%
echo Compiler: %COMPILER%
echo Platform: %PLATFORM%
echo MSYS2 directory: %MSYS2_DIR%
echo MSYS2 system: %MSYSTEM%

REM Get SeeC repo's submodules
git submodule update --init --recursive

REM Create a writeable TMPDIR
mkdir %APPVEYOR_BUILD_FOLDER%\tmp
set TMPDIR=%APPVEYOR_BUILD_FOLDER%\tmp

IF %COMPILER%==msys2 (
  @echo on
  SET "PATH=C:\%MSYS2_DIR%\%MSYSTEM%\bin;C:\%MSYS2_DIR%\usr\bin;%PATH%"

  pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-icu mingw-w64-x86_64-curl mingw-w64-x86_64-webkitgtk3

  REM download and extract llvm, seec-clang build artifacts
  mkdir c:\projects\deps
  cd c:\projects\deps
  appveyor DownloadFile https://ci.appveyor.com/api/projects/mheinsen/llvm-with-seec-clang/artifacts/llvm_install_%SEEC_LLVM_VERSION%.zip?branch=%SEEC_LLVM_BRANCH%
  7z x llvm_install_%SEEC_LLVM_VERSION%.zip -y
  appveyor DownloadFile https://ci.appveyor.com/api/projects/mheinsen/seec-clang/artifacts/seec_clang_install_%SEEC_LLVM_VERSION%.zip?branch=%SEEC_CLANG_BRANCH%
  7z x seec_clang_install_%SEEC_LLVM_VERSION%.zip -y

  REM causes problems linking libclang:
  del C:\msys64\usr\lib\libdl.a

  REM download and build wxWidgets (if not cached)
  IF NOT EXIST c:\projects\depinstall (
    mkdir c:\projects\depinstall
    mkdir c:\projects\depsrc
    mkdir c:\projects\depbuild
    cd c:\projects\depsrc
    git clone %WXWIDGETS_REPOSITORY% wxWidgets
    cd wxWidgets
    git checkout %WXWIDGETS_VERSION_TAG%
    cd c:\projects\depbuild
    mkdir wxWidgets
    cd wxWidgets
    bash -lc "c:/projects/depsrc/wxWidgets/configure --build=x86_64-w64-mingw32 --disable-shared --enable-debug --enable-webview --disable-precomp-headers --enable-no_rtti --prefix=c:/projects/depinstall CXXFLAGS=\"-std=gnu++11 -fvisibility-inlines-hidden\""
    bash -lc "make"
    bash -lc "make install"
  )
)
