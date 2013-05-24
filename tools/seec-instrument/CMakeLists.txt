add_executable(seec-instrument seec-instrument.cpp)

execute_process(
 COMMAND ${LLVM_INSTALL}/bin/llvm-config --libfiles bitreader bitwriter irreader asmparser instrumentation core target
 OUTPUT_VARIABLE REQ_LLVM_LIBRARIES
)
string(STRIP ${REQ_LLVM_LIBRARIES} REQ_LLVM_LIBRARIES)
string(REPLACE " " ";" REQ_LLVM_LIBRARIES ${REQ_LLVM_LIBRARIES})

target_link_libraries(seec-instrument
 SeeCRecordExternal
 ${REQ_LLVM_LIBRARIES}
)

INSTALL(TARGETS seec-instrument
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

