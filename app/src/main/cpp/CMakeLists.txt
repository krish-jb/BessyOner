cmake_minimum_required(VERSION 3.22.1)

set(PKG_CONFIG_EXECUTABLE "" CACHE FILEPATH "Disabled")

project("thebeast" C)

# Size-oriented compiler and linker optimization flags
set(CMAKE_C_FLAGS_RELEASE "-Os -ffunction-sections -fdata-sections -fvisibility=hidden -flto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,--gc-sections -Wl,--strip-all -Wl,-O1")

find_package(libssh2 CONFIG REQUIRED)
find_package(MbedTLS CONFIG REQUIRED)

add_library(native_core SHARED
        native_core.c
        jni_bridge.c
)

find_library(log-lib log)

target_link_libraries(native_core
        PRIVATE
            ${log-lib}
            libssh2::libssh2
            MbedTLS::mbedcrypto
            MbedTLS::mbedtls
            MbedTLS::mbedx509
)