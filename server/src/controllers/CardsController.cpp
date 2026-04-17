#include "CardsController.h"
#include <drogon/drogon.h>

static const std::set<std::string> VALID_CATEGORIES = {"Unreal","C++","CS","Company","Algorithm"};
static const std::set<std::string> VALID_DIFFICULTIES = {"Easy","Normal","Hard"};

static auto dbErrCb(std::function<void(const drogon::HttpResponsePtr &)> cb) {
    return [cb](const drogon::orm::DrogonDbException &) {
        Json::Value err; err["error"] = "server error";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k500InternalServerError);
        cb(res);
    };
}

static Json::Value rowToJson(const drogon::orm::Row &r) {
    Json::Value obj;
    obj["id"]         = r["id"].as<int>();
    obj["category"]   = r["category"].as<std::string>();
    obj["question"]   = r["question"].as<std::string>();
    obj["answer"]     = r["answer"].as<std::string>();
    obj["difficulty"] = r["difficulty"].as<std::string>();
    if (!r["company"].isNull())          obj["company"]          = r["company"].as<std::string>();
    if (!r["core_conditions"].isNull())  obj["core_conditions"]  = r["core_conditions"].as<std::string>();
    if (!r["selection_reason"].isNull()) obj["selection_reason"] = r["selection_reason"].as<std::string>();
    if (!r["code_cpp"].isNull())         obj["code_cpp"]         = r["code_cpp"].as<std::string>();
    if (!r["code_csharp"].isNull())      obj["code_csharp"]      = r["code_csharp"].as<std::string>();
    if (!r["time_complexity"].isNull())  obj["time_complexity"]  = r["time_complexity"].as<std::string>();
    return obj;
}

void CardsController::list(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto category   = req->getParameter("category");
    auto difficulty = req->getParameter("difficulty");
    auto company    = req->getParameter("company");
    int  limit  = std::max(1, req->getParameter("limit").empty()  ? 20 : std::stoi(req->getParameter("limit")));
    int  page   = std::max(1, req->getParameter("page").empty()   ? 1  : std::stoi(req->getParameter("page")));
    int  offset = (page - 1) * limit;

    std::string sql = "SELECT id,category,company,question,answer,difficulty,"
                      "core_conditions,selection_reason,code_cpp,code_csharp,time_complexity "
                      "FROM flashcards WHERE 1=1";
    std::vector<std::string> params;

    if (VALID_CATEGORIES.count(category))    { sql += " AND category=?";   params.push_back(category); }
    if (VALID_DIFFICULTIES.count(difficulty)){ sql += " AND difficulty=?"; params.push_back(difficulty); }
    if (!company.empty())                    { sql += " AND company=?";    params.push_back(company); }
    sql += " ORDER BY id LIMIT " + std::to_string(offset) + "," + std::to_string(limit);

    auto db = drogon::app().getDbClient();
    auto handler = [cb](const drogon::orm::Result &r) {
        Json::Value arr(Json::arrayValue);
        for (auto &row : r) arr.append(rowToJson(row));
        cb(drogon::HttpResponse::newHttpJsonResponse(arr));
    };

    if (params.empty())
        db->execSqlAsync(sql, handler, dbErrCb(cb));
    else if (params.size() == 1)
        db->execSqlAsync(sql, handler, dbErrCb(cb), params[0]);
    else if (params.size() == 2)
        db->execSqlAsync(sql, handler, dbErrCb(cb), params[0], params[1]);
    else
        db->execSqlAsync(sql, handler, dbErrCb(cb), params[0], params[1], params[2]);
}

