cmake_minimum_required(VERSION 3.1)
add_compile_options(-std=c++14)
add_executable (simpleServer_test main.cpp)
add_executable (svctest svctest.cpp)
add_executable (srvtest srvtest.cpp)
add_executable (wscli wscli.cpp)      
target_link_libraries (simpleServer_test LINK_PUBLIC simpleServer ssl crypto pthread) 
target_link_libraries (svctest LINK_PUBLIC simpleServer pthread)
target_link_libraries (srvtest LINK_PUBLIC simpleServer pthread)
target_link_libraries (wscli LINK_PUBLIC simpleServer ssl crypto pthread) 
