#include "StatsController.h"
#include "filters/StatsFilter.h"
#include "net/TcpEchoServer.h"
#include <drogon/drogon.h>

// TcpEchoServer 인스턴스 (main.cpp에서 extern으로 접근)
extern TcpEchoServer g_echoServer;

void StatsController::getStats(const drogon::HttpRequestPtr &,
                               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    cb(drogon::HttpResponse::newHttpJsonResponse(StatsFilter::instance().snapshot()));
}

void StatsController::resetStats(const drogon::HttpRequestPtr &,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    StatsFilter::instance().reset();
    Json::Value out;
    out["message"] = "stats reset";
    cb(drogon::HttpResponse::newHttpJsonResponse(out));
}

void StatsController::getEcho(const drogon::HttpRequestPtr &,
                              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    Json::Value out;
    out["total_connections"] = (int)g_echoServer.totalConnections();
    out["total_bytes_echoed"] = (int)g_echoServer.totalBytesEchoed();
    cb(drogon::HttpResponse::newHttpJsonResponse(out));
}
