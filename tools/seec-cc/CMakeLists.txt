add_executable(seec-cc
               main.cpp
               cc1_main.cpp
               SeeCExecuteCompilerInvocation.cpp)

# Determine the LLVM libraries that we need
llvm_map_components_to_libnames(REQ_LLVM_LIBRARIES ${LLVM_TARGETS_TO_BUILD} analysis codegen core ipa ipo instcombine instrumentation mc mcparser objcarcopts option scalaropts support transformutils vectorize asmparser bitreader bitwriter irreader linker profiledata)

message(STATUS "seec-cc llvm libs: ${REQ_LLVM_LIBRARIES}")

# Determine ICU libraries that we need
execute_process(
 COMMAND ${ICU_INSTALL}/bin/icu-config --prefix=${ICU_INSTALL} --ldflags-libsonly
 OUTPUT_VARIABLE REQ_ICU_LIBRARIES
)
string(STRIP ${REQ_ICU_LIBRARIES} REQ_ICU_LIBRARIES)
string(REPLACE "-l" "" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})
string(REPLACE " " ";" REQ_ICU_LIBRARIES ${REQ_ICU_LIBRARIES})

target_link_libraries(seec-cc
 # SeeC libraries
 SeeCClang
 SeeCBreakConstantGEPs
 SeeCICU

 # Clang libraries
 ${LLVM_INSTALL}/lib/libclangFrontendTool.a
 ${LLVM_INSTALL}/lib/libclangFrontend.a
 ${LLVM_INSTALL}/lib/libclangDriver.a
 ${LLVM_INSTALL}/lib/libclangSerialization.a
 ${LLVM_INSTALL}/lib/libclangCodeGen.a
 ${LLVM_INSTALL}/lib/libclangParse.a
 ${LLVM_INSTALL}/lib/libclangSema.a
 ${LLVM_INSTALL}/lib/libclangRewriteFrontend.a
 ${LLVM_INSTALL}/lib/libclangRewrite.a
 ${LLVM_INSTALL}/lib/libclangAnalysis.a
 ${LLVM_INSTALL}/lib/libclangEdit.a
 ${LLVM_INSTALL}/lib/libclangAST.a
 ${LLVM_INSTALL}/lib/libclangBasic.a
 ${LLVM_INSTALL}/lib/libclangLex.a

 # LLVM libraries
 ${REQ_LLVM_LIBRARIES}

 # ICU libraries
 ${REQ_ICU_LIBRARIES}

 ${LLVM_LIB_DEPS}
)

INSTALL(TARGETS seec-cc
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

