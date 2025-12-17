/**
 * @file Database.h
 * @brief MariaDB connection pool
 */

#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <map>

#ifdef KNC_HAS_MARIADB
#include <mysql.h>
#endif

namespace knc {

struct DBConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string database = "knc_emu";
    std::string user = "knc";
    std::string password = "knc_password";
    int poolSize = 5;
};

class Database {
public:
    static Database& instance() {
        static Database inst;
        return inst;
    }

    bool init(const DBConfig& config);
    void shutdown();
    
    // Execute query (INSERT, UPDATE, DELETE) - UNSAFE, use executePrepared instead!
    bool execute(const std::string& query);
    
    // Execute query with results (SELECT) - UNSAFE, use queryPrepared instead!
    std::vector<std::map<std::string, std::string>> query(const std::string& sql);
    
    // =========================================================================
    // SECURE METHODS - Use these to prevent SQL injection
    // =========================================================================
    
    // Escape a string for safe SQL usage
    std::string escapeString(const std::string& input);
    
    // Execute with prepared statement placeholders (? = placeholder)
    // Example: executePrepared("INSERT INTO users (name) VALUES (?)", {"John"})
    bool executePrepared(const std::string& sql, const std::vector<std::string>& params);
    
    // Query with prepared statement placeholders
    // Example: queryPrepared("SELECT * FROM users WHERE name = ?", {"John"})
    std::vector<std::map<std::string, std::string>> queryPrepared(
        const std::string& sql, 
        const std::vector<std::string>& params
    );

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

} // namespace knc

