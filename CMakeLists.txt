cmake_minimum_required(VERSION 3.16)
project(SessionAppFramework VERSION 1.0.0 LANGUAGES CXX)

# ─────────────────────────────────────────────────────────
# C++ Standard
# ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-stringop-overflow)
    add_link_options(-Wno-stringop-overflow)
endif()

# ─────────────────────────────────────────────────────────
# Build options
# ─────────────────────────────────────────────────────────
option(SAF_BUILD_EXAMPLES  "Build examples"         ON)
option(SAF_BUILD_TESTS     "Build tests"            OFF)
option(SAF_BUILD_SHARED    "Build shared library"   OFF)
option(SAF_ENABLE_ONION    "Enable onion-routing (requires libsession-util ENABLE_ONIONERQ)" ON)

# ─────────────────────────────────────────────────────────
# Dependencies
# ─────────────────────────────────────────────────────────
find_package(PkgConfig QUIET)

# OpenSSL (Required by both CURL and SAF)
if(WIN32 AND NOT MINGW AND NOT OPENSSL_ROOT_DIR)
    # Check common paths on GitHub Runners and Windows systems (only for MSVC)
    foreach(DIR "C:/Program Files/OpenSSL-Win64" "C:/Program Files/OpenSSL" "C:/local/OpenSSL-Win64")
        if(EXISTS "${DIR}")
            set(OPENSSL_ROOT_DIR "${DIR}" CACHE PATH "" FORCE)
            message(STATUS "Setting OPENSSL_ROOT_DIR to ${DIR}")
            break()
        endif()
    endforeach()
endif()
find_package(OpenSSL REQUIRED)

