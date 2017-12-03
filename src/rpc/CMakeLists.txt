cmake_minimum_required(VERSION 2.8) 
add_compile_options(-std=c++11 -fPIC)
file(GLOB simpleRpcServer_SRC "*.cpp" "linux/*.cpp")
file(GLOB simpleRpcServer_HDR "*.h" "*.tcc" "linux/*.h" "linux/*.tcc")
add_library (simpleRpcServer ${simpleRpcServer_SRC})

