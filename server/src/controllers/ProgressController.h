#pragma once
#include <drogon/HttpController.h>
#include "../filters/JwtFilter.h"

class ProgressController : public drogon::HttpController<ProgressController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProgressController::list,       "/api/progress",          drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(ProgressController::summary,    "/api/progress/summary",  drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(ProgressController::sessions,   "/api/progress/sessions", drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(ProgressController::heatmap,    "/api/progress/heatmap",  drogon::Get,  "JwtFilter");
        ADD_METHOD_TO(ProgressController::upsert,     "/api/progress/{cardId}", drogon::Put,  "JwtFilter");
        ADD_METHOD_TO(ProgressController::saveSession,"/api/progress/session",  drogon::Post, "JwtFilter");
    METHOD_LIST_END

    void list       (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void summary    (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void sessions   (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void heatmap    (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void upsert     (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int cardId);
    void saveSession(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
};
