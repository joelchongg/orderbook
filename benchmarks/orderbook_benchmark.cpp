#include <benchmark/benchmark.h>
#include "matching_engine.h"
#include "order_gateway.h"
#include "orderbook.h"
#include "utils/object_pool.h"
#include "tradehistory.h"

using namespace ob;

// Single order add to empty book
static void BM_AddOrder_Empty(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    for (auto _ : state) {
        auto order = pool.allocate(
            order_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100,
            100
        );
        engine.onNewOrder(order);
        benchmark::DoNotOptimize(order);
    }
}
BENCHMARK(BM_AddOrder_Empty);

// Single order add with existing orders (no match)
static void BM_AddOrder_NoMatch(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    for (int i = 0; i < 50; ++i) {
        auto sell = pool.allocate(
            i,
            OrderType::Limit,
            OrderSide::Sell,
            TimeInForce::GoodTillCancel,
            105 + i,
            100
        );
        book.addOrder(sell);
    }
    
    int buy_id = 1000;
    for (auto _ : state) {
        auto buy = pool.allocate(
            buy_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100,  // buy price below all sell price
            100
        );
        engine.onNewOrder(buy);
        benchmark::DoNotOptimize(buy);
    }
}
BENCHMARK(BM_AddOrder_NoMatch);

// Single order match (best case - instant fill)
static void BM_Match_SingleLevel(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int buy_id = 1000;
    for (auto _ : state) {
        auto sell_order = pool.allocate(
            buy_id - 1,
            OrderType::Limit,
            OrderSide::Sell,
            TimeInForce::GoodTillCancel,
            100,
            100
        );
        book.addOrder(sell_order);
        
        auto buy = pool.allocate(
            buy_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100,
            100
        );
        engine.onNewOrder(buy);
        benchmark::DoNotOptimize(buy);
    }
}
BENCHMARK(BM_Match_SingleLevel);

// Match across multiple price levels and add to book
static void BM_Match_MultiLevel(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        state.PauseTiming();

        for (int level = 0; level < 10; ++level) {
            for (int i = 0; i < 10; ++i) {
                auto sell = pool.allocate(
                    order_id++,
                    OrderType::Limit,
                    OrderSide::Sell,
                    TimeInForce::GoodTillCancel,
                    100 + level,  // prices 100, 101, 102, ...
                    10
                );
                book.addOrder(sell);
            }
        }
        
        state.ResumeTiming();
        
        auto buy = pool.allocate(
            order_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            110,  // will match across all levels
            500   // > total sell orders quantity
        );
        engine.onNewOrder(buy);
        benchmark::DoNotOptimize(buy);
    }
}
BENCHMARK(BM_Match_MultiLevel);

// Market order execution
static void BM_MarketOrder(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        state.PauseTiming();

        for (int i = 0; i < 20; ++i) {
            auto sell = pool.allocate(
                order_id++,
                OrderType::Limit,
                OrderSide::Sell,
                TimeInForce::GoodTillCancel,
                100 + (i / 5),  // 5 orders per price level
                50
            );
            book.addOrder(sell);
        }
        
        state.ResumeTiming();
        
        auto market_buy = pool.allocate(
            order_id++,
            OrderType::Market,
            OrderSide::Buy,
            TimeInForce::ImmediateOrCancel,
            0,     // price irrelevant for market orders
            300
        );
        engine.onNewOrder(market_buy);
        benchmark::DoNotOptimize(market_buy);
    }
}
BENCHMARK(BM_MarketOrder);

// IOC (Immediate-or-Cancel) order
static void BM_IOC_Order(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        state.PauseTiming();

        for (int i = 0; i < 10; ++i) {
            auto sell = pool.allocate(
                order_id++,
                OrderType::Limit,
                OrderSide::Sell,
                TimeInForce::GoodTillCancel,
                100,
                50
            );
            book.addOrder(sell);
        }
        
        state.ResumeTiming();
        
        auto ioc_buy = pool.allocate(
            order_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::ImmediateOrCancel,
            100,
            200  // partial fill, rest cancelled
        );
        engine.onNewOrder(ioc_buy);
        benchmark::DoNotOptimize(ioc_buy);
    }
}
BENCHMARK(BM_IOC_Order);

// Cancel operation
static void BM_Cancel_Order(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        state.PauseTiming();

        auto buy = pool.allocate(
            order_id,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100,
            100
        );
        book.addOrder(buy);
        
        state.ResumeTiming();
        
        engine.onCancelOrder(order_id);
        
        order_id++;
    }
}
BENCHMARK(BM_Cancel_Order);

// OrderGateway validation (checks overhead)
static void BM_Gateway_Validation(benchmark::State& state) {
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    OrderGateway gateway(engine);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        auto order = pool.allocate(
            order_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100,
            100
        );
        benchmark::DoNotOptimize(gateway.submitOrder(order));

        state.PauseTiming();
        book.cancelOrder(order->getOrderId());
        state.ResumeTiming();
    }
}
BENCHMARK(BM_Gateway_Validation);

// ============================================================================
// PARAMETERIZED BENCHMARKS - Test with different sizes
// ============================================================================

// Varying price levels
static void BM_Match_VaryLevels(benchmark::State& state) {
    int num_levels = state.range(0);    // 1, 5, 10, 50
    
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        state.PauseTiming();

        for (int level = 0; level < num_levels; ++level) {
            for (int i = 0; i < 5; ++i) {
                auto sell = pool.allocate(
                    order_id++,
                    OrderType::Limit,
                    OrderSide::Sell,
                    TimeInForce::GoodTillCancel,
                    100 + level,
                    20
                );
                book.addOrder(sell);
            }
        }
        
        state.ResumeTiming();
        
        auto buy = pool.allocate(
            order_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100 + num_levels,
            num_levels * 100
        );
        engine.onNewOrder(buy);
        benchmark::DoNotOptimize(buy);
    }
}
BENCHMARK(BM_Match_VaryLevels)
    ->Arg(1)
    ->Arg(5)
    ->Arg(10)
    ->Arg(50);

// Vary quantity size
static void BM_Match_VaryQuantity(benchmark::State& state) {
    int qty = state.range(0);  // 100, 1000, 10000
    
    OrderBook book;
    TradeHistory history;
    MatchingEngine engine(book, history);
    ObjectPool pool(10);
    
    int order_id = 0;
    
    for (auto _ : state) {
        state.PauseTiming();

        for (int i = 0; i < qty / 10; ++i) {
            auto sell = pool.allocate(
                order_id++,
                OrderType::Limit,
                OrderSide::Sell,
                TimeInForce::GoodTillCancel,
                100,
                10
            );
            book.addOrder(sell);
        }
        
        state.ResumeTiming();
        
        auto buy = pool.allocate(
            order_id++,
            OrderType::Limit,
            OrderSide::Buy,
            TimeInForce::GoodTillCancel,
            100,
            qty
        );
        engine.onNewOrder(buy);
        benchmark::DoNotOptimize(buy);
    }
}
BENCHMARK(BM_Match_VaryQuantity)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000);

BENCHMARK_MAIN();
