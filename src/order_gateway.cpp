#include "order_events.h"
#include "order_gateway.h"

namespace ob {

OrderResult OrderGateway::submitOrder(OrderPointer order) {
    if (order->getPrice() <= 0) {
        return {order->getOrderId(), false, OrderRejectionReason::InvalidPrice};
    }

    if (order->getInitialQuantity() <= 0) {
        return {order->getOrderId(), false, OrderRejectionReason::InvalidQuantity};
    }

    if (order->getOrderType() == OrderType::Market && order->getTimeInForce() == TimeInForce::GoodTillCancel) {
        return {order->getOrderId(), false, OrderRejectionReason::InvalidTIF};
    }
    
    try {
        engine_.onNewOrder(order);
    } catch (std::exception& e) {
        return {order->getOrderId(), false, OrderRejectionReason::Other};
    }

    if ((order->getTimeInForce() == TimeInForce::FillOrKill || order->getTimeInForce() == TimeInForce::ImmediateOrCancel) 
        && order->getOrderStatus() == OrderStatus::Cancelled) {
        return {order->getOrderId(), false, OrderRejectionReason::InsufficientLiquidity};
    }

    return {order->getOrderId(), true, OrderRejectionReason::None};
}

OrderResult OrderGateway::cancelOrder(OrderId orderId) {
    engine_.onCancelOrder(orderId);
    return {orderId, true, OrderRejectionReason::None};
}

} // namespace ob