#pragma once
#include <cstdint>
#include <format>
#include <stdexcept>

enum class OrderType {
    Limit,
    Market
};

enum class OrderSide {
    Buy,
    Sell
};

enum class OrderStatus {
    New,
    Partial,
    Filled,
    Cancelled
};

using Price = std::uint32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

class Order {
private:
    OrderId orderId_;
    OrderType orderType_;
    OrderSide orderSide_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
    OrderStatus orderStatus_;

public:
    Order(OrderId orderId, OrderType orderType, OrderSide orderSide, Price price, Quantity quantity)
        : orderId_ { orderId }
        , orderType_ { orderType }
        , orderSide_ { orderSide }
        , price_ { price }
        , initialQuantity_ { quantity }
        , remainingQuantity_ { quantity }
        , orderStatus_ { OrderStatus::New }
    { }

    OrderId getOrderId() const { return orderId_; }
    OrderType getOrderType() const { return orderType_; }
    OrderSide getOrderSide() const { return orderSide_; }
    Price getPrice() const { return price_; }
    Quantity getInitialQuantity() const { return initialQuantity_; }
    Quantity getRemainingQuantity() const { return remainingQuantity_; }
    Quantity getFilledQuantity() const { return getInitialQuantity() - getRemainingQuantity(); }
    OrderStatus getOrderStatus() const { return orderStatus_; }

    void fill(Quantity quantity) {
        if (quantity == 0) return;
        if (quantity > getRemainingQuantity())
            throw std::logic_error(std::format("Cannot fill Order ({}) as its remaining quantity is less than the fill quantity.", getOrderId()));
;
        remainingQuantity_ -= quantity;

        if (remainingQuantity_ == 0) orderStatus_ = OrderStatus::Filled;
        else orderStatus_ = OrderStatus::Partial;
    }

    void cancel() {
        if (orderStatus_ == OrderStatus::Filled)
            throw std::logic_error(std::format("Cannot cancel order ({}) as it has already been filled.", getOrderId()));
        
        orderStatus_ = OrderStatus::Cancelled;
    }
};
