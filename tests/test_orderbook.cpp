#include <catch2/catch_all.hpp>
#include "order.h"
#include "orderbook.h"
#include "utils/object_pool.h"
#include "order_events.h"
#include "order_gateway.h"
#include "matching_engine.h"
#include "tradehistory.h"

using namespace ob;

static OrderPointer make_order(OrderId id, OrderType type, TimeInForce timeInForce, OrderSide side, Price price, Quantity qty) {
    return new Order{id, type, side, timeInForce, price, qty};
}

TEST_CASE("Add orders populates correct price levels") {
    OrderBook book;
    auto buy = make_order(1, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 50);
    auto sell = make_order(2, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 101, 25);

    book.addOrder(buy);
    book.addOrder(sell);

    auto& buys = book.getBuyOrders();
    auto& sells = book.getSellOrders();

    REQUIRE_FALSE(buys.empty());
    REQUIRE_FALSE(sells.empty());
    REQUIRE(buys.begin()->first == 100);
    REQUIRE(sells.begin()->first == 101);
    REQUIRE(buys.begin()->second.size() == 1);
    REQUIRE(sells.begin()->second.size() == 1);
}

TEST_CASE("Partial match updates quantities and records trade") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);

    auto restingBuy = make_order(10, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 50);
    book.addOrder(restingBuy);

    auto incomingSell = make_order(11, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20);
    engine.onNewOrder(incomingSell);

    const auto& trades = history.getTrades();
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0]->tradePrice_ == 100);
    REQUIRE(trades[0]->tradeQuantity_ == 20);
    REQUIRE(restingBuy->getRemainingQuantity() == 30);
    REQUIRE(incomingSell->getRemainingQuantity() == 0);
    REQUIRE(incomingSell->getOrderStatus() == OrderStatus::Filled);
    REQUIRE(restingBuy->getOrderStatus() == OrderStatus::Partial);
}

TEST_CASE("Full fill erases price level") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);

    auto restingBuy = make_order(20, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 30);
    book.addOrder(restingBuy);

    auto incomingSell = make_order(21, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 30);
    engine.onNewOrder(incomingSell);

    auto& buys = book.getBuyOrders();
    REQUIRE(buys.empty());
    REQUIRE(history.getTrades().size() == 1);
}

TEST_CASE("Cancel removes order and marks cancelled") {
    OrderBook book;
    auto buy = make_order(30, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 10);
    book.addOrder(buy);

    book.cancelOrder(30);

    auto& buys = book.getBuyOrders();
    REQUIRE(buys.empty());
    REQUIRE(buy->getOrderStatus() == OrderStatus::Cancelled);
}

TEST_CASE("Cancel non-existent order is a no-op") {
    OrderBook book;
    // No orders added; cancelling should not throw and should leave books empty
    REQUIRE_NOTHROW(book.cancelOrder(99999));
    REQUIRE(book.getBuyOrders().empty());
    REQUIRE(book.getSellOrders().empty());
}

TEST_CASE("Cancelling a filled order throws") {
    auto filled = make_order(400, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 10);
    filled->fill(10);
    REQUIRE_THROWS_AS(filled->cancel(), std::logic_error);
}

TEST_CASE("No match when buy below best ask") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(100, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));
    auto incomingBuy = make_order(101, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 99, 20);
    engine.onNewOrder(incomingBuy);
    const auto& trades = history.getTrades();
    REQUIRE(trades.empty());
    auto& buys = book.getBuyOrders();
    auto& sells = book.getSellOrders();
    REQUIRE(buys.size() == 1);
    REQUIRE(buys.begin()->first == 99);
    REQUIRE(buys.begin()->second.size() == 1);
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 100);
}

