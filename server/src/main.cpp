#include <drogon/drogon.h>
#include "redis/RedisManager.h"
#include "net/TcpEchoServer.h"
#include "filters/StatsFilter.h"

// StatsController에서 extern으로 참조
TcpEchoServer g_echoServer(3001);

int main() {
    drogon::app().loadConfigFile("../config.json");

    const auto& cfg = drogon::app().getCustomConfig();

    // Redis 초기화 — 연결 실패 시에도 서버는 계속 기동
    RedisManager::instance().connect(
        cfg.get("redis_host", "127.0.0.1").asString(),
        cfg.get("redis_port", 6379).asInt(),
        cfg.get("redis_password", "").asString()
    );

    // TcpEchoServer 시작 (포트 3001, Drogon과 독립 스레드)
    int echoPort = cfg.get("echo_port", 3001).asInt();
    TcpEchoServer* echoPtr = &g_echoServer;
    (void)echoPort;
    g_echoServer.start();

    drogon::app()
        .registerHandler("/health",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                Json::Value out;
                out["status"]  = "ok";
                out["version"] = "3.0.0";
                out["redis"]   = RedisManager::instance().isConnected() ? "ok" : "unavailable";
                out["echo_connections"] = (int)g_echoServer.totalConnections();
                cb(drogon::HttpResponse::newHttpJsonResponse(out));
            }, {drogon::Get})
        .run();

    g_echoServer.stop();
    return 0;
}
