/// the quest mode of the scenario menu its records its flow and its pendant rewards

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include "net/Packet.h"
#include "packets/gen/ScenarioPackets.h"
#include "util/PendantRules.h"

using namespace knc;

namespace {

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= p.size()) std::memcpy(&v, p.data() + at, 4);
    return v;
}

std::string slotAt(const std::vector<uint8_t>& p, size_t at, size_t size) {
    std::string s;
    for (size_t i = at; i < at + size && i < p.size() && p[i] != 0; ++i) s.push_back(static_cast<char>(p[i]));
    return s;
}

bool slotTerminated(const std::vector<uint8_t>& p, size_t at, size_t size) {
    for (size_t i = at; i < at + size && i < p.size(); ++i)
        if (p[i] == 0) return true;
    return false;
}

ScenarioPackets::ScenarioDefWire quest(uint32_t key, uint32_t level, uint32_t category = ScenarioPackets::REWARD_NONE,
                                      uint32_t rewardKey = 0) {
    ScenarioPackets::ScenarioDefWire d;
    d.scenarioKey = key;
    d.trackId = 10;
    d.characterDefKey = 13;
    d.kartDefKey = 13008;
    d.entryFee = 50;
    d.rewardMileage = 300;
    d.rewardExp = 200;
    d.rewardCategory = category;
    d.rewardKey = rewardKey;
    d.titleKey = "Quest_01_TITLE";
    d.descKey = "Quest_01_Info";
    d.msgKeyBase = "QUEST_01_STORY";
    d.requiredLevel = level;
    return d;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::string();
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

// sub 47DAA0 reads 156 flat bytes the offsets its readers use carry the def fields
TEST(QuestMode, TheDefinitionRecordLayout) {
    ScenarioPackets::ScenarioDefWire d = quest(5, 5, ScenarioPackets::REWARD_PENDANT, 2);
    d.msgKeyBase = "QUEST_05_STORY_WITH_A_VERY_LONG_TAIL_KEY";
    const Packet pkt = ScenarioPackets::scenarioDefinition(d);
    const std::vector<uint8_t>& p = pkt.payload();
    ASSERT_EQ(pkt.opcode(), 0x00F3);
    ASSERT_EQ(p.size(), ScenarioPackets::SCENARIO_DEF_SIZE);
    EXPECT_EQ(u32At(p, 0x04), 5u);
    EXPECT_EQ(u32At(p, 0x0C), 10u);
    EXPECT_EQ(u32At(p, 0x10), 13u);
    EXPECT_EQ(u32At(p, 0x14), 13008u);
    EXPECT_EQ(u32At(p, 0x1C), 50u);
    EXPECT_EQ(u32At(p, 0x20), 300u);
    EXPECT_EQ(u32At(p, 0x24), 200u);
    EXPECT_EQ(u32At(p, 0x28), ScenarioPackets::REWARD_PENDANT);
    EXPECT_EQ(u32At(p, 0x2C), 2u);
    EXPECT_EQ(slotAt(p, 0x38, 33), "Quest_01_TITLE");
    EXPECT_EQ(slotAt(p, 0x59, 33), "Quest_01_Info");
    // sub 4B5EB0 appends SUCCESS to the story key in a 36 byte cell so it is cut to 27
    EXPECT_EQ(slotAt(p, 0x7A, 33).size(), ScenarioPackets::STORY_KEY_MAX);
    EXPECT_TRUE(slotTerminated(p, 0x38, 33));
    EXPECT_TRUE(slotTerminated(p, 0x59, 33));
    EXPECT_TRUE(slotTerminated(p, 0x7A, 33));
}

// sub 438720 derefs the catalogue row of the reward so a zero key or an unknown category draws none
TEST(QuestMode, TheWireRewardCategory) {
    EXPECT_EQ(ScenarioPackets::wireRewardCategory(7, 2), 7u);
    EXPECT_EQ(ScenarioPackets::wireRewardCategory(7, 0), ScenarioPackets::REWARD_NONE);
    EXPECT_EQ(ScenarioPackets::wireRewardCategory(9, 5), ScenarioPackets::REWARD_NONE);
    const Packet pkt = ScenarioPackets::scenarioDefinition(quest(1, 1, 0, 0));
    EXPECT_EQ(u32At(pkt.payload(), 0x28), ScenarioPackets::REWARD_NONE);
}

// sub 47DB00 clears then appends 8 byte rows and sub 47E350 appends one with the NEW badge
TEST(QuestMode, ProgressListAndAppend) {
    std::vector<ScenarioPackets::ScenarioProgressRow> rows = {{3, 0}, {2, 1}, {1, 1}};
    const Packet list = ScenarioPackets::progressList(rows);
    ASSERT_EQ(list.opcode(), 0x00F4);
    ASSERT_EQ(list.payload().size(), 4u + 8u * 3u);
    EXPECT_EQ(u32At(list.payload(), 0), 3u);
    EXPECT_EQ(u32At(list.payload(), 4), 3u);
    EXPECT_EQ(u32At(list.payload(), 8), 0u);
    std::vector<ScenarioPackets::ScenarioProgressRow> many;
    for (uint32_t k = 1; k <= 60; ++k) many.push_back({k, 0});
    EXPECT_EQ(ScenarioPackets::progressList(many).payload().size(), 4u + 8u * 50u);
    const Packet one = ScenarioPackets::scenarioProgressAppend({4, 0});
    ASSERT_EQ(one.opcode(), 0x00F9);
    EXPECT_EQ(one.payload().size(), 8u);
    EXPECT_EQ(ScenarioPackets::menuOpenAck().opcode(), 0x011C);
    EXPECT_TRUE(ScenarioPackets::menuOpenAck().payload().empty());
}

// sub 47DFA0 reads kind flag and key then the wallet on kind 2 and 3 and the tail on 3
TEST(QuestMode, TheResultFrames) {
    ScenarioPackets::ScenarioResultWire fail;
    fail.scenarioKey = 5;
    EXPECT_EQ(ScenarioPackets::scenarioResult(fail).payload().size(), ScenarioPackets::RESULT_BASE_SIZE);

    ScenarioPackets::ScenarioResultWire again = fail;
    again.kind = ScenarioPackets::RESULT_CLEARED;
    again.flag = 1;
    EXPECT_EQ(ScenarioPackets::scenarioResult(again).payload().size(), 13u);

    ScenarioPackets::ScenarioResultWire paid = again;
    paid.kind = ScenarioPackets::RESULT_PAID;
    paid.goldAfter = 1234;
    paid.expAfter = 99;
    const Packet p2 = ScenarioPackets::scenarioResult(paid);
    ASSERT_EQ(p2.payload().size(), ScenarioPackets::RESULT_PAID_SIZE);
    EXPECT_EQ(p2.payload()[4], 1u);
    EXPECT_EQ(u32At(p2.payload(), 5), 5u);
    EXPECT_EQ(u32At(p2.payload(), 13), 1234u);
    EXPECT_EQ(u32At(p2.payload(), 17), 99u);

    ScenarioPackets::ScenarioResultWire item = paid;
    item.kind = ScenarioPackets::RESULT_PAID_ITEM;
    item.tail = ScenarioPackets::pendantRewardTail(2);
    const Packet p3 = ScenarioPackets::scenarioResult(item);
    ASSERT_EQ(p3.payload().size(), 21u + 8u);
    // case 7 drops the key then appends the instance and key row like 0x011A
    const auto row = pendantRow(2);
    EXPECT_EQ(std::vector<uint8_t>(p3.payload().begin() + 21, p3.payload().end()),
              std::vector<uint8_t>(row.begin(), row.end()));
}

// the typed tail of kind 3 by def category the sizes of the owned records the client reads
TEST(QuestMode, RewardTailSizesPerCategory) {
    EXPECT_EQ(ScenarioPackets::rewardTailSize(0), 0x2Cu);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(1), 0x38u);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(2), 0x1Cu);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(3), 0x1Cu);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(4), 0x1Cu);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(5), 0x30u);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(6, false), 0x85u);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(6, true), 0xB9u);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(7), 8u);
    EXPECT_EQ(ScenarioPackets::rewardTailSize(8), 0u);
}