void CardsController::interview(const drogon::HttpRequestPtr &req,
                                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto category = req->getParameter("category");
    auto company  = req->getParameter("company");
    int  count    = std::max(1, req->getParameter("count").empty() ? 10 : std::stoi(req->getParameter("count")));

    std::string sql = "SELECT id,category,company,question,answer,difficulty,"
                      "core_conditions,selection_reason,code_cpp,code_csharp,time_complexity "
                      "FROM flashcards WHERE 1=1";
    std::vector<std::string> params;

    if (VALID_CATEGORIES.count(category)) { sql += " AND category=?"; params.push_back(category); }
    if (!company.empty())                 { sql += " AND company=?";  params.push_back(company); }
    sql += " ORDER BY RAND() LIMIT " + std::to_string(count);

    auto db = drogon::app().getDbClient();
    auto handler = [cb](const drogon::orm::Result &r) {
        Json::Value arr(Json::arrayValue);
        for (auto &row : r) arr.append(rowToJson(row));
        cb(drogon::HttpResponse::newHttpJsonResponse(arr));
    };

    if (params.empty())
        db->execSqlAsync(sql, handler, dbErrCb(cb));
    else if (params.size() == 1)
        db->execSqlAsync(sql, handler, dbErrCb(cb), params[0]);
    else
        db->execSqlAsync(sql, handler, dbErrCb(cb), params[0], params[1]);
}

void CardsController::stats(const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT category, COUNT(*) AS total FROM flashcards GROUP BY category",
        [cb](const drogon::orm::Result &r) {
            Json::Value arr(Json::arrayValue);
            for (auto &row : r) {
                Json::Value obj;
                obj["category"] = row["category"].as<std::string>();
                obj["total"]    = row["total"].as<int>();
                arr.append(obj);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        dbErrCb(cb)
    );
}

void CardsController::companies(const drogon::HttpRequestPtr &req,
                                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT DISTINCT company FROM flashcards WHERE category='Company' AND company IS NOT NULL ORDER BY company",
        [cb](const drogon::orm::Result &r) {
            Json::Value arr(Json::arrayValue);
            for (auto &row : r) arr.append(row["company"].as<std::string>());
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        dbErrCb(cb)
    );
}

void CardsController::getById(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                              int id) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id,category,company,question,answer,difficulty,"
        "core_conditions,selection_reason,code_cpp,code_csharp,time_complexity "
        "FROM flashcards WHERE id=?",
        [cb](const drogon::orm::Result &r) {
            if (r.empty()) {
                Json::Value err; err["error"] = "card not found";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k404NotFound);
                cb(res); return;
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(rowToJson(r[0])));
        },
        dbErrCb(cb), id
    );
}

void CardsController::create(const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("category") || !body->isMember("question") || !body->isMember("answer")) {
        Json::Value err; err["error"] = "invalid card payload";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto category = (*body)["category"].asString();
    if (!VALID_CATEGORIES.count(category)) {
        Json::Value err; err["error"] = "invalid category";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto question         = (*body)["question"].asString();
    auto answer           = (*body)["answer"].asString();
    auto difficulty       = body->isMember("difficulty")       ? (*body)["difficulty"].asString()       : "Normal";
    auto company          = body->isMember("company")          ? (*body)["company"].asString()          : "";
    auto core_conditions  = body->isMember("core_conditions")  ? (*body)["core_conditions"].asString()  : "";
    auto selection_reason = body->isMember("selection_reason") ? (*body)["selection_reason"].asString() : "";
    auto code_cpp         = body->isMember("code_cpp")         ? (*body)["code_cpp"].asString()         : "";
    auto code_csharp      = body->isMember("code_csharp")      ? (*body)["code_csharp"].asString()      : "";
    auto time_complexity  = body->isMember("time_complexity")  ? (*body)["time_complexity"].asString()  : "";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO flashcards (category,company,question,answer,difficulty,"
        "core_conditions,selection_reason,code_cpp,code_csharp,time_complexity) "
        "VALUES (?,?,?,?,?,?,?,?,?,?)",
        [cb](const drogon::orm::Result &r) {
            Json::Value out; out["id"] = (int)r.insertId();
            auto res = drogon::HttpResponse::newHttpJsonResponse(out);
            res->setStatusCode(drogon::k201Created);
            cb(res);
        },
        dbErrCb(cb),
        category,
        company.empty()          ? nullptr : company.c_str(),
        question, answer, difficulty,
        core_conditions.empty()  ? nullptr : core_conditions.c_str(),
        selection_reason.empty() ? nullptr : selection_reason.c_str(),
        code_cpp.empty()         ? nullptr : code_cpp.c_str(),
        code_csharp.empty()      ? nullptr : code_csharp.c_str(),
        time_complexity.empty()  ? nullptr : time_complexity.c_str()
    );
}
