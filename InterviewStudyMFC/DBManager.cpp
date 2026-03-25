#include "stdafx.h"
#include "DBManager.h"

// ─── helpers ─────────────────────────────────────────────────────────────────

CStringA DBManager::ToUtf8(const CString& s) {
    CT2CA utf8(s, CP_UTF8);
    return CStringA(utf8);
}

CString DBManager::FromUtf8(const char* s) {
    if (!s) return _T("");
    CA2CT wide(s, CP_UTF8);
    return CString(wide);
}

// ─── singleton ───────────────────────────────────────────────────────────────

DBManager& DBManager::Get() {
    static DBManager instance;
    return instance;
}

DBManager::DBManager() : m_pConn(nullptr) {}

DBManager::~DBManager() {
    Disconnect();
}

// ─── connection ──────────────────────────────────────────────────────────────

bool DBManager::Connect(LPCSTR host, unsigned int port,
                        LPCSTR user, LPCSTR password, LPCSTR dbName) {
    Disconnect();

    m_pConn = mysql_init(nullptr);
    if (!m_pConn) return false;

    my_bool reconnect = 1;
    mysql_options(m_pConn, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(m_pConn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (!mysql_real_connect(m_pConn, host, user, password, dbName,
                            port, nullptr, 0)) {
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

// ─── internal: escape string ─────────────────────────────────────────────────

CStringA DBManager::Escape(const CString& s) {
    if (!m_pConn) return ToUtf8(s);
    CStringA raw = ToUtf8(s);
    CStringA buf;
    unsigned long rawLen = (unsigned long)raw.GetLength();
    char* p = buf.GetBuffer(rawLen * 2 + 1);
    mysql_real_escape_string(m_pConn, p, raw, rawLen);
    buf.ReleaseBuffer();
    return buf;
}

// ─── queries ─────────────────────────────────────────────────────────────────

std::vector<CString> DBManager::GetDates() {
    std::vector<CString> dates;
    if (!m_pConn) return dates;

    const char* sql =
        "SELECT DISTINCT study_date FROM questions "
        "ORDER BY study_date DESC LIMIT 30";

    if (mysql_query(m_pConn, sql) != 0) return dates;

    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (!res) return dates;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
        dates.push_back(FromUtf8(row[0]));

    mysql_free_result(res);
    return dates;
}

std::vector<Question> DBManager::GetQuestions(const CString& date) {
    std::vector<Question> result;
    if (!m_pConn) return result;

    // 멤버/카테고리 정렬 우선순위를 CASE WHEN 으로 처리
    // (UTF-8 한글 리터럴은 연결이 utf8mb4이므로 직접 사용 가능)
    CStringA esc = Escape(date);

    CStringA sql =
        "SELECT id, study_date, member, category, question,"
        " DATE_FORMAT(created_at,'%H:%i') "
        "FROM questions WHERE study_date='";
    sql += esc;
    sql += "' ORDER BY "
           " CASE category WHEN 'CS' THEN 1 WHEN 'C++' THEN 2 ELSE 3 END,"
           " CASE member"
           "  WHEN '\xEC\x97\xAC\xEB\xAF\xBC' THEN 1"   // 여민
           "  WHEN '\xEC\x9D\x80\xEC\xA0\x95' THEN 2"   // 은정
           "  ELSE 3 END";                                // 혜선

    if (mysql_query(m_pConn, sql) != 0) return result;

    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (!res) return result;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Question q;
        q.id        = row[0] ? atoi(row[0]) : 0;
        q.date      = FromUtf8(row[1]);
        q.member    = FromUtf8(row[2]);
        q.category  = FromUtf8(row[3]);
        q.question  = FromUtf8(row[4]);
        q.createdAt = FromUtf8(row[5]);
        result.push_back(q);
    }
    mysql_free_result(res);
    return result;
}

bool DBManager::AddQuestion(const CString& date, const CString& member,
                             const CString& category, const CString& question,
                             CString& outError) {
    if (!m_pConn) {
        outError = _T("DB에 연결되지 않았습니다.");
        return false;
    }

    CStringA eDate     = Escape(date);
    CStringA eMember   = Escape(member);
    CStringA eCategory = Escape(category);
    CStringA eQuestion = Escape(question);

    CStringA sql;
    sql.Format(
        "INSERT INTO questions (study_date, member, category, question)"
        " VALUES ('%s','%s','%s','%s')",
        (LPCSTR)eDate, (LPCSTR)eMember,
        (LPCSTR)eCategory, (LPCSTR)eQuestion);

    if (mysql_query(m_pConn, sql) != 0) {
        if (mysql_errno(m_pConn) == MYSQL_ERR_DUP_ENTRY)
            outError = _T("이미 해당 날짜에 같은 카테고리 질문을 등록하셨습니다.");
        else
            outError = FromUtf8(mysql_error(m_pConn));
        return false;
    }
    return true;
}

bool DBManager::DeleteQuestion(int id, CString& outError) {
    if (!m_pConn) {
        outError = _T("DB에 연결되지 않았습니다.");
        return false;
    }

    CStringA sql;
    sql.Format("DELETE FROM questions WHERE id=%d", id);

    if (mysql_query(m_pConn, sql) != 0) {
        outError = FromUtf8(mysql_error(m_pConn));
        return false;
    }
    return true;
}
