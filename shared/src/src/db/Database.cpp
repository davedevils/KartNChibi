
#include "db/Database.h"
#include "logging/Logger.h"

namespace knc {

#ifdef KNC_HAS_MARIADB

bool Database::init(const DBConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    
    for (int i = 0; i < config.poolSize; ++i) {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            LOG_ERROR("DB", "mysql_init failed");
            return false;
        }
        
        if (!mysql_real_connect(conn, 
                config.host.c_str(),
                config.user.c_str(),
                config.password.c_str(),
                config.database.c_str(),
                config.port, nullptr, 0)) {
            LOG_ERROR("DB", std::string("Connect failed: ") + mysql_error(conn));
            mysql_close(conn);
            return false;
        }
        
        mysql_set_character_set(conn, "utf8mb4");
        m_pool.push(conn);
    }
    
    m_initialized = true;
    LOG_INFO("DB", "Pool initialized with " + std::to_string(config.poolSize) + " connections");
    return true;
}

void Database::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_pool.empty()) {
        mysql_close(m_pool.front());
        m_pool.pop();
    }
    m_initialized = false;
}

MYSQL* Database::getConnection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool.empty()) return nullptr;
    MYSQL* conn = m_pool.front();
    m_pool.pop();
    
    if (mysql_ping(conn) != 0) {
        mysql_close(conn);
        conn = mysql_init(nullptr);
        mysql_real_connect(conn,
            m_config.host.c_str(),
            m_config.user.c_str(),
            m_config.password.c_str(),
            m_config.database.c_str(),
            m_config.port, nullptr, 0);
    }
    return conn;
}

void Database::releaseConnection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.push(conn);
}

bool Database::execute(const std::string& sql) {
    MYSQL* conn = getConnection();
    if (!conn) return false;
    
    bool success = mysql_query(conn, sql.c_str()) == 0;
    if (!success) {
        LOG_ERROR("DB", std::string("Query failed: ") + mysql_error(conn));
    }
    
    releaseConnection(conn);
    return success;
}

std::vector<std::map<std::string, std::string>> Database::query(const std::string& sql) {
    std::vector<std::map<std::string, std::string>> results;
    
    MYSQL* conn = getConnection();
    if (!conn) return results;
    
    if (mysql_query(conn, sql.c_str()) != 0) {
        LOG_ERROR("DB", std::string("Query failed: ") + mysql_error(conn));
        releaseConnection(conn);
        return results;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (res) {
        int numFields = mysql_num_fields(res);
        MYSQL_FIELD* fields = mysql_fetch_fields(res);
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            // real lengths else a blob with an embedded null truncates at strlen
            unsigned long* lens = mysql_fetch_lengths(res);
            std::map<std::string, std::string> rowMap;
            for (int i = 0; i < numFields; ++i) {
                rowMap[fields[i].name] = row[i] ? std::string(row[i], lens[i]) : "";
            }
            results.push_back(rowMap);
        }
        mysql_free_result(res);
    }
    
    releaseConnection(conn);
    return results;
}

std::string Database::escapeString(const std::string& input) {
    MYSQL* conn = getConnection();
    if (!conn) {
        // fallback basic escaping without a connection
        std::string result;
        result.reserve(input.size() * 2);
        for (char c : input) {
            switch (c) {
                case '\0': result += "\\0"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\\': result += "\\\\"; break;
                case '\'': result += "\\'"; break;
                case '"': result += "\\\""; break;
                case '\x1a': result += "\\Z"; break;
                default: result += c;
            }
        }
        return result;
    }
    
    std::vector<char> buffer(input.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, buffer.data(), input.c_str(), 
                                                  static_cast<unsigned long>(input.size()));
    releaseConnection(conn);
    return std::string(buffer.data(), len);
}

bool Database::executePreparedRaw(const std::string& sql, const std::vector<std::string>& params) {
    std::string finalSql;
    finalSql.reserve(sql.size() + params.size() * 32);
    
    size_t paramIndex = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == '?' && paramIndex < params.size()) {
            finalSql += '\'';
            finalSql += escapeString(params[paramIndex]);
            finalSql += '\'';
            ++paramIndex;
        } else {
            finalSql += sql[i];
        }
    }
    
    if (paramIndex != params.size()) {
        LOG_WARN("DB", "executePrepared: param count mismatch - expected " + 
                 std::to_string(paramIndex) + " got " + std::to_string(params.size()));
    }
    
    return execute(finalSql);
}

std::vector<std::map<std::string, std::string>> Database::queryPreparedRaw(
    const std::string& sql, 
    const std::vector<std::string>& params
) {
    std::string finalSql;
    finalSql.reserve(sql.size() + params.size() * 32);
    
    size_t paramIndex = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == '?' && paramIndex < params.size()) {
            finalSql += '\'';
            finalSql += escapeString(params[paramIndex]);
            finalSql += '\'';
            ++paramIndex;
        } else {
            finalSql += sql[i];
        }
    }
    
    if (paramIndex != params.size()) {
        LOG_WARN("DB", "queryPrepared: param count mismatch - expected " + 
                 std::to_string(paramIndex) + " got " + std::to_string(params.size()));
    }
    
    return query(finalSql);
}

#else

// Stub implementations when MariaDB is not available
bool Database::init(const DBConfig&) {
    LOG_WARN("DB", "MariaDB not available - database disabled");
    return true;
}

