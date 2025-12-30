#pragma once

#include "order.h"

namespace ob {

class ObjectPool {
public:

ObjectPool(uint32_t initialSize);

static OrderPointer allocate(OrderId orderId, OrderType orderType, OrderSide orderSide, TimeInForce timeInForce, Price price, Quantity quantity);
static void release(OrderPointer order);

private:
    inline static std::vector<OrderPointer> expiredOrders_;
};

} // namespace ob