#pragma once

#include "order.h"
#include "orderbook.h"
#include "tradehistory.h"

namespace ob {

class MatchingEngine {
private:
    OrderBook& orderBook_;
    TradeHistory& tradeHistory_;    

public:
    MatchingEngine(OrderBook& orderBook, TradeHistory& tradeHistory)
        : orderBook_ { orderBook }
        , tradeHistory_ { tradeHistory }
    { }

    void onNewOrder(OrderPointer order);
    void onCancelOrder(OrderId id);

private:
    bool canMatch(OrderPointer order);
    Trade executeTrade(const OrderPointer& buyOrder, const OrderPointer& sellOrder);
    void matchOrders(OrderPointer& incomingOrder);
    
    template <typename BookType>
    void matchWithBook(OrderPointer& incomingOrder, BookType& oppositeBook);
    template <typename BookType>
    void tryToMatchWithBook(OrderPointer& incomingOrder, BookType& oppositeBook);
};
} // namespace ob
