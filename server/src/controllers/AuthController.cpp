#include "AuthController.h"
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <bcrypt/BCrypt.hpp>
#include <curl/curl.h>
#include <json/json.h>
#include "redis/RedisManager.h"

static std::string makeToken(int userId, const std::string &nickname) {
    auto secret = drogon::app().getCustomConfig()["jwt_secret"].asString();
    // nickname은 JWT payload에서 제외 (한글 등 멀티바이트 문자로 인한 크래시 방지)
    // 클라이언트는 userId로 nickname을 별도 조회하거나 로그인 응답값을 캐싱해야 함
    return jwt::create()
        .set_payload_claim("id", jwt::claim(std::to_string(userId)))
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24))
        .sign(jwt::algorithm::hs256{secret});
}

static size_t curlWriteCb(char *ptr, size_t size, size_t nmemb, std::string *out) {
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

static Json::Value httpsGet(const std::string &url) {
    CURL *curl = curl_easy_init();
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    Json::Value root;
    Json::Reader reader;
    reader.parse(body, root);
    return root;
}

void AuthController::registerUser(const drogon::HttpRequestPtr &req,
                                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("username") || !body->isMember("password")) {
        Json::Value err; err["error"] = "username and password are required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto username = (*body)["username"].asString();
    auto password = (*body)["password"].asString();
    auto nickname = body->isMember("nickname") ? (*body)["nickname"].asString() : username;
    auto hash = BCrypt::generateHash(password);

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO users (username, password_hash, nickname) VALUES (?,?,?)",
        [cb](const drogon::orm::Result &r) {
            Json::Value out;
            out["id"]      = (int)r.insertId();
            out["message"] = "registered";
            auto res = drogon::HttpResponse::newHttpJsonResponse(out);
            res->setStatusCode(drogon::k201Created);
            cb(res);
        },
        [cb](const drogon::orm::DrogonDbException &e) {
            std::string msg = e.base().what();
            Json::Value err;
            if (msg.find("Duplicate") != std::string::npos)
                err["error"] = "username already exists";
            else
                err["error"] = "server error";
            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
            res->setStatusCode(msg.find("Duplicate") != std::string::npos
                               ? drogon::k409Conflict : drogon::k500InternalServerError);
            cb(res);
        },
        username, hash, nickname
    );
}

void AuthController::login(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("username") || !body->isMember("password")) {
        Json::Value err; err["error"] = "username and password are required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto username = (*body)["username"].asString();
    auto password = (*body)["password"].asString();

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, password_hash, nickname FROM users WHERE username=? LIMIT 1",
        [cb, password](const drogon::orm::Result &r) {
            if (r.empty() || r[0]["password_hash"].isNull()) {
                Json::Value err; err["error"] = "invalid credentials";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k401Unauthorized);
                cb(res); return;
            }
            auto row = r[0];
            if (!BCrypt::validatePassword(password, row["password_hash"].as<std::string>())) {
                Json::Value err; err["error"] = "invalid credentials";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k401Unauthorized);
                cb(res); return;
            }
            int uid      = row["id"].as<int>();
            auto nick    = row["nickname"].as<std::string>();
            Json::Value out;
            out["token"]    = makeToken(uid, nick);
            out["nickname"] = nick;
            out["userId"]   = uid;
            cb(drogon::HttpResponse::newHttpJsonResponse(out));
        },
        [cb](const drogon::orm::DrogonDbException &) {
            Json::Value err; err["error"] = "server error";
            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
            res->setStatusCode(drogon::k500InternalServerError);
            cb(res);
        },
        username
    );
}

void AuthController::googleMobile(const drogon::HttpRequestPtr &req,
                                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("idToken")) {
        Json::Value err; err["error"] = "idToken is required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto idToken  = (*body)["idToken"].asString();
    auto clientId = drogon::app().getCustomConfig()["google_client_id"].asString();
    auto profile  = httpsGet("https://oauth2.googleapis.com/tokeninfo?id_token=" + idToken);

    if (profile.isMember("error") || profile["aud"].asString() != clientId) {
        Json::Value err; err["error"] = "google token verification failed";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k401Unauthorized);
        cb(res); return;
    }

    auto googleId  = profile["sub"].asString();
    auto email     = profile["email"].asString();
    auto nickname  = profile.isMember("name") ? profile["name"].asString() : email;
    auto avatarUrl = profile.isMember("picture") ? profile["picture"].asString() : "";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, nickname FROM users WHERE google_id=? LIMIT 1",
        [cb, db, googleId, email, nickname, avatarUrl](const drogon::orm::Result &r) {
            if (!r.empty()) {
                int uid   = r[0]["id"].as<int>();
                auto nick = r[0]["nickname"].as<std::string>();
                Json::Value out;
                out["token"]    = makeToken(uid, nick);
                out["nickname"] = nick;
                out["userId"]   = uid;
                cb(drogon::HttpResponse::newHttpJsonResponse(out));
                return;
            }
            db->execSqlAsync(
                "INSERT INTO users (google_id, email, nickname, avatar_url) VALUES (?,?,?,?)",
                [cb](const drogon::orm::Result &r2) {
                    int uid = (int)r2.insertId();
                    Json::Value out;
                    out["token"]    = makeToken(uid, "");
                    out["nickname"] = "";
                    out["userId"]   = uid;
                    cb(drogon::HttpResponse::newHttpJsonResponse(out));
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                googleId, email, nickname, avatarUrl
            );
        },
        [cb](const drogon::orm::DrogonDbException &) {
            Json::Value err; err["error"] = "server error";
            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
            res->setStatusCode(drogon::k500InternalServerError);
            cb(res);
        },
        googleId
    );
}

void AuthController::logout(const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    // JwtFilter가 이미 토큰을 검증하고 raw_token을 attributes에 저장함
    auto token = req->attributes()->get<std::string>("raw_token");

    auto& redis = RedisManager::instance();
    if (redis.isConnected() && !token.empty()) {
        // 토큰 만료 잔여 시간 계산 (최대 24시간)
        int ttl = 86400;
        try {
            auto decoded = jwt::decode(token);
            auto exp = decoded.get_expires_at();
            auto now = std::chrono::system_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::seconds>(exp - now).count();
            if (remaining > 0) ttl = static_cast<int>(remaining);
        } catch (...) {}

        redis.blacklistToken(token, ttl);
    }

    Json::Value out;
    out["message"] = "logged out";
    cb(drogon::HttpResponse::newHttpJsonResponse(out));
}
