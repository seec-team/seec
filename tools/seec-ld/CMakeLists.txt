add_executable(seec-ld seec-ld.cpp)

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles all
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

target_link_libraries(seec-ld
 SeeCBreakConstantGEPs
 SeeCRecordExternal
 ${REQ_LLVM_LIBRARIES}
 ${LLVM_LIB_DEPS}
)

INSTALL(TARGETS seec-ld
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
