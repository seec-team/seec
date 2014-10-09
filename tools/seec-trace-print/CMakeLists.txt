add_executable(seec-print
 ClangMapped.cpp
 main.cpp
 OnlinePythonTutor.cpp
 Unmapped.cpp
)

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (LLVM)
#--------------------------------------------------------------------------------
execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles all-targets codegen linker bitreader bitwriter asmparser selectiondag ipo instrumentation core target irreader option
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (ICU)
#--------------------------------------------------------------------------------
execute_process(
 COMMAND ${ICU_INSTALL}/bin/icu-config --prefix=${ICU_INSTALL} --ldflags-libsonly
 OUTPUT_VARIABLE REQ_ICU_LIBRARIES
)
string(STRIP ${REQ_ICU_LIBRARIES} REQ_ICU_LIBRARIES)
string(REPLACE "-l" "" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
string(REPLACE " " ";" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (WX)
#--------------------------------------------------------------------------------
execute_process(
 COMMAND ${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --libs base
 OUTPUT_VARIABLE REQ_WX_LIBRARIES
)
string(STRIP ${REQ_WX_LIBRARIES} REQ_WX_LIBRARIES)

target_link_libraries(seec-print
 # SeeC libraries
 SeeCClang
 SeeCClangMappedTrace
 SeeCTrace
 SeeCRuntimeErrors
 SeeCICU
 SeeCUtil

 # wxWidgets libraries
 ${REQ_WX_LIBRARIES}

 # Clang libraries
 ${LLVM_INSTALL}/lib/libclangFrontend.a
 ${LLVM_INSTALL}/lib/libclangDriver.a
 ${LLVM_INSTALL}/lib/libclangSerialization.a
 ${LLVM_INSTALL}/lib/libclangParse.a
 ${LLVM_INSTALL}/lib/libclangSema.a
 ${LLVM_INSTALL}/lib/libclangEdit.a
 ${LLVM_INSTALL}/lib/libclangAnalysis.a
 ${LLVM_INSTALL}/lib/libclangLex.a
 ${LLVM_INSTALL}/lib/libclangAST.a
 ${LLVM_INSTALL}/lib/libclangBasic.a

 # LLVM libraries
 ${REQ_LLVM_LIBRARIES}

 # ICU libraries
 ${REQ_ICU_LIBRARIES}

 ${LLVM_LIB_DEPS}
)

INSTALL(TARGETS seec-print
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

