#include <catch2/catch_all.hpp>

#include "utils/object_pool.h"

using namespace ob;

static OrderPointer make_order(OrderId id, OrderSide side) {
    return ObjectPool::allocate(
        id,
        OrderType::Limit,
        side,
        TimeInForce::GoodTillCancel,
        100,
        50
    );
}

TEST_CASE("ObjectPool returns valid orders when empty") {
    ObjectPool pool(0);

    auto o1 = make_order(1, OrderSide::Buy);
    auto o2 = make_order(2, OrderSide::Sell);

    REQUIRE(o1 != nullptr);
    REQUIRE(o2 != nullptr);
    REQUIRE(o1 != o2);
    REQUIRE(o1->getOrderId() == 1);
    REQUIRE(o2->getOrderId() == 2);
}

TEST_CASE("ObjectPool reuses expired orders and rewrites fields") {
    ObjectPool pool(1); // one preallocated slot

    auto o1 = make_order(10, OrderSide::Buy);
    auto* raw1 = o1; // capture pointer
    REQUIRE(o1->getOrderId() == 10);
    REQUIRE(o1->getOrderSide() == OrderSide::Buy);
    REQUIRE(o1->getPrice() == 100);
    REQUIRE(o1->getInitialQuantity() == 50);

    // Return to pool
    ObjectPool::release(o1);

    // Request again with different fields; should reuse same pointer
    auto o2 = ObjectPool::allocate(
        20,
        OrderType::Market,
        OrderSide::Sell,
        TimeInForce::ImmediateOrCancel,
        105,
        75
    );

    REQUIRE(o2 == raw1);
    REQUIRE(o2->getOrderId() == 20);
    REQUIRE(o2->getOrderType() == OrderType::Market);
    REQUIRE(o2->getOrderSide() == OrderSide::Sell);
    REQUIRE(o2->getTimeInForce() == TimeInForce::ImmediateOrCancel);
    REQUIRE(o2->getPrice() == 105);
    REQUIRE(o2->getInitialQuantity() == 75);
    REQUIRE(o2->getRemainingQuantity() == 75);
    REQUIRE(o2->getOrderStatus() == OrderStatus::New);
}
