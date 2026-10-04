cmake_minimum_required(VERSION 3.16)
project(cpp_demo LANGUAGES CXX)

add_executable(cpp_demo src/main.cpp)
target_compile_features(cpp_demo PRIVATE cxx_std_17)