// a new quest opens at every level rows go highest first and sub 437F30 wants the row under cleared
TEST(QuestMode, TheLockRule) {
    const std::vector<ScenarioPackets::ScenarioDefWire> defs = {quest(1, 1), quest(2, 2), quest(3, 3),
                                                               quest(4, 4), quest(5, 5)};
    const auto rows = ScenarioPackets::visibleRows(defs, {1, 2}, 3);
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].key, 3u);
    EXPECT_EQ(rows[0].cleared, 0u);
    EXPECT_EQ(rows[2].key, 1u);
    EXPECT_EQ(rows[2].cleared, 1u);
    EXPECT_TRUE(ScenarioPackets::rowPlayable(rows, 3));
    EXPECT_TRUE(ScenarioPackets::rowPlayable(rows, 1));
    EXPECT_FALSE(ScenarioPackets::rowPlayable(rows, 4));

    const auto locked = ScenarioPackets::visibleRows(defs, {1}, 3);
    EXPECT_FALSE(ScenarioPackets::rowPlayable(locked, 3));
    EXPECT_TRUE(ScenarioPackets::rowPlayable(locked, 2));

    // a cleared row never hides again even under its level
    const auto kept = ScenarioPackets::visibleRows(defs, {4}, 1);
    ASSERT_EQ(kept.size(), 2u);
    EXPECT_EQ(kept[0].key, 4u);

    // the rows a level up opens are the 0x00F9 appends
    const auto after = ScenarioPackets::visibleRows(defs, {1, 2, 3}, 5);
    const auto fresh = ScenarioPackets::newRows(rows, after);
    ASSERT_EQ(fresh.size(), 2u);
    EXPECT_EQ(fresh[0].key, 5u);
    EXPECT_EQ(fresh[1].key, 4u);
}

