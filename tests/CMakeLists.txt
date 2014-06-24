cmake_minimum_required(VERSION 2.8)
project(seec-tests)
set(SEEC_INSTALL "/usr/local" CACHE PATH "Path to SeeC installation.")
enable_testing()
add_subdirectory(byval)
add_subdirectory(cstdlib)

