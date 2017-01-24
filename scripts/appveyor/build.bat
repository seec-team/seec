@echo on

cd "%APPVEYOR_BUILD_FOLDER%"

IF %COMPILER%==msys2 (
  cd "C:\projects"
  mkdir seec_build
  cd "C:\projects\seec_build"
  bash "%APPVEYOR_BUILD_FOLDER%\scripts\appveyor\build.sh"
)
