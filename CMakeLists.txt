cmake_minimum_required(VERSION 3.20)

project(coroutines VERSION 1.0 LANGUAGES CXX)

add_executable(fib fib.cpp)
target_compile_features(fib PUBLIC cxx_std_26)