TEST_CASE("No match when sell above best bid") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(110, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 20));
    auto incomingSell = make_order(111, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 101, 20);
    engine.onNewOrder(incomingSell);
    const auto& trades = history.getTrades();
    REQUIRE(trades.empty());
    auto& buys = book.getBuyOrders();
    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 101);
    REQUIRE(sells.begin()->second.size() == 1);
    REQUIRE(buys.size() == 1);
    REQUIRE(buys.begin()->first == 100);
}

TEST_CASE("Buy consumes across two sell levels") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(200, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 15));
    book.addOrder(make_order(201, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));
    auto incomingBuy = make_order(202, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 30);
    engine.onNewOrder(incomingBuy);
    const auto& trades = history.getTrades();
    REQUIRE(trades.size() == 2);
    REQUIRE(trades[0]->tradePrice_ == 99);
    REQUIRE(trades[0]->tradeQuantity_ == 15);
    REQUIRE(trades[1]->tradePrice_ == 100);
    REQUIRE(trades[1]->tradeQuantity_ == 15);
    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 100);
    REQUIRE(sells.begin()->second.size() == 1);
    auto remainingSell = sells.begin()->second.front();
    REQUIRE(remainingSell->getRemainingQuantity() == 5);
}

TEST_CASE("Sell matches only at best bid price") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(300, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 25));
    book.addOrder(make_order(301, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 99, 10));
    auto incomingSell = make_order(302, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 30);
    engine.onNewOrder(incomingSell);
    const auto& trades = history.getTrades();
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0]->tradePrice_ == 100);
    REQUIRE(trades[0]->tradeQuantity_ == 25);
    auto& buys = book.getBuyOrders();
    REQUIRE(buys.size() == 1);
    REQUIRE(buys.begin()->first == 99);
    REQUIRE(buys.begin()->second.size() == 1);
    auto remainingBuy = buys.begin()->second.front();
    REQUIRE(remainingBuy->getRemainingQuantity() == 10);
    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 100);
    auto remainingIncoming = sells.begin()->second.front();
    REQUIRE(remainingIncoming->getRemainingQuantity() == 5);
}

TEST_CASE("FIFO at same price (sell side)") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    auto buyA = make_order(1000, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 10);
    auto buyB = make_order(1001, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 10);
    auto buyC = make_order(1002, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 10);
    book.addOrder(buyA);
    book.addOrder(buyB);
    book.addOrder(buyC);

    auto incomingSell = make_order(2000, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 25);
    engine.onNewOrder(incomingSell);

    REQUIRE(buyA->getRemainingQuantity() == 0);
    REQUIRE(buyB->getRemainingQuantity() == 0);
    REQUIRE(buyC->getRemainingQuantity() == 5);
}

TEST_CASE("FIFO at same price (buy side)") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    auto sellA = make_order(3000, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 10);
    auto sellB = make_order(3001, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 10);
    auto sellC = make_order(3002, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 10);
    book.addOrder(sellA);
    book.addOrder(sellB);
    book.addOrder(sellC);

    auto incomingBuy = make_order(4000, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 25);
    engine.onNewOrder(incomingBuy);

    REQUIRE(sellA->getRemainingQuantity() == 0);
    REQUIRE(sellB->getRemainingQuantity() == 0);
    REQUIRE(sellC->getRemainingQuantity() == 5);
}

TEST_CASE("OrderGateway rejects zero quantity") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history); OrderGateway gateway(engine);
    auto zeroQty = make_order(500, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 0);
    auto negativeQty = make_order(501, OrderType::Market, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, -1);
    
    OrderResult result1 = gateway.submitOrder(zeroQty);
    REQUIRE(result1.accepted == false);
    REQUIRE(result1.reason == OrderRejectionReason::InvalidQuantity);
}

TEST_CASE("OrderGateway rejects zero price") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history); OrderGateway gateway(engine);
    auto zeroPrice = make_order(500, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 0, 100);
    auto negativePrice = make_order(501, OrderType::Market, TimeInForce::ImmediateOrCancel, OrderSide::Buy, -1, 100);
    
    OrderResult result1 = gateway.submitOrder(zeroPrice);
    REQUIRE(result1.accepted == false);
    REQUIRE(result1.reason == OrderRejectionReason::InvalidPrice);
}

