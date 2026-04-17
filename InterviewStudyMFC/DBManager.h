#pragma once
#include <mysql.h>
#include <vector>

#define MYSQL_ERR_DUP_ENTRY 1062

struct Question {
    int     id;
    CString date;
    CString member;
    CString category;
    CString question;
    CString createdAt;
};

class DBManager {
public:
    static DBManager& Get();

    bool Connect(LPCSTR host, unsigned int port,
                 LPCSTR user, LPCSTR password, LPCSTR dbName);
    void Disconnect();
    bool IsConnected() const { return m_pConn != nullptr; }

    std::vector<CString> GetDates();
    std::vector<Question> GetQuestions(const CString& date);

    bool AddQuestion(const CString& date, const CString& member,
                     const CString& category, const CString& question,
                     CString& outError);

    bool DeleteQuestion(int id, CString& outError);

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    MYSQL* m_pConn;

    static CStringA ToUtf8(const CString& s);
    static CString FromUtf8(const char* s);
    CStringA Escape(const CString& s);
    bool ExecuteNonQuery(const CStringA& sql, CString& err);
    MYSQL_RES* ExecuteQuery(const CStringA& sql, CString& err);
    void ClearPendingResults();
};
