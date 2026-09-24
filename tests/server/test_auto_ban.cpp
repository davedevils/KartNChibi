#include <gtest/gtest.h>

#include "util/AutoBan.h"

// the user rule 10 min then 1 h 3 h 1 day 7 days 30 days capped there
TEST(AutoBan, EachEarlierBanTakesTheNextStep) {
    EXPECT_EQ(knc::autoBanMinutes(0), 10);
    EXPECT_EQ(knc::autoBanMinutes(1), 60);
    EXPECT_EQ(knc::autoBanMinutes(2), 180);
    EXPECT_EQ(knc::autoBanMinutes(3), 1440);
    EXPECT_EQ(knc::autoBanMinutes(4), 10080);
    EXPECT_EQ(knc::autoBanMinutes(5), 43200);
}

TEST(AutoBan, TheMonthIsTheCap) {
    EXPECT_EQ(knc::autoBanMinutes(6), 43200);
    EXPECT_EQ(knc::autoBanMinutes(40), 43200);
    EXPECT_EQ(knc::autoBanMinutes(-3), 10);
}
