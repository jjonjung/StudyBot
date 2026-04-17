cmake_minimum_required(VERSION 3.20)
project(studybot_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)
find_package(CURL REQUIRED)

add_executable(studybot_server
    src/main.cpp
    src/controllers/AuthController.cpp
    src/controllers/CardsController.cpp
    src/controllers/ProgressController.cpp
    src/controllers/LobbyController.cpp
    src/filters/JwtFilter.cpp
    src/ws/LobbyWebSocket.cpp
)

target_link_libraries(studybot_server PRIVATE
    Drogon::Drogon
    CURL::libcurl
    bcrypt
)

target_include_directories(studybot_server PRIVATE src)
