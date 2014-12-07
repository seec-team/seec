# Disable -Wcast-qual because wxWidgets raises many warnings.
if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${WX_CXX_FLAGS} -Wno-cast-qual")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${WX_CXX_FLAGS} -Wno-cast-qual")
else ()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WX_C_FLAGS} -Wno-cast-qual")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WX_CXX_FLAGS} -Wno-cast-qual")
endif ()

# wxWidgets for Cocoa uses RTTI, so we need to enable it for this tool.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(SEEC_TRACE_VIEW_HEADERS
  ActionRecord.hpp
  ActionRecordSettings.hpp
  ActionReplay.hpp
  AugmentationSettings.hpp
  CommonMenus.hpp
  ExplanationViewer.hpp
  FunctionStateViewer.hpp
  InternationalizedButton.hpp
  LocaleSettings.hpp
  MallocViewer.hpp
  NotifyContext.hpp
  OpenTrace.hpp
  Preferences.hpp
  ProcessMoveEvent.hpp
  RuntimeValueLookup.hpp
  SourceViewer.hpp
  SourceViewerSettings.hpp
  SourceViewerSettingsTypes.def
  StateAccessToken.hpp
  StateEvaluationTree.hpp
  StateGraphViewer.hpp
  StmtTooltip.hpp
  StreamStatePanel.hpp
  ThreadMoveEvent.hpp
  ThreadStateViewer.hpp
  ThreadTimeControl.hpp
  TraceViewerApp.hpp
  TraceViewerFrame.hpp
  TracingPreferences.hpp
  ValueFormat.hpp
  WelcomeFrame.hpp
  )

set(SEEC_TRACE_VIEW_SRCS
  ActionRecord.cpp
  ActionRecordSettings.cpp
  ActionReplay.cpp
  AugmentationSettings.cpp
  CommonMenus.cpp
  ExplanationViewer.cpp
  FunctionStateViewer.cpp
  InternationalizedButton.cpp
  LocaleSettings.cpp
  MallocViewer.cpp
  NotifyContext.cpp
  OpenTrace.cpp
  Preferences.cpp
  ProcessMoveEvent.cpp
  RuntimeValueLookup.cpp
  SourceViewer.cpp
  SourceViewerSettings.cpp
  StateEvaluationTree.cpp
  StateGraphViewer.cpp
  StmtTooltip.cpp
  StreamStatePanel.cpp
  ThreadMoveEvent.cpp
  ThreadStateViewer.cpp
  ThreadTimeControl.cpp
  TraceViewerApp.cpp
  TraceViewerFrame.cpp
  TracingPreferences.cpp
  ValueFormat.cpp
  WelcomeFrame.cpp
  )

#--------------------------------------------------------------------------------
# Create the executable.
#--------------------------------------------------------------------------------
add_executable(seec-view MACOSX_BUNDLE ${SEEC_TRACE_VIEW_HEADERS} ${SEEC_TRACE_VIEW_SRCS})

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
 COMMAND ${WX_CONFIG_BIN} --prefix=${WX_INSTALL} --libs html stc core base aui adv webview xml net
 OUTPUT_VARIABLE REQ_WX_LIBRARIES
)
string(STRIP ${REQ_WX_LIBRARIES} REQ_WX_LIBRARIES)
# string(REPLACE "-l" "" REQ_WX_LIBRARIES ${REQ_WX_LIBRARIES})
# string(REPLACE " " ";" REQ_WX_LIBRARIES ${REQ_WX_LIBRARIES})

#--------------------------------------------------------------------------------
# Link.
#--------------------------------------------------------------------------------
target_link_libraries(seec-view
 # wxWidgets libraries
 ${REQ_WX_LIBRARIES}
 
 # SeeC libraries
 SeeCClangEPV
 SeeCClangMappedTrace
 SeeCClang
 SeeCTraceReader
 SeeCTrace
 SeeCRuntimeErrors
 SeeCICU
 SeeCUtil
 SeeCwxWidgets
 SeeCwxWidgetsGUI

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

 # cURL
 curl
)

#--------------------------------------------------------------------------------
# Bundle options for Mac OS X
#--------------------------------------------------------------------------------
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_target_properties(seec-view PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Bundle/Info.plist.in)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

#--------------------------------------------------------------------------------
# Installation settings.
#--------------------------------------------------------------------------------
INSTALL(TARGETS seec-view
  BUNDLE DESTINATION ${BUNDLE_PREFIX} COMPONENT Runtime
  RUNTIME DESTINATION bin COMPONENT Runtime
)

# Copy all of our resources into the bundle's Resources directory.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/resources/clang DESTINATION ${BUNDLE_PREFIX}/seec-view.app/Resources)
  
  get_property(RES_FILES GLOBAL PROPERTY SEEC_RESOURCE_FILES)
  foreach(RES_FILE ${RES_FILES})
    INSTALL(FILES ${RES_FILE} DESTINATION ${BUNDLE_PREFIX}/seec-view.app/Resources)
  endforeach(RES_FILE)

  INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Bundle/SeeC.icns DESTINATION ${BUNDLE_PREFIX}/seec-view.app/Contents/Resources/)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Linux-specific installation options.
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/XDesktop/seec.desktop DESTINATION share/applications)
  INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/XDesktop/seec.xml DESTINATION share/mime/packages)
  # INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/XDesktop/Icons/48x48/seec.png DESTINATION share/icons/hicolor/48x48/apps)
  INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/XDesktop/Icons/256x256/seec.png DESTINATION share/pixmaps)

  FIND_PROGRAM(PROG_UPDATE_MIME_DATABASE update-mime-database)
  IF (PROG_UPDATE_MIME_DATABASE)
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND ${PROG_UPDATE_MIME_DATABASE} \"${CMAKE_INSTALL_PREFIX}/share/mime\")")
  ENDIF (PROG_UPDATE_MIME_DATABASE)

  FIND_PROGRAM(PROG_UPDATE_DESKTOP_DATABASE update-desktop-database)
  IF (PROG_UPDATE_DESKTOP_DATABASE)
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND ${PROG_UPDATE_DESKTOP_DATABASE})")
  ENDIF (PROG_UPDATE_DESKTOP_DATABASE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

