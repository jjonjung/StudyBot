#pragma once
#include <drogon/HttpController.h>

class StatsController : public drogon::HttpController<StatsController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(StatsController::getStats,  "/api/internal/stats",        drogon::Get);
        ADD_METHOD_TO(StatsController::resetStats,"/api/internal/stats/reset",  drogon::Post);
        ADD_METHOD_TO(StatsController::getEcho,   "/api/internal/echo-stats",   drogon::Get);
    METHOD_LIST_END

    // GET  /api/internal/stats        — 요청수/응답시간/에러율/히스토그램
    void getStats (const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb);

    // POST /api/internal/stats/reset  — 통계 초기화 (부하 테스트 전 리셋용)
    void resetStats(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb);

    // GET  /api/internal/echo-stats   — TcpEchoServer 연결수/바이트 통계
    void getEcho  (const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb);
};
