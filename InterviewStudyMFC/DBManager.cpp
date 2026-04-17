#include "stdafx.h"
#include "DBManager.h"

CStringA DBManager::ToUtf8(const CString& s) {
    CT2CA utf8(s, CP_UTF8);
    return CStringA(utf8);
}

CString DBManager::FromUtf8(const char* s) {
    if (!s) return _T("");
    CA2CT wide(s, CP_UTF8);
    return CString(wide);
}

DBManager& DBManager::Get() {
    static DBManager instance;
    return instance;
}

DBManager::DBManager() : m_pConn(nullptr) {}
DBManager::~DBManager() { Disconnect(); }

bool DBManager::Connect(LPCSTR host, unsigned int port,
                        LPCSTR user, LPCSTR password, LPCSTR dbName) {
    Disconnect();

    m_pConn = mysql_init(nullptr);
    if (!m_pConn) return false;

    my_bool reconnect = 1;
    mysql_options(m_pConn, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(m_pConn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (!mysql_real_connect(m_pConn, host, user, password, dbName, port, nullptr, 0)) {
        mysql_close(m_pConn);
        m_pConn = nullptr;
        return false;
    }

    mysql_set_character_set(m_pConn, "utf8mb4");
    return true;
}

void DBManager::Disconnect() {
    if (m_pConn) {
        mysql_close(m_pConn);
        m_pConn = nullptr;
    }
}

CStringA DBManager::Escape(const CString& s) {
    if (!m_pConn) return ToUtf8(s);
    CStringA raw = ToUtf8(s);
    CStringA buf;
    unsigned long rawLen = static_cast<unsigned long>(raw.GetLength());
    char* p = buf.GetBuffer(rawLen * 2 + 1);
    mysql_real_escape_string(m_pConn, p, raw, rawLen);
    buf.ReleaseBuffer();
    return buf;
}

bool DBManager::ExecuteNonQuery(const CStringA& sql, CString& err) {
    if (mysql_query(m_pConn, sql) != 0) {
        err = FromUtf8(mysql_error(m_pConn));
        ClearPendingResults();
        return false;
    }
    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (res) mysql_free_result(res);
    ClearPendingResults();
    return true;
}

MYSQL_RES* DBManager::ExecuteQuery(const CStringA& sql, CString& err) {
    if (mysql_query(m_pConn, sql) != 0) {
        err = FromUtf8(mysql_error(m_pConn));
        ClearPendingResults();
        return nullptr;
    }
    return mysql_store_result(m_pConn);
}

void DBManager::ClearPendingResults() {
    while (mysql_next_result(m_pConn) == 0) {
        MYSQL_RES* extra = mysql_store_result(m_pConn);
        if (extra) mysql_free_result(extra);
    }
}

std::vector<CString> DBManager::GetDates() {
    std::vector<CString> dates;
    if (!m_pConn) return dates;

    CString err;
    MYSQL_RES* res = ExecuteQuery("CALL sp_question_dates()", err);
    if (!res) return dates;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        dates.push_back(FromUtf8(row[0]));
    }

    mysql_free_result(res);
    ClearPendingResults();
    return dates;
}

std::vector<Question> DBManager::GetQuestions(const CString& date) {
    std::vector<Question> result;
    if (!m_pConn) return result;

    CStringA sql;
    sql.Format("CALL sp_questions_list('%s')", static_cast<LPCSTR>(Escape(date)));

    CString err;
    MYSQL_RES* res = ExecuteQuery(sql, err);
    if (!res) return result;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Question q;
        q.id = row[0] ? atoi(row[0]) : 0;
        q.date = FromUtf8(row[1]);
        q.member = FromUtf8(row[2]);
        q.category = FromUtf8(row[3]);
        q.question = FromUtf8(row[4]);
        q.createdAt = FromUtf8(row[5]);
        result.push_back(q);
    }

    mysql_free_result(res);
    ClearPendingResults();
    return result;
}

bool DBManager::AddQuestion(const CString& date, const CString& member,
                            const CString& category, const CString& question,
                            CString& outError) {
    if (!m_pConn) {
        outError = _T("DB not connected");
        return false;
    }

    CStringA sql;
    sql.Format(
        "CALL sp_question_create('%s','%s','%s','%s')",
        static_cast<LPCSTR>(Escape(date)),
        static_cast<LPCSTR>(Escape(member)),
        static_cast<LPCSTR>(Escape(category)),
        static_cast<LPCSTR>(Escape(question))
    );

    if (!ExecuteNonQuery(sql, outError)) {
        if (mysql_errno(m_pConn) == MYSQL_ERR_DUP_ENTRY) {
            outError = _T("Duplicate question for the same date/member/category");
        }
        return false;
    }
    return true;
}

bool DBManager::DeleteQuestion(int id, CString& outError) {
    if (!m_pConn) {
        outError = _T("DB not connected");
        return false;
    }

    CStringA sql;
    sql.Format("CALL sp_question_delete(%d)", id);
    return ExecuteNonQuery(sql, outError);
}
