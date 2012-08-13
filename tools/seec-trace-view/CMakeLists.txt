# wxWidgets for Cocoa uses RTTI, so we need to enable it for this tool.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(SEEC_TRACE_VIEW_SRCS
  main.cpp
  MallocViewer.cpp
  OpenTrace.cpp
  ProcessTimeControl.cpp
  SourceViewer.cpp
  StateViewer.cpp
  ThreadTimeControl.cpp
  TraceViewerFrame.cpp
  WelcomeFrame.cpp
)

#--------------------------------------------------------------------------------
# Create the executable.
#--------------------------------------------------------------------------------
add_executable(seec-trace-view MACOSX_BUNDLE
  ${SEEC_TRACE_VIEW_SRCS}
  )

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against.
#--------------------------------------------------------------------------------
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

#--------------------------------------------------------------------------------
# Installation settings.
#--------------------------------------------------------------------------------
INSTALL(TARGETS seec-trace-view
  BUNDLE DESTINATION ${BUNDLE_PREFIX} COMPONENT Runtime
  RUNTIME DESTINATION bin COMPONENT Runtime
)