TEST_CASE("Trade price is always resting order price") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    // Resting sell at 99, incoming buy at 105 (willing to pay more)
    book.addOrder(make_order(600, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 10));
    auto incomingBuy = make_order(601, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 105, 10);
    engine.onNewOrder(incomingBuy);

    const auto& trades = history.getTrades();
    REQUIRE(trades.size() == 1);
    // Trade should execute at 99 (resting), not 105 (incoming)
    REQUIRE(trades[0]->tradePrice_ == 99);
    REQUIRE(trades[0]->tradeQuantity_ == 10);
}

TEST_CASE("Fully filled incoming order is removed from book") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(700, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));
    auto incomingBuy = make_order(701, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 20);
    engine.onNewOrder(incomingBuy);

    // Incoming buy should be fully filled and removed
    REQUIRE(incomingBuy->getRemainingQuantity() == 0);
    auto& buys = book.getBuyOrders();
    REQUIRE(buys.empty());
}

TEST_CASE("FIFO with 5 orders at same price") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    auto sell1 = make_order(800, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 10);
    auto sell2 = make_order(801, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20);
    auto sell3 = make_order(802, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 15);
    auto sell4 = make_order(803, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 25);
    auto sell5 = make_order(804, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 30);
    book.addOrder(sell1);
    book.addOrder(sell2);
    book.addOrder(sell3);
    book.addOrder(sell4);
    book.addOrder(sell5);

    // Incoming buy for 50: should consume sell1 (10), sell2 (20), sell3 (15), sell4 (5)
    auto incomingBuy = make_order(900, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 50);
    engine.onNewOrder(incomingBuy);

    REQUIRE(sell1->getRemainingQuantity() == 0);
    REQUIRE(sell2->getRemainingQuantity() == 0);
    REQUIRE(sell3->getRemainingQuantity() == 0);
    REQUIRE(sell4->getRemainingQuantity() == 20);
    REQUIRE(sell5->getRemainingQuantity() == 30);
}

TEST_CASE("Price priority across 3 sell levels") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(1000, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 98, 10));
    book.addOrder(make_order(1001, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 15));
    book.addOrder(make_order(1002, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));
    
    // Incoming buy for 35 units at 100: should consume 98 (10), 99 (15), 100 (10)
    auto incomingBuy = make_order(1003, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 35);
    engine.onNewOrder(incomingBuy);

    const auto& trades = history.getTrades();
    REQUIRE(trades.size() == 3);
    REQUIRE(trades[0]->tradePrice_ == 98);
    REQUIRE(trades[0]->tradeQuantity_ == 10);
    REQUIRE(trades[1]->tradePrice_ == 99);
    REQUIRE(trades[1]->tradeQuantity_ == 15);
    REQUIRE(trades[2]->tradePrice_ == 100);
    REQUIRE(trades[2]->tradeQuantity_ == 10);

    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 100);
    auto remainingSell = sells.begin()->second.front();
    REQUIRE(remainingSell->getRemainingQuantity() == 10);
}

TEST_CASE("Order state transitions: New -> Partial -> Filled") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    auto restingSell = make_order(1100, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 50);
    REQUIRE(restingSell->getOrderStatus() == OrderStatus::New);
    
    book.addOrder(restingSell);
    REQUIRE(restingSell->getOrderStatus() == OrderStatus::New);

    // First fill: partial
    auto buy1 = make_order(1101, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 20);
    engine.onNewOrder(buy1);
    REQUIRE(restingSell->getOrderStatus() == OrderStatus::Partial);
    REQUIRE(restingSell->getRemainingQuantity() == 30);

    // Second fill: still partial
    auto buy2 = make_order(1102, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 10);
    engine.onNewOrder(buy2);
    REQUIRE(restingSell->getOrderStatus() == OrderStatus::Partial);
    REQUIRE(restingSell->getRemainingQuantity() == 20);

    // Final fill: filled
    auto buy3 = make_order(1103, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 20);
    engine.onNewOrder(buy3);
    REQUIRE(restingSell->getOrderStatus() == OrderStatus::Filled);
    REQUIRE(restingSell->getRemainingQuantity() == 0);
}

