#include "orderbook.h"

#include "order.h"
#include "utils/object_pool.h"

#include <format>

namespace ob {

void OrderBook::addOrder(OrderPointer order) {
    Price orderPrice = order->getPrice();
    if (order->getOrderSide() == OrderSide::Buy) {
        buyOrders_[orderPrice].push_back(order);
        auto it = std::prev(buyOrders_[orderPrice].end());
        orders_.emplace(order->getOrderId(), OrderEntry{order, it});
    } else {
        sellOrders_[orderPrice].push_back(order);
        auto it = std::prev(sellOrders_[orderPrice].end());
        orders_.emplace(order->getOrderId(), OrderEntry{order, it});
    }
}

void OrderBook::removeOrder(OrderId orderId) {
    const auto& it = orders_.find(orderId);
    if (it == orders_.end()) {
        // might want to log
        return;
    }

    auto [order, iter] = it->second;
    Price orderPrice = order->getPrice();
    if (order->getOrderSide() == OrderSide::Buy) {
        buyOrders_[orderPrice].erase(iter);
        if (buyOrders_[orderPrice].empty()) {
            buyOrders_.erase(orderPrice);
        }
    } else {
        sellOrders_[orderPrice].erase(iter);
        if (sellOrders_[orderPrice].empty()) {
            sellOrders_.erase(orderPrice);
        }
    }
    orders_.erase(it);
    ObjectPool::release(order);
}

void OrderBook::cancelOrder(OrderId orderId) {
    const auto& it = orders_.find(orderId);
    if (it == orders_.end()) {
        // might want to log
        return;
    }
    auto order = it->second.order_;
    order->cancel();
    removeOrder(orderId);    
}
} // namespace ob
