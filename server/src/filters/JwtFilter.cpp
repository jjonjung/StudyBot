#include "JwtFilter.h"
#include <jwt-cpp/jwt.h>
#include <drogon/drogon.h>
#include "redis/RedisManager.h"

void JwtFilter::doFilter(const drogon::HttpRequestPtr &req,
                         drogon::FilterCallback &&cb,
                         drogon::FilterChainCallback &&ccb) {
    auto auth = req->getHeader("Authorization");
    if (auth.empty() || auth.substr(0, 7) != "Bearer ") {
        auto res = drogon::HttpResponse::newHttpJsonResponse(
            Json::Value({{"error", "인증 토큰이 없습니다."}}));
        res->setStatusCode(drogon::k401Unauthorized);
        cb(res);
        return;
    }

    try {
        auto secret = drogon::app().getCustomConfig()["jwt_secret"].asString();
        auto token = auth.substr(7);

        // Redis 블랙리스트 확인 (로그아웃된 토큰 차단)
        auto& redis = RedisManager::instance();
        if (redis.isConnected() && redis.isBlacklisted(token)) {
            auto res = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value({{"error", "로그아웃된 토큰입니다."}}));
            res->setStatusCode(drogon::k401Unauthorized);
            cb(res);
            return;
        }

        auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .verify(decoded);

        req->attributes()->insert("user_id",
            std::stoi(decoded.get_payload_claim("id").as_string()));
        // 이후 컨트롤러에서 토큰 원문이 필요할 수 있으므로 저장
        req->attributes()->insert("raw_token", token);
        ccb();
    } catch (...) {
        auto res = drogon::HttpResponse::newHttpJsonResponse(
            Json::Value({{"error", "토큰이 만료되었거나 유효하지 않습니다."}}));
        res->setStatusCode(drogon::k401Unauthorized);
        cb(res);
    }
}
