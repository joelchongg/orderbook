#pragma once
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include "order.h"

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderBook {
private:
    std::map<Price, OrderPointers, std::greater<Price>> buyOrders_;
    std::map<Price, OrderPointers, std::less<Price>> sellOrders_;
    std::unordered_map<OrderId, OrderPointer> orders_;

public:
    void addOrder(OrderPointer order) {
        orders_[order->getOrderId()] = order;

        if (order->getOrderSide() == OrderSide::Sell) {
            sellOrders_[order->getPrice()].push_back(order);
        } else {
            buyOrders_[order->getPrice()].push_back(order);
        }
    }

    void cancelOrder(OrderId orderId) {
        if (orders_.find(orderId) == orders_.end())
            throw std::logic_error(std::format("Order with order id ({}) does not exist.", orderId));

        OrderPointer order = orders_[orderId];
        order->cancel();
    }

private:
    
};