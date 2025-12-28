#pragma once

#include "order.h"
#include "trade.h"

namespace ob {

enum class OrderRejectionReason {
    None,
    InvalidTIF,
    InvalidPrice,
    InvalidQuantity,
    InsufficientLiquidity,
    Other
};

struct OrderResult {
    OrderId id;
    bool accepted;
    OrderRejectionReason reason;
};

} // namespace ob