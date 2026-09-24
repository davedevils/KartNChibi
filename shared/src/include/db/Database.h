/// MariaDB connection pool

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <map>

#ifdef KNC_HAS_MARIADB
#include <mysql.h>
#endif

namespace knc {

/// one bound value so a call site can mix ints and strings in one list
struct DbParam {
    std::string s;
    DbParam(const std::string& v) : s(v) {}
    DbParam(const char* v) : s(v ? v : "") {}
    DbParam(bool v) : s(v ? "1" : "0") {}
    DbParam(int v) : s(std::to_string(v)) {}
    DbParam(unsigned v) : s(std::to_string(v)) {}
    DbParam(long v) : s(std::to_string(v)) {}
    DbParam(unsigned long v) : s(std::to_string(v)) {}
    DbParam(long long v) : s(std::to_string(v)) {}
    DbParam(unsigned long long v) : s(std::to_string(v)) {}
    DbParam(double v) : s(std::to_string(v)) {}
};

using DbParams = std::vector<DbParam>;

/// a pinned connection so a multi statement update cannot half apply since the pool hands out five connections
class Transaction {
public:
    Transaction() = default;
    ~Transaction();
    Transaction(Transaction&& o) noexcept : m_conn(o.m_conn), m_done(o.m_done) {
        o.m_conn = nullptr;
        o.m_done = true;
    }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool valid() const { return m_conn != nullptr; }
    bool execute(const std::string& sql, const DbParams& params = {});
    std::vector<std::map<std::string, std::string>> query(const std::string& sql,
                                                          const DbParams& params = {});
    uint64_t lastInsertId();
    /// rows the last statement touched an update that matched nothing is not an error
    uint64_t affectedRows();
    bool commit();
    void rollback();

private:
    friend class Database;
    void* m_conn = nullptr;
    bool  m_done = false;
};


struct DBConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string database = "knc_emu";
    std::string user = "knc";
    std::string password = "knc_password";
    int poolSize = 5;
};

class Database {
    friend class Transaction;

public:
    static Database& instance() {
        static Database inst;
        return inst;
    }

    bool init(const DBConfig& config);
    void shutdown();
    
    // execute query INSERT UPDATE DELETE unsafe use executePrepared instead
    bool execute(const std::string& query);
    
    // execute query with results SELECT unsafe use queryPrepared instead
    std::vector<std::map<std::string, std::string>> query(const std::string& sql);
    
    // secure methods use these to prevent SQL injection

    std::string escapeString(const std::string& input);
    
    // execute with prepared statement placeholders example executePrepared insert into users name values with John
    bool executePreparedRaw(const std::string& sql, const std::vector<std::string>& params);
    
    // query with prepared statement placeholders example queryPrepared select from users where name equals John
    std::vector<std::map<std::string, std::string>> queryPreparedRaw(
        const std::string& sql, 
        const std::vector<std::string>& params
    );

    /// one pinned connection see class Transaction below
    Transaction beginTransaction();

    /// auto increment id of the last insert on this connection
    uint64_t lastInsertId();


    static std::vector<std::string> flatten(const DbParams& in);

    bool executePrepared(const std::string& sql, const DbParams& params) {
        return executePreparedRaw(sql, flatten(params));
    }

    std::vector<std::map<std::string, std::string>> queryPrepared(
        const std::string& sql, const DbParams& params) {
        return queryPreparedRaw(sql, flatten(params));
    }

private:
    Database() = default;
    ~Database() { shutdown(); }
    
#ifdef KNC_HAS_MARIADB
    MYSQL* getConnection();
    void releaseConnection(MYSQL* conn);
    std::queue<MYSQL*> m_pool;
#endif
    
    std::mutex m_mutex;
    DBConfig m_config;
    bool m_initialized = false;
};


}
