add_executable(seec-trace-view
  main.cpp
  OpenTrace.cpp
  SourceViewer.cpp
  StateViewer.cpp
  TraceViewerFrame.cpp
  )

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles nativecodegen codegen linker bitreader bitwriter asmparser selectiondag ipo instrumentation core target
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

execute_process(
 COMMAND ${ICU_INSTALL}/bin/icu-config --ldflags-libsonly
 OUTPUT_VARIABLE REQ_ICU_LIBRARIES
)
string(STRIP ${REQ_ICU_LIBRARIES} REQ_ICU_LIBRARIES)
string(REPLACE "-l" "" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
string(REPLACE " " ";" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})

target_link_libraries(seec-trace-view
 # SeeC libraries
 SeeCClang
 SeeCTrace
 SeeCRuntimeErrors
 SeeCUtil
 SeeCICU
 SeeCwxWidgets

 # Clang libraries
 ${LLVM_INSTALL}/lib/libclangRewrite.a
 ${LLVM_INSTALL}/lib/libclangCodeGen.a
 ${LLVM_INSTALL}/lib/libclangFrontendTool.a
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

 # wxWidgets libraries
 ${wxWidgets_LIBRARIES}
)

INSTALL(TARGETS seec-trace-view
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

