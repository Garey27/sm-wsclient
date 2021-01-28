cmake_minimum_required(VERSION 3.8)

project(websocket_client)

find_path(BOOST_ASIO_INCLUDE_DIRS "boost/asio.hpp")
find_path(BOOST_BEAST_INCLUDE_DIRS "boost/beast.hpp")
find_path(BOOST_FUNCTIONAL_INCLUDE_DIRS "boost/functional.hpp")
find_path(BOOST_CONTAINER_HASH_INCLUDE_DIRS "boost/container_hash/detail/float_functions.hpp")

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

# CMake configuration #
set(OUTPUT_NAME "wsclient" CACHE STRING "Output name of the extension")
string(REGEX REPLACE " " "" OUTPUT_NAME "${OUTPUT_NAME}")

if(OUTPUT_NAME STREQUAL "")
    message(FATAL_ERROR "OUTPUT_NAME must be configured")
endif()

set(SM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dep/sourcemod")
set(SP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dep/sourcemod/sourcepawn")

include("cmake/sourcemod.cmake")

#add_subdirectory(dep/websocketpp)

# Compiler specific jobs #
if(MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Arch must be Win32")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /W3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

# Add header and source files #
include_directories("src")

set(CPP_FILES 
"src/extension.cpp"
"src/websockets_natives.cpp"
)
set(HPP_FILES 
"src/websockets_session.hpp"
)


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CPP_FILES} ${HPP_FILES})

# Add a executable file #
add_extension(${OUTPUT_NAME} ${CPP_FILES} ${HPP_FILES})
if(MSVC)
target_link_libraries(${OUTPUT_NAME}     
    OpenSSL::SSL
    OpenSSL::Crypto
    Crypt32.lib
)
else()
target_link_libraries(${OUTPUT_NAME}     
    OpenSSL::SSL
    OpenSSL::Crypto
)
endif()
target_include_directories(${OUTPUT_NAME} PUBLIC "dep/certify/include")
#target_include_directories(${OUTPUT_NAME} PUBLIC "dep/websocketpp")
target_include_directories(${OUTPUT_NAME} PUBLIC ${BOOST_ASIO_INCLUDE_DIRS})
target_include_directories(${OUTPUT_NAME} PUBLIC ${BOOST_BEAST_INCLUDE_DIRS})
target_include_directories(${OUTPUT_NAME} PUBLIC ${BOOST_FUNCTIONAL_INCLUDE_DIRS})
target_include_directories(${OUTPUT_NAME} PUBLIC ${BOOST_CONTAINER_HASH_INCLUDE_DIRS})