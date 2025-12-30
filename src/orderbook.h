#pragma once

#include "trade.h"
#include "order.h"

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>

namespace ob {

using OrderPointers = std::list<OrderPointer>;
using Trades = std::vector<Trade>;

class OrderBook {
private:
    struct OrderEntry {
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location_;
    };

public:
    void addOrder(OrderPointer order);
    void removeOrder(OrderId orderId);
    void cancelOrder(OrderId orderId);
    
    std::map<Price, OrderPointers, std::greater<Price>>& getBuyOrders() { return buyOrders_; }
    std::map<Price, OrderPointers, std::less<Price>>& getSellOrders() { return sellOrders_; }
    std::unordered_map<OrderId, OrderEntry>& getOrders() { return orders_; }

private:
    std::map<Price, OrderPointers, std::greater<Price>> buyOrders_; // highest price first
    std::map<Price, OrderPointers, std::less<Price>> sellOrders_;   // 
    std::unordered_map<OrderId, OrderEntry> orders_;


};
} // namespace ob
