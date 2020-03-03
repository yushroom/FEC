cmake_minimum_required(VERSION 3.11.0)

add_executable(hlslreflect main.cpp)
target_compile_features(hlslreflect PUBLIC cxx_std_17)
target_link_libraries(hlslreflect fmt d3dcompiler)
target_include_directories(hlslreflect PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/rapidjson/include")