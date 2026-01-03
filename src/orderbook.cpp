#include "orderbook.h"

#include "order.h"
#include "utils/object_pool.h"

#include <format>

namespace ob {

void OrderBook::addOrder(OrderPointer order) {
    Price orderPrice = order->getPrice();
    orders_.emplace(order->getOrderId(), order);
    if (order->getOrderSide() == OrderSide::Buy) {
        buyOrders_[orderPrice].push_back(order);
    } else {
        sellOrders_[orderPrice].push_back(order);
    }
}

void OrderBook::removeOrder(OrderId orderId) {
    const auto& it = orders_.find(orderId);
    if (it == orders_.end()) {
        // might want to log
        return;
    }

    auto order = it->second; // if make as a reference, need to swap below orders_.erase(it) and ObjectPool::release to avoid seg fault due to reference erased UB
    Price orderPrice = order->getPrice();
    if (order->getOrderSide() == OrderSide::Buy) {
        auto& ordersAtPriceLevel = buyOrders_[orderPrice];
        auto it = std::find(ordersAtPriceLevel.begin(), ordersAtPriceLevel.end(), order);
        ordersAtPriceLevel.erase(it);
        if (buyOrders_[orderPrice].empty()) {
            buyOrders_.erase(orderPrice);
        }
    } else {
        auto& ordersAtPriceLevel = sellOrders_[orderPrice];
        auto it = std::find(ordersAtPriceLevel.begin(), ordersAtPriceLevel.end(), order);
        ordersAtPriceLevel.erase(it);
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
    auto order = it->second;
    order->cancel();
    removeOrder(orderId);    
}
} // namespace ob
