cd "/c/projects/seec_build"
"C:\\Program Files (x86)\\cmake\\bin\\cmake.exe" -G"MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=c:/projects/deps/lib/cmake/llvm -DWX_INSTALL=c:/projects/depinstall -DCMAKE_INSTALL_PREFIX="C:\\projects\\seec_install" -DWX_TOOLCHAIN="msw-unicode-static-3.0" -DICU_INSTALL="/c/$MSYS2_DIR/mingw64/" $APPVEYOR_BUILD_FOLDER
make
make install
cd "/c/projects/seec_install/Program Files (x86)/seec/bin/"
python "$APPVEYOR_BUILD_FOLDER/scripts/appveyor/copydeps.py"
cd $APPVEYOR_BUILD_FOLDER
7z a "seec_install_$SEEC_VERSION_STRING.zip" "C:\\projects\\seec_install\\*"
