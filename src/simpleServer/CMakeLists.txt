cmake_minimum_required(VERSION 2.8) 
add_compile_options(-std=c++11 -fPIC)
file(GLOB simpleServer_SRC "*.cpp" "linux/*.cpp")
file(GLOB simpleServer_HDR "*.h" "*.tcc" "linux/*.h" "linux/*.tcc")
add_library (simpleServer ${simpleServer_SRC})
# target_include_directories (simpleServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS simpleServer
        DESTINATION "lib"
        ) 
INSTALL(FILES ${simpleServer_HDR}
        DESTINATION "include/simpleServer" 
        )             