// the server judges the run the client reports only its race time
TEST(QuestMode, TheResultDecision) {
    using SP = ScenarioPackets;
    EXPECT_FALSE(SP::decideResult(false, false, 60000, 70000, 0, 0, false).success);
    EXPECT_FALSE(SP::decideResult(true, false, 10000, 70000, 0, 0, false).success);
    EXPECT_FALSE(SP::decideResult(true, false, 90000, 70000, 0, 0, false).success);
    EXPECT_FALSE(SP::decideResult(true, false, 60000, 70000, 55000, 0, false).success);
    EXPECT_FALSE(SP::decideResult(true, false, 60000, 70000, 0, 50000, false).success);

    const auto first = SP::decideResult(true, false, 60000, 70000, 65000, 0, true);
    EXPECT_TRUE(first.success);
    EXPECT_TRUE(first.firstClear);
    EXPECT_EQ(first.kind, SP::RESULT_PAID_ITEM);
    EXPECT_EQ(first.flag, 1u);
    EXPECT_EQ(SP::decideResult(true, false, 60000, 70000, 0, 0, false).kind, SP::RESULT_PAID);

    const auto repeat = SP::decideResult(true, true, 60000, 70000, 0, 0, true);
    EXPECT_TRUE(repeat.success);
    EXPECT_FALSE(repeat.firstClear);
    EXPECT_EQ(repeat.kind, SP::RESULT_CLEARED);
}

// a def that would crash or stall the client never reaches the wire
TEST(QuestMode, UnsafeDefinitionsStayOff) {
    const auto d = quest(3, 3);
    EXPECT_TRUE(ScenarioPackets::defProblem(d, true, true, true, true, "Frankie").empty());
    EXPECT_FALSE(ScenarioPackets::defProblem(d, false, true, true, true, "Frankie").empty());
    EXPECT_FALSE(ScenarioPackets::defProblem(d, true, false, true, true, "Frankie").empty());
    EXPECT_FALSE(ScenarioPackets::defProblem(d, true, true, false, true, "Frankie").empty());
    // sub 47DD10 swprintf of the rival name overruns 14 wchar so Prince Waddles III is out
    EXPECT_FALSE(ScenarioPackets::defProblem(d, true, true, true, true, "Prince Waddles III").empty());
    auto pendant = quest(5, 5, ScenarioPackets::REWARD_PENDANT, 2);
    EXPECT_FALSE(ScenarioPackets::defProblem(pendant, true, true, true, false, "Huck").empty());
    EXPECT_TRUE(ScenarioPackets::defProblem(pendant, true, true, true, true, "Huck").empty());
}

// C2S 0xF5 is the key and C2S 0xF8 the key and the race time
TEST(QuestMode, TheClientRequests) {
    Packet select = Packet::fromCmdFull(0x00F5);
    select.writeUInt32(7);
    ScenarioPackets::StageSelectReq s;
    ASSERT_TRUE(ScenarioPackets::parseStageSelect(select, s));
    EXPECT_EQ(s.key, 7u);
    Packet report = Packet::fromCmdFull(0x00F8);
    report.writeUInt32(7);
    report.writeUInt32(61234);
    ScenarioPackets::ResultReportReq r;
    ASSERT_TRUE(ScenarioPackets::parseResultReport(report, r));
    EXPECT_EQ(r.scenarioKey, 7u);
    EXPECT_EQ(r.resultValue, 61234u);
    Packet shortOne = Packet::fromCmdFull(0x00F5);
    EXPECT_FALSE(ScenarioPackets::parseStageSelect(shortOne, s));
}

// migration 069 seeds twenty quests and quest 5 10 15 20 give the pendants PendantRules names
TEST(QuestMode, Migration069SeedsTheTwentyQuests) {
    const std::string sql = readFile(std::string(KNC_REPO_ROOT) + "/server/scripts/069_quest_mode.sql");
    ASSERT_FALSE(sql.empty()) << "migration 069 is missing";
    const std::regex row(R"(\(\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d),\s*(\d+),\s*'(Quest_\d\d_TITLE)')");
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> rewards;
    for (auto it = std::sregex_iterator(sql.begin(), sql.end(), row); it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        rewards[static_cast<uint32_t>(std::stoul(m[1]))] = {static_cast<uint32_t>(std::stoul(m[8])),
                                                            static_cast<uint32_t>(std::stoul(m[9]))};
    }
    ASSERT_EQ(rewards.size(), 20u);
    for (const auto& kv : rewards) {
        const uint32_t pendant = questPendantFor(kv.first);
        if (pendant != 0) {
            EXPECT_EQ(kv.second.first, ScenarioPackets::REWARD_PENDANT) << "quest " << kv.first;
            EXPECT_EQ(kv.second.second, pendant) << "quest " << kv.first;
        } else {
            EXPECT_EQ(kv.second.first, ScenarioPackets::REWARD_NONE) << "quest " << kv.first;
        }
    }
    EXPECT_EQ(sql.find("DROP TABLE"), std::string::npos);
}