TEST_CASE("Cancel a partially filled order") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    auto restingBuy = make_order(1200, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 50);
    book.addOrder(restingBuy);

    // Partial fill
    auto incomingSell = make_order(1201, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20);
    engine.onNewOrder(incomingSell);
    REQUIRE(restingBuy->getOrderStatus() == OrderStatus::Partial);
    REQUIRE(restingBuy->getRemainingQuantity() == 30);

    // Cancel the partially filled order
    book.cancelOrder(1200);
    REQUIRE(restingBuy->getOrderStatus() == OrderStatus::Cancelled);
    auto& buys = book.getBuyOrders();
    REQUIRE(buys.empty());
}

TEST_CASE("Empty price level is removed after all orders consumed") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(1300, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 10));
    book.addOrder(make_order(1301, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 15));
    book.addOrder(make_order(1302, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));

    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 2);

    // Consume all orders at 99
    auto incomingBuy = make_order(1303, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 99, 25);
    engine.onNewOrder(incomingBuy);

    // Price level 99 should be removed
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 100);
}

TEST_CASE("Market order matches all orders until all orders consumed") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(1300, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 10));
    book.addOrder(make_order(1301, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 15));
    book.addOrder(make_order(1302, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));

    auto& buys = book.getBuyOrders();
    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 2);

    // Consume all orders at 99
    auto incomingBuy1 = make_order(1303, OrderType::Market, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 99, 25); // Buy price does not matter due to market order
    engine.onNewOrder(incomingBuy1);

    // Price level 99 should be removed
    REQUIRE(sells.size() == 1);
    REQUIRE(sells.begin()->first == 100);
    REQUIRE(buys.empty());

    // Consume rest of orders at 100
    auto incomingBuy2 = make_order(1304, OrderType::Market, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, 20);
    engine.onNewOrder(incomingBuy2);
    REQUIRE(sells.empty());
    REQUIRE(buys.empty());

    int originalHistorySize = history.getTrades().size();
    auto incomingBuy3 = make_order(1304, OrderType::Market, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, 10);
    engine.onNewOrder(incomingBuy3);
    REQUIRE(buys.empty());
    REQUIRE(history.getTrades().size() == originalHistorySize); // No new trades occurred
}

TEST_CASE("Market order matches consumes all orders and is cancelled due to insufficient liquidity") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    book.addOrder(make_order(1300, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 10));
    book.addOrder(make_order(1301, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 101, 10));

    auto& buys = book.getBuyOrders();
    auto& sells = book.getSellOrders();
    REQUIRE(sells.size() == 2);

    auto incomingBuy = make_order(1302, OrderType::Market, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, 30);
    engine.onNewOrder(incomingBuy);
    REQUIRE(history.getTrades().size() == 2);
    REQUIRE(sells.empty());
    REQUIRE(buys.empty());
}

TEST_CASE("Invalid Market orders are caught (Market + GTC)") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history); OrderGateway gateway(engine);
    auto invalidMarketOrder = make_order(1300, OrderType::Market, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 30);
    OrderResult result = gateway.submitOrder(invalidMarketOrder);
    REQUIRE(result.accepted == false);
    REQUIRE(result.reason == OrderRejectionReason::InvalidTIF);
}

