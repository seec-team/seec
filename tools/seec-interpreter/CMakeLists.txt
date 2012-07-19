add_executable(seec-interpreter main.cpp)

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles mcjit jit interpreter nativecodegen bitreader bitwriter asmparser selectiondag instrumentation core target
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

target_link_libraries(seec-interpreter
 SeeCRecordInternal
 SeeCExecutionListener
 SeeCTraceWriter
 SeeCTrace
 SeeCUtil
 SeeCClang
 ${LLVM_BUILD}/lib/libclangCodeGen.a
 ${LLVM_BUILD}/lib/libclangFrontendTool.a
 ${LLVM_BUILD}/lib/libclangFrontend.a
 ${LLVM_BUILD}/lib/libclangParse.a
 ${LLVM_BUILD}/lib/libclangSema.a
 ${LLVM_BUILD}/lib/libclangAnalysis.a
 ${LLVM_BUILD}/lib/libclangAST.a
 ${LLVM_BUILD}/lib/libclangLex.a
 ${LLVM_BUILD}/lib/libclangBasic.a
 ${LLVM_BUILD}/lib/libclangDriver.a
 ${LLVM_BUILD}/lib/libclangSerialization.a
 ${REQ_LLVM_LIBRARIES}
)

INSTALL(TARGETS seec-interpreter
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

