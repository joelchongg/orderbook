#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include "trade.h"
#include "order.h"

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;
using Trades = std::vector<Trade>;

class OrderBook {
private:
    struct OrderEntry {
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std::greater<Price>> buyOrders_;
    std::map<Price, OrderPointers, std::less<Price>> sellOrders_;
    std::unordered_map<OrderId, OrderEntry> orders_;

public:
    //TODO
    void addOrder(OrderPointer order) {}

    //TODO
    void cancelOrder(OrderId orderId) {}
};
