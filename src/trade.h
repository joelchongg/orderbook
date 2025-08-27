#pragma once

#include <cstdint>

using Price = uint32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct Trade {
    OrderId buyOrderId_;
    OrderId sellOrderId_;
    Price price_;
    Quantity quantity_;

    Trade(OrderId buyOrderId, OrderId sellOrderId, Price price, Quantity quantity)
        : buyOrderId_ { buyOrderId }
        , sellOrderId_ { sellOrderId }
        , price_ { price }
        , quantity_ { quantity_ }
    { }
};
