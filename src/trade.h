#pragma once

#include "order.h"

#include <cstdint>
#include <memory>

namespace ob {

struct Trade {
    OrderId buyOrderId_;
    OrderId sellOrderId_;
    Price tradePrice_;
    Quantity tradeQuantity_;

    // Used for replay (eventually)
    Price buyOrderPrice_;
    OrderType buyOrderType_;
    TimeInForce buyOrderTIF_;
    Price sellOrderPrice_;
    OrderType sellOrderType_;
    TimeInForce sellOrderTIF_;

    static Trade* createTrade(const OrderPointer buyOrder, const OrderPointer sellOrder, Price tradePrice, Quantity tradeQty) {
        return new Trade{buyOrder->getOrderId(), sellOrder->getOrderId(), tradePrice, tradeQty,
                         buyOrder->getPrice(), buyOrder->getOrderType(), buyOrder->getTimeInForce(),
                         sellOrder->getPrice(), sellOrder->getOrderType(), sellOrder->getTimeInForce()};
    }
};

using TradePointer = Trade*;

} // namespace ob
