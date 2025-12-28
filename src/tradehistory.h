#pragma once

#include <vector>
#include "trade.h"

namespace ob {

class TradeHistory {
private:
    std::vector<Trade> trades_;

public:
    void recordTrade(const Trade& trade) {
        trades_.push_back(trade);
    }

    const std::vector<Trade>& getTrades() const {
        return trades_;
    }
};
} // namespace ob
