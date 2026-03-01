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
if(NOT WIN32)
    find_package(PkgConfig)
endif()

# libsession-util  (must be installed or provided via -DLIBSESSION_ROOT=...)
if(DEFINED LIBSESSION_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${LIBSESSION_ROOT}")
endif()

# Try pkg-config first (if available), then manual path
if(PKG_CONFIG_FOUND)
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

# libcurl (HTTP for swarm communication)
find_package(CURL REQUIRED)

# nlohmann/json (JSON parsing for Session Node responses)
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

# OpenSSL (for additional crypto, key serialisation)
find_package(OpenSSL REQUIRED)

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
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
    PRIVATE
        -Wl,--start-group
        ${LIBSESSION_LIBRARIES}
        -Wl,--end-group
        ${CURL_LIBRARIES}
)

if(SAF_ENABLE_ONION)
    target_compile_definitions(SessionAppFramework PRIVATE SAF_ENABLE_ONION=1)
endif()

# ─────────────────────────────────────────────────────────
# Examples
# ─────────────────────────────────────────────────────────
if(SAF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ─────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────
if(SAF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─────────────────────────────────────────────────────────
# Install rules
# ─────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ─────────────────────────────────────────────────────────
# Python Bindings (Optional)
# ─────────────────────────────────────────────────────────
option(SAF_BUILD_PYTHON "Build Python bindings" OFF)

if(SAF_BUILD_PYTHON)
    find_package(pybind11 REQUIRED)
    pybind11_add_module(session_saf python/bindings.cpp)
    target_link_libraries(session_saf PRIVATE SAF::SessionAppFramework)
    
    # REQUIRED for scikit-build-core to include the module in the wheel
    install(TARGETS session_saf 
            LIBRARY DESTINATION .
            RUNTIME DESTINATION .
            COMPONENT python_modules)
endif()

install(TARGETS SessionAppFramework EXPORT SessionAppFrameworkTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/SessionAppFramework
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

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
