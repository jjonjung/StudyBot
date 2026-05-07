cmake_minimum_required(VERSION 3.20)
project(studybot_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)
find_package(CURL REQUIRED)

# ── hiredis: 시스템 설치가 있으면 사용, 없으면 소스 자동 빌드 ──────────────
# sudo 없이도 동작 (WSL/Linux 공통)
find_package(hiredis QUIET)
if(NOT hiredis_FOUND)
    find_library(HIREDIS_LIB_SYS hiredis)
    find_path(HIREDIS_INC_SYS hiredis/hiredis.h)
endif()

if(hiredis_FOUND OR (HIREDIS_LIB_SYS AND HIREDIS_INC_SYS))
    message(STATUS "[hiredis] found on system")
    if(NOT hiredis_FOUND)
        add_library(hiredis::hiredis UNKNOWN IMPORTED)
        set_target_properties(hiredis::hiredis PROPERTIES
            IMPORTED_LOCATION "${HIREDIS_LIB_SYS}"
            INTERFACE_INCLUDE_DIRECTORIES "${HIREDIS_INC_SYS}"
        )
    endif()
else()
    message(STATUS "[hiredis] not found — fetching source (v1.2.0)")
    include(FetchContent)
    FetchContent_Declare(
        hiredis
        GIT_REPOSITORY https://github.com/redis/hiredis.git
        GIT_TAG        v1.2.0
        GIT_SHALLOW    TRUE
    )
    set(DISABLE_TESTS ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(hiredis)

    # FetchContent 소스 위치: <build>/_deps/hiredis-src/hiredis.h
    # hiredis-src 디렉토리 자체를 include path로 추가
    # → #include <hiredis.h> 로 사용 (소스에서도 동일하게 처리)
    set(HIREDIS_FETCH_INCLUDE "${CMAKE_BINARY_DIR}/_deps/hiredis-src")
    set_target_properties(hiredis PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/_deps/hiredis-src"
    )
    if(NOT TARGET hiredis::hiredis)
        add_library(hiredis::hiredis ALIAS hiredis)
    endif()
endif()

add_executable(studybot_server
    src/main.cpp
    src/controllers/AuthController.cpp
    src/controllers/CardsController.cpp
    src/controllers/ProgressController.cpp
    src/controllers/LobbyController.cpp
    src/controllers/StatsController.cpp
    src/filters/JwtFilter.cpp
    src/filters/StatsFilter.cpp
    src/ws/LobbyWebSocket.cpp
    src/redis/RedisManager.cpp
    src/net/TcpEchoServer.cpp
    src/util/ThreadPool.cpp
)

target_link_libraries(studybot_server PRIVATE
    Drogon::Drogon
    CURL::libcurl
    bcrypt
    hiredis::hiredis
)

target_include_directories(studybot_server PRIVATE
    src
    $<$<BOOL:${HIREDIS_FETCH_INCLUDE}>:${HIREDIS_FETCH_INCLUDE}>
)
