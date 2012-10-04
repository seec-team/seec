# wxWidgets for Cocoa uses RTTI, so we need to enable it for this tool.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(SEEC_TRACE_VIEW_HEADERS
  FunctionStateViewer.hpp
  MallocViewer.hpp
  OpenTrace.hpp
  ProcessTimeControl.hpp
  SourceViewer.hpp
  SourceViewerSettings.hpp
  SourceViewerSettingsTypes.def
  StateViewer.hpp
  ThreadStateViewer.hpp
  ThreadTimeControl.hpp
  TraceViewerApp.hpp
  TraceViewerFrame.hpp
  WelcomeFrame.hpp
  )

set(SEEC_TRACE_VIEW_SRCS
  FunctionStateViewer.cpp
  MallocViewer.cpp
  OpenTrace.cpp
  ProcessTimeControl.cpp
  SourceViewer.cpp
  SourceViewerSettings.cpp
  StateViewer.cpp
  ThreadStateViewer.cpp
  ThreadTimeControl.cpp
  TraceViewerApp.cpp
  TraceViewerFrame.cpp
  WelcomeFrame.cpp
  )

#--------------------------------------------------------------------------------
# Create the executable.
#--------------------------------------------------------------------------------
if (${CMAKE_GENERATOR} MATCHES "Xcode")
  add_executable(seec-trace-view MACOSX_BUNDLE ${SEEC_TRACE_VIEW_HEADERS} ${SEEC_TRACE_VIEW_SRCS})
else()
  add_executable(seec-trace-view MACOSX_BUNDLE ${SEEC_TRACE_VIEW_SRCS})
endif()

#--------------------------------------------------------------------------------
# Determine the libraries that we need to link against. (LLVM)
#--------------------------------------------------------------------------------
execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles all-targets codegen linker bitreader bitwriter asmparser selectiondag ipo instrumentation core target
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
 COMMAND ${WX_INSTALL}/bin/wx-config --prefix=${WX_INSTALL} --libs html stc core base aui adv
 OUTPUT_VARIABLE REQ_WX_LIBRARIES
)
string(STRIP ${REQ_WX_LIBRARIES} REQ_WX_LIBRARIES)
# string(REPLACE "-l" "" REQ_WX_LIBRARIES ${REQ_WX_LIBRARIES})
# string(REPLACE " " ";" REQ_WX_LIBRARIES ${REQ_WX_LIBRARIES})

#--------------------------------------------------------------------------------
# Link.
#--------------------------------------------------------------------------------
target_link_libraries(seec-trace-view
 # SeeC libraries
 SeeCClang
 SeeCTrace
 SeeCRuntimeErrors
 SeeCUtil
 SeeCICU
 SeeCwxWidgets

 # Clang libraries
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
 ${REQ_WX_LIBRARIES}
)

#--------------------------------------------------------------------------------
# Bundle options for Mac OS X
#--------------------------------------------------------------------------------
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_target_properties(seec-trace-view PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Bundle/Info.plist.in)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

#--------------------------------------------------------------------------------
# Installation settings.
#--------------------------------------------------------------------------------
INSTALL(TARGETS seec-trace-view
  BUNDLE DESTINATION ${BUNDLE_PREFIX} COMPONENT Runtime
  RUNTIME DESTINATION bin COMPONENT Runtime
)

# Copy all of our resources into the bundle's Resources directory.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/resources/clang DESTINATION ${BUNDLE_PREFIX}/seec-trace-view.app/Resources)
  
  get_property(RES_FILES GLOBAL PROPERTY SEEC_RESOURCE_FILES)
  foreach(RES_FILE ${RES_FILES})
    INSTALL(FILES ${RES_FILE} DESTINATION ${BUNDLE_PREFIX}/seec-trace-view.app/Resources)
  endforeach(RES_FILE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
