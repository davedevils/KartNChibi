#include <gtest/gtest.h>
#include "config/Config.h"
#include <fstream>

using namespace knc;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::ofstream f("test_config.json");
        f << R"({
            "server": {
                "port": 12345,
                "name": "Test Server"
            },
            "nested": {
                "deep": {
                    "value": 42
                }
            }
        })";
        f.close();
    }
    
    void TearDown() override {
        std::filesystem::remove("test_config.json");
    }
};

TEST_F(ConfigTest, LoadAndGet) {
    ASSERT_TRUE(Config::instance().load("test_config.json"));
    
    int port = Config::instance().get("server.port", 0);
    EXPECT_EQ(port, 12345);
    
    std::string name = Config::instance().get<std::string>("server.name", "");
    EXPECT_EQ(name, "Test Server");
}

TEST_F(ConfigTest, DefaultValue) {
    Config::instance().load("test_config.json");
    
    int missing = Config::instance().get("nonexistent.key", 999);
    EXPECT_EQ(missing, 999);
}

TEST_F(ConfigTest, NestedAccess) {
    Config::instance().load("test_config.json");
    
    int deep = Config::instance().get("nested.deep.value", 0);
    EXPECT_EQ(deep, 42);
}

