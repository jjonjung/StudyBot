#include "ProgressController.h"
#include <drogon/drogon.h>

static auto dbErrCb(std::function<void(const drogon::HttpResponsePtr &)> cb) {
    return [cb](const drogon::orm::DrogonDbException &) {
        Json::Value err; err["error"] = "server error";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k500InternalServerError);
        cb(res);
    };
}

static int userId(const drogon::HttpRequestPtr &req) {
    return req->attributes()->get<int>("user_id");
}

void ProgressController::list(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT p.card_id, f.category, f.question, p.known, p.score, p.studied_at "
        "FROM user_progress p JOIN flashcards f ON f.id=p.card_id "
        "WHERE p.user_id=? ORDER BY p.studied_at DESC",
        [cb](const drogon::orm::Result &r) {
            Json::Value arr(Json::arrayValue);
            for (auto &row : r) {
                Json::Value obj;
                obj["card_id"]    = row["card_id"].as<int>();
                obj["category"]   = row["category"].as<std::string>();
                obj["question"]   = row["question"].as<std::string>();
                obj["known"]      = row["known"].as<int>();
                obj["score"]      = row["score"].as<int>();
                obj["studied_at"] = row["studied_at"].as<std::string>();
                arr.append(obj);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        dbErrCb(cb), userId(req)
    );
}

void ProgressController::summary(const drogon::HttpRequestPtr &req,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT f.category, COUNT(*) AS studied, SUM(p.known) AS known_count, AVG(p.score) AS avg_score "
        "FROM user_progress p JOIN flashcards f ON f.id=p.card_id "
        "WHERE p.user_id=? GROUP BY f.category",
        [cb](const drogon::orm::Result &r) {
            Json::Value arr(Json::arrayValue);
            for (auto &row : r) {
                Json::Value obj;
                obj["category"]    = row["category"].as<std::string>();
                obj["studied"]     = row["studied"].as<int>();
                obj["known_count"] = row["known_count"].as<int>();
                obj["avg_score"]   = row["avg_score"].as<double>();
                arr.append(obj);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        dbErrCb(cb), userId(req)
    );
}

void ProgressController::sessions(const drogon::HttpRequestPtr &req,
                                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id,category,total_cards,known_count,played_at FROM interview_sessions "
        "WHERE user_id=? ORDER BY played_at DESC LIMIT 20",
        [cb](const drogon::orm::Result &r) {
            Json::Value arr(Json::arrayValue);
            for (auto &row : r) {
                Json::Value obj;
                obj["id"]          = row["id"].as<int>();
                obj["category"]    = row["category"].as<std::string>();
                obj["total_cards"] = row["total_cards"].as<int>();
                obj["known_count"] = row["known_count"].as<int>();
                obj["played_at"]   = row["played_at"].as<std::string>();
                arr.append(obj);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        dbErrCb(cb), userId(req)
    );
}

void ProgressController::heatmap(const drogon::HttpRequestPtr &req,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto yearStr = req->getParameter("year");
    int year = yearStr.empty() ? 2025 : std::stoi(yearStr);

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT score_date, category, cards_done, known_count, "
        "ROUND(known_count/GREATEST(cards_done,1),4) AS ratio "
        "FROM daily_scores WHERE user_id=? AND YEAR(score_date)=? ORDER BY score_date",
        [cb](const drogon::orm::Result &r) {
            Json::Value arr(Json::arrayValue);
            for (auto &row : r) {
                Json::Value obj;
                obj["score_date"] = row["score_date"].as<std::string>();
                obj["category"]   = row["category"].as<std::string>();
                obj["cards_done"] = row["cards_done"].as<int>();
                obj["known_count"]= row["known_count"].as<int>();
                obj["ratio"]      = row["ratio"].as<double>();
                arr.append(obj);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        dbErrCb(cb), userId(req), year
    );
}

void ProgressController::upsert(const drogon::HttpRequestPtr &req,
                                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                                int cardId) {
    auto body  = req->getJsonObject();
    int known  = body && body->isMember("known") ? (*body)["known"].asInt() : 0;
    int score  = body && body->isMember("score") ? (*body)["score"].asInt() : 0;

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO user_progress (user_id,card_id,known,score) VALUES (?,?,?,?) "
        "ON DUPLICATE KEY UPDATE known=VALUES(known),score=VALUES(score),studied_at=NOW()",
        [cb](const drogon::orm::Result &) {
            Json::Value out; out["message"] = "saved";
            cb(drogon::HttpResponse::newHttpJsonResponse(out));
        },
        dbErrCb(cb), userId(req), cardId, known, score
    );
}

void ProgressController::saveSession(const drogon::HttpRequestPtr &req,
                                     std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("total_cards") || !body->isMember("known_count")) {
        Json::Value err; err["error"] = "total_cards and known_count are required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto category   = body->isMember("category") ? (*body)["category"].asString() : "Mixed";
    int  totalCards = (*body)["total_cards"].asInt();
    int  knownCount = (*body)["known_count"].asInt();
    int  uid        = userId(req);

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO interview_sessions (user_id,category,total_cards,known_count) VALUES (?,?,?,?)",
        [cb, db, uid, category, totalCards, knownCount](const drogon::orm::Result &r) {
            int sessionId = (int)r.insertId();
            db->execSqlAsync(
                "INSERT INTO daily_scores (user_id,category,score_date,cards_done,known_count) "
                "VALUES (?,?,CURDATE(),?,?) "
                "ON DUPLICATE KEY UPDATE cards_done=cards_done+VALUES(cards_done),"
                "known_count=known_count+VALUES(known_count)",
                [cb, sessionId](const drogon::orm::Result &) {
                    Json::Value out; out["id"] = sessionId;
                    auto res = drogon::HttpResponse::newHttpJsonResponse(out);
                    res->setStatusCode(drogon::k201Created);
                    cb(res);
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                uid, category, totalCards, knownCount
            );
        },
        dbErrCb(cb), uid, category, totalCards, knownCount
    );
}
