#pragma once
#include <drogon/HttpController.h>
#include "../filters/JwtFilter.h"

class LobbyController : public drogon::HttpController<LobbyController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(LobbyController::create,  "/api/lobby",            drogon::Post, "JwtFilter");
        ADD_METHOD_TO(LobbyController::join,    "/api/lobby/join",       drogon::Post, "JwtFilter");
        ADD_METHOD_TO(LobbyController::get,     "/api/lobby/{id}",       drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(LobbyController::kick,    "/api/lobby/{id}/kick",  drogon::Post, "JwtFilter");
        ADD_METHOD_TO(LobbyController::start,   "/api/lobby/{id}/start", drogon::Post, "JwtFilter");
        ADD_METHOD_TO(LobbyController::close,   "/api/lobby/{id}/close", drogon::Post, "JwtFilter");
    METHOD_LIST_END

    void create(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void join  (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void get   (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id);
    void kick  (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id);
    void start (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id);
    void close (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id);
};
