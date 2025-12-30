#pragma once

#include "matching_engine.h"
#include "order.h"
#include "order_events.h"

namespace ob {

class OrderGateway {
public:
    explicit OrderGateway(MatchingEngine& engine)
        : engine_ { engine }
    {}

    OrderResult submitOrder(OrderPointer order);
    OrderResult cancelOrder(OrderId orderId);

private:
    MatchingEngine& engine_;
};

} // namespace ob