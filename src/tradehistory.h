#pragma once

#include <vector>
#include "trade.h"

class TradeHistory {
private:
    std::vector<Trade> trades_;

public:
    void recordTrade(const Trade& trade) { }
    const std::vector<Trade>& getTrades() const { }
};
