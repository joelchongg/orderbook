#pragma once

#include <vector>
#include "trade.h"

namespace ob {

class TradeHistory {
private:
    std::vector<TradePointer> trades_;

public:

    ~TradeHistory() {
        clear();
    }

    void recordTrade(TradePointer trade) {
        trades_.push_back(trade);
    }

    const std::vector<TradePointer>& getTrades() const {
        return trades_;
    }

private:
    // for google benchmark
    void clear() {
        for (auto trade : trades_) {
            delete trade;
        }
    }
};
} // namespace ob
