#include "object_pool.h"

#include "order.h"

namespace ob {

ObjectPool::ObjectPool(uint32_t initialSize) {
    expiredOrders_.reserve(initialSize);
    while (initialSize > 0) {
        expiredOrders_.push_back(Order::createDummyOrder());
        --initialSize;
    }
}

void ObjectPool::release(OrderPointer order) {
    expiredOrders_.push_back(order);
    if (expiredOrders_.size() > 500) {
        for (auto it = expiredOrders_.begin() + 250; it != expiredOrders_.end(); ++it) {
            delete *it;
        }
        expiredOrders_.resize(250);
    }
}

OrderPointer ObjectPool::allocate(OrderId orderId, OrderType orderType, OrderSide orderSide, 
                                  TimeInForce timeInForce, Price price, Quantity quantity) {
    if (!expiredOrders_.empty()) {
        OrderPointer newOrder = expiredOrders_.back();
        expiredOrders_.pop_back();
        newOrder->setOrderId(orderId);
        newOrder->setOrderType(orderType);
        newOrder->setOrderSide(orderSide);
        newOrder->setTimeInForce(timeInForce);
        newOrder->setPrice(price);
        newOrder->setInitialQuantity(quantity);
        newOrder->setRemainingQuantity(quantity);
        newOrder->setOrderStatus(OrderStatus::New);
        return newOrder;
    } else {
        return new Order{orderId, orderType, orderSide, timeInForce, price, quantity};
    }
}

}