# libsession-util  (must be installed or provided via -DLIBSESSION_ROOT=...)
if(NOT LIBSESSION_FOUND AND NOT LIBSESSION_INCLUDE_DIRS)
    set(LOCAL_LIBSESSION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libsession-util")
    if(EXISTS "${LOCAL_LIBSESSION_DIR}/include/session/config/user_profile.hpp")
        message(STATUS "Found local libsession-util source directory")
        set(LIBSESSION_ROOT "${LOCAL_LIBSESSION_DIR}")
        file(TO_CMAKE_PATH "${LOCAL_LIBSESSION_DIR}/Build" LIBSESSION_BUILD_DIR)
        
        set(OXENC_INC "${LOCAL_LIBSESSION_DIR}/external/oxen-libquic/external/oxen-encoding")
        set(FMT_INC "${LOCAL_LIBSESSION_DIR}/external/oxen-logging/fmt/include")
        set(SPDLOG_INC "${LOCAL_LIBSESSION_DIR}/external/oxen-logging/spdlog/include")
        set(PROTO_INC "${LOCAL_LIBSESSION_DIR}/proto")
        
        set(LIBSESSION_INCLUDE_DIRS 
            "${LOCAL_LIBSESSION_DIR}/include"
            "${OXENC_INC}"
            "${FMT_INC}"
            "${SPDLOG_INC}"
            "${PROTO_INC}"
        )
        
        # Look for libraries in Build folder
        file(GLOB_RECURSE FOUND_LIBS 
            "${LIBSESSION_BUILD_DIR}/*.a"
            "${LIBSESSION_BUILD_DIR}/*.lib"
        )
        
        set(LIBSESSION_LIBRARIES "")
        foreach(LIB_PATH ${FOUND_LIBS})
            if(NOT LIB_PATH MATCHES "CMakeLTOTest")
                file(TO_CMAKE_PATH "${LIB_PATH}" CLEAN_LIB_PATH)
                list(APPEND LIBSESSION_LIBRARIES "${CLEAN_LIB_PATH}")
            endif()
        endforeach()
        message(STATUS "Found libraries: ${LIBSESSION_LIBRARIES}")
        set(LIBSESSION_FOUND TRUE)
    endif()
endif()

if(DEFINED LIBSESSION_ROOT AND NOT LIBSESSION_FOUND)
    list(PREPEND CMAKE_PREFIX_PATH "${LIBSESSION_ROOT}")
endif()

# Try pkg-config first (if available), then manual path
if(PKG_CONFIG_FOUND AND NOT LIBSESSION_FOUND)
    pkg_check_modules(LIBSESSION libsession-util)
endif()

if(NOT LIBSESSION_FOUND)
    find_path(LIBSESSION_INCLUDE_DIRS session/config/user_profile.hpp
        PATHS ${LIBSESSION_ROOT}/include /usr/local/include /usr/include
        REQUIRED)
    find_library(LIBSESSION_LIBRARIES session-util
        PATHS ${LIBSESSION_ROOT}/lib ${LIBSESSION_ROOT}/Build /usr/local/lib /usr/lib
        REQUIRED)
endif()

# libcurl
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    message(STATUS "CURL not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        curl
        URL https://github.com/curl/curl/releases/download/curl-8_12_1/curl-8.12.1.tar.xz
        URL_HASH SHA256=0341f1ed97a26c811abaebd37d62b833956792b7607ea3f15d001613c76de202
    )
    set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
    set(HTTP_ONLY ON CACHE INTERNAL "")
    set(CURL_USE_OPENSSL ON CACHE INTERNAL "")
    FetchContent_MakeAvailable(curl)
    set(CURL_LIBRARIES libcurl)
    FetchContent_GetProperties(curl SOURCE_DIR CURL_SOURCE_DIR)
    FetchContent_GetProperties(curl BINARY_DIR CURL_BINARY_DIR)
    set(CURL_INCLUDE_DIRS "${CURL_SOURCE_DIR}/include" "${CURL_BINARY_DIR}/include/curl")
endif()

# nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ─────────────────────────────────────────────────────────
# Source files
# ─────────────────────────────────────────────────────────
set(SAF_SOURCES
    src/Account.cpp
    src/NetworkClient.cpp
    src/SwarmManager.cpp
    src/MessageService.cpp
    src/GroupManager.cpp
    src/FileTransfer.cpp
    src/ConfigManager.cpp
    src/OnionRouter.cpp
    src/Utils.cpp
    src/Session.cpp
)

set(SAF_HEADERS
    include/SessionAppFramework/SessionAppFramework.hpp
    include/SessionAppFramework/Session.hpp
    include/SessionAppFramework/Account.hpp
    include/SessionAppFramework/NetworkClient.hpp
    include/SessionAppFramework/SwarmManager.hpp
    include/SessionAppFramework/MessageService.hpp
    include/SessionAppFramework/GroupManager.hpp
    include/SessionAppFramework/FileTransfer.hpp
    include/SessionAppFramework/ConfigManager.hpp
    include/SessionAppFramework/OnionRouter.hpp
    include/SessionAppFramework/Types.hpp
    include/SessionAppFramework/Utils.hpp
    include/SessionAppFramework/Exceptions.hpp
)

# ─────────────────────────────────────────────────────────
# Library target
# ─────────────────────────────────────────────────────────
if(SAF_BUILD_SHARED)
    add_library(SessionAppFramework SHARED ${SAF_SOURCES} ${SAF_HEADERS})
else()
    add_library(SessionAppFramework STATIC ${SAF_SOURCES} ${SAF_HEADERS})
endif()

add_library(SAF::SessionAppFramework ALIAS SessionAppFramework)

target_include_directories(SessionAppFramework
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        SYSTEM ${LIBSESSION_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(SessionAppFramework PUBLIC -Wno-stringop-overflow)
endif()

target_link_libraries(SessionAppFramework
    PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
    PRIVATE
        $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
        -Wl,--start-group
        ${LIBSESSION_LIBRARIES}
        -Wl,--end-group
        ${CURL_LIBRARIES}
)

# ─────────────────────────────────────────────────────────
# System Dependencies (Protobuf, Sodium, etc.)
# ─────────────────────────────────────────────────────────
# Always try to find these if we are not in a fully bundled static build
find_package(Protobuf QUIET)
if(Protobuf_FOUND)
    if(TARGET Protobuf::libprotobuf-lite)
        target_link_libraries(SessionAppFramework PRIVATE Protobuf::libprotobuf-lite)
    else()
        target_link_libraries(SessionAppFramework PRIVATE Protobuf::libprotobuf)
    endif()
endif()

if(PKG_CONFIG_FOUND)
    pkg_check_modules(SODIUM libsodium IMPORTED_TARGET)
    if(SODIUM_FOUND)
        target_link_libraries(SessionAppFramework PRIVATE PkgConfig::SODIUM)
    endif()
    
    if(SAF_ENABLE_ONION)
        pkg_check_modules(NETTLE nettle IMPORTED_TARGET)
        if(NETTLE_FOUND)
            target_link_libraries(SessionAppFramework PRIVATE PkgConfig::NETTLE)
        endif()
    endif()
endif()

if(SAF_ENABLE_ONION)
    target_compile_definitions(SessionAppFramework PRIVATE SAF_ENABLE_ONION=1)
endif()

# ─────────────────────────────────────────────────────────
# Examples / Tests / Install / Python
# ─────────────────────────────────────────────────────────
if(SAF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SAF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SAF_BUILD_PYTHON "Build Python bindings" OFF)
if(SAF_BUILD_PYTHON)
    find_package(pybind11 REQUIRED)
    pybind11_add_module(session_saf python/bindings.cpp)
    target_link_libraries(session_saf PRIVATE SAF::SessionAppFramework)
    install(TARGETS session_saf LIBRARY DESTINATION . RUNTIME DESTINATION . COMPONENT python_modules)
endif()

install(TARGETS SessionAppFramework EXPORT SessionAppFrameworkTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/SessionAppFramework DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT SessionAppFrameworkTargets
    FILE SessionAppFrameworkTargets.cmake
    NAMESPACE SAF::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SessionAppFramework
)

configure_package_config_file(
    cmake/SessionAppFrameworkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SessionAppFrameworkConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SessionAppFramework
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SessionAppFrameworkConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SessionAppFrameworkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SessionAppFrameworkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SessionAppFramework
)
