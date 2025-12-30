#include "matching_engine.h"

#include "order.h"
#include "utils/object_pool.h"

#include <format>
#include <numeric>

namespace ob {

void MatchingEngine::onNewOrder(OrderPointer order) {
    matchOrders(order);

    if (order->getOrderStatus() != OrderStatus::Filled) {
        if (order->getOrderType() == OrderType::Limit 
                && order->getTimeInForce() == TimeInForce::GoodTillCancel) {
            orderBook_.addOrder(order);
        }

        if (TimeInForce tif = order->getTimeInForce(); 
            tif == TimeInForce::ImmediateOrCancel || tif == TimeInForce::FillOrKill) {
            order->cancel();
        }
    }

    if (order->getOrderStatus() == OrderStatus::Filled || order->getOrderStatus() == OrderStatus::Cancelled) {
        ObjectPool::release(order);
    }
}

void MatchingEngine::onCancelOrder(OrderId orderId) {
    orderBook_.cancelOrder(orderId);
}

void MatchingEngine::matchOrders(OrderPointer& incomingOrder) {
    switch (incomingOrder->getTimeInForce()) {
        case TimeInForce::GoodTillCancel:
            [[fallthrough]];
        case TimeInForce::ImmediateOrCancel:
            if (incomingOrder->getOrderSide() == OrderSide::Buy) {
               matchWithBook(incomingOrder, orderBook_.getSellOrders());
            } else {
                matchWithBook(incomingOrder, orderBook_.getBuyOrders());
            }
            break;
        
        case TimeInForce::FillOrKill:
            if (incomingOrder->getOrderSide() == OrderSide::Buy) {
                tryToMatchWithBook(incomingOrder, orderBook_.getSellOrders());
            } else {
                tryToMatchWithBook(incomingOrder, orderBook_.getBuyOrders());
            }
            break;
        default:
            throw std::logic_error(
                std::format("Order ({}) has an unsupported TimeInForce", incomingOrder->getOrderId())); // should not execute
    }
}

bool MatchingEngine::canMatch(OrderPointer order) {
    OrderType type = order->getOrderType();
    OrderSide side = order->getOrderSide();
    if (type == OrderType::Market) {
        return side == OrderSide::Buy 
                ? !orderBook_.getSellOrders().empty() 
                : !orderBook_.getBuyOrders().empty();
    } else if (type == OrderType::Limit) {
        Price price = order->getPrice();
        if (side == OrderSide::Buy) {
            const auto& sellOrders = orderBook_.getSellOrders();
            if (sellOrders.empty()) {
                return false;
            }
            const auto& [bestAskingPrice, _] = *sellOrders.begin();
            return price >= bestAskingPrice;
        } else {
            const auto& buyOrders = orderBook_.getBuyOrders();
            if (buyOrders.empty()) {
                return false;
            }
            const auto& [bestBuyingPrice, _] = *buyOrders.begin();
            return price <= bestBuyingPrice;
        }
    } else {
        throw std::invalid_argument("Order Type not implemented yet"); // should not execute
    }
}

Trade MatchingEngine::executeTrade(const OrderPointer& incomingOrder, const OrderPointer& restingOrder) {
    OrderId incomingOrderId = incomingOrder->getOrderId();
    OrderId restingOrderId = restingOrder->getOrderId();

    if (incomingOrder->getOrderSide() == restingOrder->getOrderSide()) {
        throw std::logic_error(
            std::format(
                "Unable to execute trade between incoming order ({}) and resting order ({}) as they are of the same side", 
                incomingOrderId,
                restingOrderId));
    }

    if (incomingOrder->getOrderStatus() == OrderStatus::Cancelled || restingOrder->getOrderStatus() == OrderStatus::Cancelled) {
        throw std::logic_error(
            std::format(
                "Unable to execute trade as one or both orders are cancelled. Incoming order ({}), Resting order ({})",
                incomingOrderId,
                restingOrderId));
    }

    Quantity orderQuantity = std::min(incomingOrder->getRemainingQuantity(), restingOrder->getRemainingQuantity());
    Price orderPrice = restingOrder->getPrice();
    incomingOrder->fill(orderQuantity);
    restingOrder->fill(orderQuantity);

    // URVO
    if (incomingOrder->getOrderSide() == OrderSide::Buy) {
        return Trade::createTrade(incomingOrder, restingOrder, orderPrice, orderQuantity);
    } else {
        return Trade::createTrade(restingOrder, incomingOrder, orderPrice, orderQuantity);
    }
}

template <typename BookType>
void MatchingEngine::matchWithBook(OrderPointer& incomingOrder, BookType& oppositeBook) {
    while (incomingOrder->getRemainingQuantity() > 0) {
        if (!canMatch(incomingOrder)) {
            break;
        }
        
        auto& [_, ordersAtPrice] = *oppositeBook.begin();
        auto& restingOrder = ordersAtPrice.front();
        Trade trade = executeTrade(incomingOrder, restingOrder);
        tradeHistory_.recordTrade(trade);

        if (restingOrder->getOrderStatus() == OrderStatus::Filled) {
            orderBook_.removeOrder(restingOrder->getOrderId());
        }
    }
}

template <typename BookType>
void MatchingEngine::tryToMatchWithBook(OrderPointer& incomingOrder, BookType& oppositeBook) {
    OrderPointers entries;
    Quantity qtyNeeded = incomingOrder->getInitialQuantity();
    auto levelIt = oppositeBook.begin();
    while (qtyNeeded > 0 && levelIt != oppositeBook.end()) {
        const auto& [price, ordersAtPrice] = *levelIt;
        if ((incomingOrder->getOrderSide() == OrderSide::Buy && price > incomingOrder->getPrice())
            || (incomingOrder->getOrderSide() == OrderSide::Sell && price < incomingOrder->getPrice())) {
            break;
        }

        Quantity totalQtyAtPrice = std::accumulate(ordersAtPrice.begin(), ordersAtPrice.end(), 0, 
                                                        [](Quantity acc, const OrderPointer& order) {
                                                            return acc + order->getRemainingQuantity();
                                                        });
        if (totalQtyAtPrice < qtyNeeded) {
            entries.insert(entries.end(), ordersAtPrice.begin(), ordersAtPrice.end());
            qtyNeeded -= totalQtyAtPrice;
        } else {
            auto orderIt = ordersAtPrice.begin();
            while (qtyNeeded > 0 && orderIt != ordersAtPrice.end()) {
                Quantity currQty = std::min(qtyNeeded, (*orderIt)->getRemainingQuantity());
                entries.push_back(*orderIt);
                qtyNeeded -= currQty;
                ++orderIt;
            }
        }
        ++levelIt;
    }

    if (qtyNeeded == 0) {
        for (const auto& entry : entries) {
            Trade trade = executeTrade(incomingOrder, entry);
            tradeHistory_.recordTrade(trade);

            if (entry->getOrderStatus() == OrderStatus::Filled) {
                orderBook_.removeOrder(entry->getOrderId());
            }
        }
    }
}

} // namespace ob