void Database::shutdown() {}

bool Database::execute(const std::string&) {
    return false;
}

std::vector<std::map<std::string, std::string>> Database::query(const std::string&) {
    return {};
}

std::string Database::escapeString(const std::string& input) {
    std::string result;
    result.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '\0': result += "\\0"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\\': result += "\\\\"; break;
            case '\'': result += "\\'"; break;
            case '"': result += "\\\""; break;
            case '\x1a': result += "\\Z"; break;
            default: result += c;
        }
    }
    return result;
}

bool Database::executePreparedRaw(const std::string&, const std::vector<std::string>&) {
    return false;
}

std::vector<std::map<std::string, std::string>> Database::queryPreparedRaw(
    const std::string&, const std::vector<std::string>&
) {
    return {};
}

#endif


namespace {
std::string bindParams(const std::string& sql,
                       const std::vector<std::string>& params,
                       Database& db) {
    std::string out;
    out.reserve(sql.size() + params.size() * 32);
    size_t p = 0;
    for (char ch : sql) {
        if (ch == '?' && p < params.size()) {
            out += char(39);
            out += db.escapeString(params[p]);
            out += char(39);
            ++p;
        } else {
            out += ch;
        }
    }
    return out;
}
}

Transaction Database::beginTransaction() {
    Transaction tx;
#ifdef KNC_HAS_MARIADB
    MYSQL* conn = getConnection();
    if (!conn) return tx;
    if (mysql_real_query(conn, "START TRANSACTION", 17) != 0) {
        LOG_ERROR("DB", std::string("START TRANSACTION failed: ") + mysql_error(conn));
        releaseConnection(conn);
        return tx;
    }
    tx.m_conn = conn;
#endif
    return tx;
}

Transaction::~Transaction() {
    if (m_conn && !m_done) rollback();
#ifdef KNC_HAS_MARIADB
    if (m_conn) {
        Database::instance().releaseConnection(static_cast<MYSQL*>(m_conn));
        m_conn = nullptr;
    }
#endif
}

bool Transaction::execute(const std::string& sql, const DbParams& params) {
#ifdef KNC_HAS_MARIADB
    if (!m_conn) return false;
    const std::string q = bindParams(sql, Database::flatten(params), Database::instance());
    MYSQL* conn = static_cast<MYSQL*>(m_conn);
    if (mysql_real_query(conn, q.c_str(), static_cast<unsigned long>(q.size())) != 0) {
        LOG_ERROR("DB", std::string("tx execute failed: ") + mysql_error(conn) + " | " + q);
        return false;
    }
    return true;
#else
    (void)sql; (void)params;
    return false;
#endif
}

std::vector<std::map<std::string, std::string>> Transaction::query(
        const std::string& sql, const DbParams& params) {
    std::vector<std::map<std::string, std::string>> rows;
#ifdef KNC_HAS_MARIADB
    if (!m_conn) return rows;
    const std::string q = bindParams(sql, Database::flatten(params), Database::instance());
    MYSQL* conn = static_cast<MYSQL*>(m_conn);
    if (mysql_real_query(conn, q.c_str(), static_cast<unsigned long>(q.size())) != 0) {
        LOG_ERROR("DB", std::string("tx query failed: ") + mysql_error(conn) + " | " + q);
        return rows;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return rows;
    const unsigned nf = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        unsigned long* lens = mysql_fetch_lengths(res);
        std::map<std::string, std::string> r;
        for (unsigned i = 0; i < nf; ++i)
            r[fields[i].name] = row[i] ? std::string(row[i], lens[i]) : "";
        rows.push_back(std::move(r));
    }
    mysql_free_result(res);
#else
    (void)sql; (void)params;
#endif
    return rows;
}

uint64_t Transaction::lastInsertId() {
#ifdef KNC_HAS_MARIADB
    if (!m_conn) return 0;
    return static_cast<uint64_t>(mysql_insert_id(static_cast<MYSQL*>(m_conn)));
#else
    return 0;
#endif
}

uint64_t Transaction::affectedRows() {
#ifdef KNC_HAS_MARIADB
    if (!m_conn) return 0;
    const my_ulonglong n = mysql_affected_rows(static_cast<MYSQL*>(m_conn));
    return n == static_cast<my_ulonglong>(-1) ? 0 : static_cast<uint64_t>(n);
#else
    return 0;
#endif
}

bool Transaction::commit() {
#ifdef KNC_HAS_MARIADB
    if (!m_conn || m_done) return false;
    m_done = true;
    MYSQL* conn = static_cast<MYSQL*>(m_conn);
    if (mysql_real_query(conn, "COMMIT", 6) != 0) {
        LOG_ERROR("DB", std::string("COMMIT failed: ") + mysql_error(conn));
        return false;
    }
    return true;
#else
    return false;
#endif
}

void Transaction::rollback() {
#ifdef KNC_HAS_MARIADB
    if (!m_conn || m_done) return;
    m_done = true;
    MYSQL* conn = static_cast<MYSQL*>(m_conn);
    mysql_real_query(conn, "ROLLBACK", 8);
#endif
}

std::vector<std::string> Database::flatten(const DbParams& in) {
    std::vector<std::string> out;
    out.reserve(in.size());
    for (const DbParam& p : in) out.push_back(p.s);
    return out;
}

} // namespace knc
