#pragma once
#include <drogon/HttpController.h>
#include "../filters/JwtFilter.h"

class CardsController : public drogon::HttpController<CardsController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(CardsController::list,      "/api/cards",           drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(CardsController::interview, "/api/cards/interview", drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(CardsController::stats,     "/api/cards/stats",     drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(CardsController::companies, "/api/cards/companies", drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(CardsController::getById,   "/api/cards/{id}",      drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(CardsController::create,    "/api/cards",           drogon::Post, "JwtFilter");
    METHOD_LIST_END

    void list     (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void interview(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void stats    (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void companies(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void getById  (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id);
    void create   (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
};
