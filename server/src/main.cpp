#include <drogon/drogon.h>

int main() {
    drogon::app()
        .loadConfigFile("../config.json")
        .registerHandler("/health",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                Json::Value out;
                out["status"]  = "ok";
                out["version"] = "3.0.0";
                cb(drogon::HttpResponse::newHttpJsonResponse(out));
            }, {drogon::Get})
        .run();
    return 0;
}
