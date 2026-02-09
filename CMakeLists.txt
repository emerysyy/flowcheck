cmake_minimum_required(VERSION 3.15)
project(flowcheck VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/dns
    ${CMAKE_SOURCE_DIR}/flow
    ${CMAKE_SOURCE_DIR}/protocol
)

# DNS library sources
set(DNS_SOURCES
    dns/dns_cache.cpp
    dns/dns_message.cpp
)

# Flow library sources
set(FLOW_SOURCES
    flow/flow_defines.cpp
    flow/flow_detector.cpp
    flow/flow_dns.cpp
    flow/flow_engine.cpp
)

# Protocol library sources
set(PROTOCOL_SOURCES
    protocol/ftp_parser.cpp
    protocol/http_parser.cpp
    protocol/imap_parser.cpp
    protocol/pop3_parser.cpp
    protocol/quic_parser.cpp
    protocol/rtp_parser.cpp
    protocol/smb_parser.cpp
    protocol/smtp_parser.cpp
    protocol/ssh_parser.cpp
    protocol/tftp_parser.cpp
    protocol/tls_parser.cpp
)

# Create DNS library
add_library(dns_lib STATIC ${DNS_SOURCES})

# Create Protocol library
add_library(protocol_lib STATIC ${PROTOCOL_SOURCES})

# Create Flow library
add_library(flow_lib STATIC ${FLOW_SOURCES})
target_link_libraries(flow_lib dns_lib protocol_lib)

# Test executable
add_executable(flowcheck_test tests/test_main.cpp)
target_link_libraries(flowcheck_test flow_lib dns_lib protocol_lib)

# DNS mapping test
add_executable(test_dns_mapping tests/test_dns_mapping.cpp)
target_link_libraries(test_dns_mapping flow_lib dns_lib protocol_lib)

# Test with real data from data/ directory
add_executable(test_with_data tests/test_with_data.cpp)
target_link_libraries(test_with_data flow_lib dns_lib protocol_lib)

# Advanced test with statistics
add_executable(test_data_advanced tests/test_data_advanced.cpp)
target_link_libraries(test_data_advanced flow_lib dns_lib protocol_lib)

# Analyze domain extraction failures
add_executable(test_analyze_failures tests/test_analyze_failures.cpp)
target_link_libraries(test_analyze_failures flow_lib dns_lib protocol_lib)

# Test FlowContext description
add_executable(test_description tests/test_description.cpp)
target_link_libraries(test_description flow_lib dns_lib protocol_lib)

# Test IP string caching
add_executable(test_ip_cache tests/test_ip_cache.cpp)
target_link_libraries(test_ip_cache flow_lib dns_lib protocol_lib)

# Test IP string methods
add_executable(test_ip_string tests/test_ip_string.cpp)
target_link_libraries(test_ip_string flow_lib dns_lib protocol_lib)

# Test CNAME record handling
add_executable(test_cname tests/test_cname.cpp)
target_link_libraries(test_cname flow_lib dns_lib protocol_lib)

# Test DNS parsing
add_executable(test_dns_parse tests/test_dns_parse.cpp)
target_link_libraries(test_dns_parse dns_lib)

# Test domain extraction diagnosis
add_executable(test_diagnose tests/test_diagnose.cpp)
target_link_libraries(test_diagnose flow_lib dns_lib protocol_lib)

# Test PCAP file parsing
add_executable(test_pcap tests/test_pcap.cpp)
target_link_libraries(test_pcap flow_lib dns_lib protocol_lib)

# Install targets
install(TARGETS dns_lib protocol_lib flow_lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# Install headers
install(DIRECTORY dns/ DESTINATION include/flowcheck/dns
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY flow/ DESTINATION include/flowcheck/flow
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY protocol/ DESTINATION include/flowcheck/protocol
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
