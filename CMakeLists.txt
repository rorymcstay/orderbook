cmake_minimum_required(VERSION 3.19.2)
project(OrderBook)

add_compile_options(-DBOOST_LOG_DYN_LINK 
    -Wall -Wextra -Wpedantic  
    -std=c++17
    # for clang
    #-Wno-error=unused-command-line-argument
    #-Wno-unused-command-line-argument
    #-Wgcc-compat
)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package (Threads)


find_package(GTest)
if(${GTest_FOUND} EQUAL "FALSE")
    message(FATAL "Gtest not found!")
endif(${GTest_FOUND} EQUAL "FALSE")

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(book src/main.cpp)

add_library(orderbook
        src/OrderBook.cpp
        src/Domain.cpp
        src/ExecWriter.cpp)

add_executable(test_orderbook 
    test/fwk/TestEnv.cpp
    test/main.cpp
)
target_link_libraries(test_orderbook ${GTEST_LIBRARIES} pthread orderbook)

target_link_libraries(book orderbook pthread)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
message(STATUS "BUILD_TYPE=${CMAKE_BUILD_TYPE}")


install(TARGETS
    orderbook
    book
    test_orderbook

    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

