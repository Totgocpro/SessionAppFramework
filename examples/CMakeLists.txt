cmake_minimum_required(VERSION 3.16)

foreach(EXAMPLE
    01_create_account
    02_send_receive_dm
    03_group_bot
    04_file_transfer
    05_echo_bot
    06_group_management_bot
)
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    target_link_libraries(${EXAMPLE} PRIVATE SAF::SessionAppFramework)
    target_compile_features(${EXAMPLE} PRIVATE cxx_std_17)
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${EXAMPLE} PRIVATE -Wno-stringop-overflow)
    endif()
endforeach()
