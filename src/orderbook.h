#pragma once

#include "trade.h"
#include "order.h"

#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>

namespace ob {

using OrderPointers = std::vector<OrderPointer>;

class OrderBook {
public:

    ~OrderBook() {
        clear();
    }

    void addOrder(OrderPointer order);
    void removeOrder(OrderId orderId);
    void cancelOrder(OrderId orderId);
    
    std::map<Price, OrderPointers, std::greater<Price>>& getBuyOrders() { return buyOrders_; }
    std::map<Price, OrderPointers, std::less<Price>>& getSellOrders() { return sellOrders_; }
    std::unordered_map<OrderId, OrderPointer>& getOrders() { return orders_; }

private:
    std::map<Price, OrderPointers, std::greater<Price>> buyOrders_; // highest price first
    std::map<Price, OrderPointers, std::less<Price>> sellOrders_;   // lowest price first
    std::unordered_map<OrderId, OrderPointer> orders_;

    // for google benchmark
    void clear() {
        for (auto it : orders_) {
            delete it.second;
        }
    }
};
} // namespace ob
