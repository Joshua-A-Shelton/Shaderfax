cmake_minimum_required(VERSION 3.30)
project(Shaderfax)

set(CMAKE_CXX_STANDARD 20)
include(FetchContent)
fetchcontent_declare(
        slang
        GIT_REPOSITORY https://github.com/shader-slang/slang
        GIT_TAG origin/master
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE
)
fetchcontent_makeavailable(slang)
find_package(slang REQUIRED)

find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(Shaderfax src/main.cpp)
target_link_libraries(Shaderfax PUBLIC slang Boost::program_options)
