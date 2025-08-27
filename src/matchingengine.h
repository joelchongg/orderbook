#pragma once

#include "orderbook.h"
#include "tradehistory.h"

class MatchingEngine {
private:
    OrderBook& orderBook_;
    TradeHistory& tradeHistory_;    

public:
    MatchingEngine(OrderBook& orderBook, TradeHistory& tradeHistory)
        : orderBook_ { orderBook }
        , tradeHistory_ { tradeHistory }
    { }

    void matchOrders() { }

private:
    bool canMatch() { }

    void executeTrade() { }
};
