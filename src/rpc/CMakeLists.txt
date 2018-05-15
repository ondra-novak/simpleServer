cmake_minimum_required(VERSION 2.8) 
add_compile_options(-std=c++14 -fPIC)
file(GLOB simpleRpcServer_SRC "*.cpp" )
file(GLOB simpleRpcServer_HDR "*.h" "*.tcc" )
add_library (simpleRpcServer ${simpleRpcServer_SRC})

 