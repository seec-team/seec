add_executable(seec-print
 ClangMapped.cpp
 main.cpp
 OnlinePythonTutor.cpp
 Unmapped.cpp
)

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (LLVM)
#--------------------------------------------------------------------------------
llvm_map_components_to_libnames(REQ_LLVM_LIBRARIES ${LLVM_TARGETS_TO_BUILD} codegen linker bitreader bitwriter asmparser selectiondag ipo instrumentation core target irreader option)

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (ICU)
#--------------------------------------------------------------------------------
EXEC_PROGRAM(sh
 ARGS "${ICU_INSTALL}/bin/icu-config --noverify --prefix=${ICU_INSTALL} --ldflags-libsonly"
 OUTPUT_VARIABLE REQ_ICU_LIBRARIES
)
string(STRIP ${REQ_ICU_LIBRARIES} REQ_ICU_LIBRARIES)
string(REPLACE "-l" "" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
string(REPLACE " " ";" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
message("seec-print icu libs: ${REQ_ICU_LIBRARIES}")

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (WX)
#--------------------------------------------------------------------------------
EXEC_PROGRAM(sh
 ARGS "${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --libs base xml"
 OUTPUT_VARIABLE REQ_WX_LIBRARIES
)
string(STRIP ${REQ_WX_LIBRARIES} REQ_WX_LIBRARIES)

target_link_libraries(seec-print
 # SeeC libraries
 SeeCClang
 SeeCClangMappedTrace
 SeeCTraceReader
 SeeCTrace
 SeeCRuntimeErrors
 SeeCICU
 SeeCUtil
 SeeCwxWidgets

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

 ${REQ_ICU_LIBRARIES}
)

INSTALL(TARGETS seec-print
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