TEST_CASE("IOC Limit order: unfilled portion cancelled, not added to book") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    // Add only 15 qty of sell orders
    book.addOrder(make_order(1400, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 10));
    book.addOrder(make_order(1401, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 5));
    
    // IOC buy order for 25 qty - only 15 available
    auto iocBuy = make_order(1402, OrderType::Limit, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, 25);
    engine.onNewOrder(iocBuy);
    
    // Should have filled 15 and cancelled 10
    REQUIRE(iocBuy->getRemainingQuantity() == 10);
    REQUIRE(iocBuy->getOrderStatus() == OrderStatus::Cancelled);
    REQUIRE(book.getSellOrders().empty());
    
    // Unfilled portion should NOT be added to book
    REQUIRE(book.getBuyOrders().empty());
    
    // Trades recorded for 15 qty only
    REQUIRE(history.getTrades().size() == 2);
}

TEST_CASE("IOC Limit order fully matches") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    book.addOrder(make_order(1500, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 25));
    
    auto iocBuy = make_order(1501, OrderType::Limit, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, 25);
    engine.onNewOrder(iocBuy);
    
    // Should be fully filled
    REQUIRE(iocBuy->getRemainingQuantity() == 0);
    REQUIRE(iocBuy->getOrderStatus() == OrderStatus::Filled);
    REQUIRE(book.getBuyOrders().empty());
    REQUIRE(book.getSellOrders().empty());
    REQUIRE(history.getTrades().size() == 1);
}

TEST_CASE("FOK Limit order accepts full fill") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    book.addOrder(make_order(1600, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 30));
    
    auto fokBuy = make_order(1601, OrderType::Limit, TimeInForce::FillOrKill, OrderSide::Buy, 100, 30);
    engine.onNewOrder(fokBuy);
    
    // Should be fully filled
    REQUIRE(fokBuy->getRemainingQuantity() == 0);
    REQUIRE(fokBuy->getOrderStatus() == OrderStatus::Filled);
    REQUIRE(book.getBuyOrders().empty());
    REQUIRE(book.getSellOrders().empty());
    REQUIRE(history.getTrades().size() == 1);
}

TEST_CASE("FOK Limit or Market order rejects if full fill not available") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    // Add only 25 qty, FOK order wants 30
    book.addOrder(make_order(1700, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 20));
    book.addOrder(make_order(1701, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 5));
    
    auto fokLimitBuy = make_order(1702, OrderType::Limit, TimeInForce::FillOrKill, OrderSide::Buy, 100, 30);
    engine.onNewOrder(fokLimitBuy);
    
    // Should be cancelled (no partial fill)
    REQUIRE(fokLimitBuy->getRemainingQuantity() == 30);
    REQUIRE(fokLimitBuy->getOrderStatus() == OrderStatus::Cancelled);
    
    // No trades recorded since FOK failed
    REQUIRE(history.getTrades().size() == 0);
    
    // Original sell orders still on book
    REQUIRE(book.getSellOrders().size() == 1);
    REQUIRE(book.getSellOrders().begin()->second.size() == 2);

    auto fokMarketBuy = make_order(1703, OrderType::Market, TimeInForce::FillOrKill, OrderSide::Buy, 100, 30);
    engine.onNewOrder(fokMarketBuy);
    
    // Should be cancelled (no partial fill)
    REQUIRE(fokMarketBuy->getRemainingQuantity() == 30);
    REQUIRE(fokMarketBuy->getOrderStatus() == OrderStatus::Cancelled);
    
    // No trades recorded since FOK failed
    REQUIRE(history.getTrades().size() == 0);
    
    // Original sell orders still on book
    REQUIRE(book.getSellOrders().size() == 1);
    REQUIRE(book.getSellOrders().begin()->second.size() == 2); 
}

