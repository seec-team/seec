add_executable(seec-ld seec-ld.cpp)

llvm_map_components_to_libnames(REQ_LLVM_LIBRARIES ${LLVM_TARGETS_TO_BUILD} bitreader bitwriter irreader linker)

target_link_libraries(seec-ld
 SeeCRecordExternal
 SeeCUtil
 ${REQ_LLVM_LIBRARIES}
 ${LLVM_LIB_DEPS}
)

INSTALL(TARGETS seec-ld
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/man/man1/seec-ld.1
        DESTINATION ${MAN_INSTALL_DIR}/man1)
