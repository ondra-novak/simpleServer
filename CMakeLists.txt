cmake_minimum_required(VERSION 2.8)
project (simpleServer)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

include_directories(BEFORE src)


add_subdirectory (src/simpleServer)
add_subdirectory (src/tests)
  # The 'test' target runs all but the future tests
  cmake_policy(PUSH)
  if(POLICY CMP0037)
    cmake_policy(SET CMP0037 OLD)
  endif()
add_compile_options(-std=c++11)
add_custom_target( test bin/simpleServer_test DEPENDS bin/simpleServer_test)
cmake_policy(POP)  

         