TEST_CASE("FOK with multiple price levels - fills nothing") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    // Two price levels: 100 qty at 99, 100 qty at 100
    book.addOrder(make_order(1800, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 100));
    book.addOrder(make_order(1801, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 100));
    
    auto fokBuy = make_order(1802, OrderType::Limit, TimeInForce::FillOrKill, OrderSide::Buy, 100, 150);
    engine.onNewOrder(fokBuy);
    
    // Should fill 100 at 99 and 50 at 100 = 150 total
    REQUIRE(fokBuy->getRemainingQuantity() == 0);
    REQUIRE(fokBuy->getOrderStatus() == OrderStatus::Filled);
    REQUIRE(history.getTrades().size() == 2);
    
    // First level should be consumed, second should have 50 left
    REQUIRE(book.getSellOrders().size() == 1);
    auto& [price, orders] = *book.getSellOrders().begin();
    REQUIRE(price == 100);
    REQUIRE(orders.front()->getRemainingQuantity() == 50);
}

TEST_CASE("FOK with multiple price levels - fills order") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    book.addOrder(make_order(1800, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 98, 50));
    book.addOrder(make_order(1801, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 99, 50));
    book.addOrder(make_order(1802, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 100));
    
    auto fokBuy = make_order(1803, OrderType::Limit, TimeInForce::FillOrKill, OrderSide::Buy, 100, 150);
    engine.onNewOrder(fokBuy);
    
    REQUIRE(fokBuy->getRemainingQuantity() == 0);
    REQUIRE(fokBuy->getOrderStatus() == OrderStatus::Filled);
    REQUIRE(history.getTrades().size() == 3);
    
    REQUIRE(book.getSellOrders().size() == 1);
    auto& [price, orders] = *book.getSellOrders().begin();
    REQUIRE(price == 100);
    REQUIRE(orders.front()->getRemainingQuantity() == 50);
}

TEST_CASE("FOK sell with insufficient buy liquidity") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    book.addOrder(make_order(1900, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 15));
    
    auto fokSell = make_order(1901, OrderType::Limit, TimeInForce::FillOrKill, OrderSide::Sell, 100, 25);
    engine.onNewOrder(fokSell);
    
    // Should be rejected (cannot fill 25 at price 100)
    REQUIRE(fokSell->getRemainingQuantity() == 25);
    REQUIRE(fokSell->getOrderStatus() == OrderStatus::Cancelled);
    REQUIRE(history.getTrades().size() == 0);
}

TEST_CASE("IOC sell with insufficient buy liquidity") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    book.addOrder(make_order(2000, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Buy, 100, 15));
    
    auto iocSell = make_order(2001, OrderType::Limit, TimeInForce::ImmediateOrCancel, OrderSide::Sell, 100, 25);
    engine.onNewOrder(iocSell);
    
    // Should fill 15 and cancel remaining 10
    REQUIRE(iocSell->getRemainingQuantity() == 10);
    REQUIRE(iocSell->getOrderStatus() == OrderStatus::Cancelled);
    REQUIRE(history.getTrades().size() == 1);
    
    // Unfilled portion not added to book
    REQUIRE(book.getSellOrders().empty());
}

TEST_CASE("IOC and FOK orders do not rest on book") {
    OrderBook book; TradeHistory history; MatchingEngine engine(book, history);
    
    // Add one sell order
    book.addOrder(make_order(2100, OrderType::Limit, TimeInForce::GoodTillCancel, OrderSide::Sell, 100, 5));
    
    // IOC buy order larger than available
    auto iocBuy = make_order(2101, OrderType::Limit, TimeInForce::ImmediateOrCancel, OrderSide::Buy, 100, 20);
    engine.onNewOrder(iocBuy);
    
    // Even though IOC had unfilled qty, nothing should be on buy side
    REQUIRE(book.getBuyOrders().empty());
    REQUIRE(iocBuy->getOrderStatus() == OrderStatus::Cancelled);
    
    // FOK buy order
    auto fokBuy = make_order(2102, OrderType::Limit, TimeInForce::FillOrKill, OrderSide::Buy, 100, 10);
    engine.onNewOrder(fokBuy);
    
    // FOK failed (no fill), so nothing on buy side
    REQUIRE(book.getBuyOrders().empty());
    REQUIRE(fokBuy->getOrderStatus() == OrderStatus::Cancelled);
}

