#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <functional>
#include <random>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <fstream> 

using OrderId = uint64_t;
using Price = int64_t;
using Volume = int64_t;

enum class Side { Ask, Bid };

struct PriceLevel {
    Price price;
    Volume volume;

    bool operator<(const PriceLevel& other) const { return price < other.price; }
    bool operator>(const PriceLevel& other) const { return price > other.price; }
};

class IOrderBook {
public:
    virtual ~IOrderBook() = default;
    virtual void AddOrder(OrderId orderId, Side side, Price price, Volume volume) = 0;
    virtual void DeleteOrder(OrderId orderId) = 0;
    virtual void ModifyOrder(OrderId orderId, Volume volume) = 0;
    virtual std::pair<Price, Price> GetBestPrices() const = 0;
};

#define EXPECT(condition, message) \
    if(!(condition)){ \
        std::cerr<<"Assertion failed: "<<message<<std::endl; \
        std::exit(EXIT_FAILURE); \
    }

class OrderBookMap : public IOrderBook {
private:
    std::map<Price, Volume, std::greater<Price>> mBidLevels;
    std::map<Price, Volume, std::less<Price>> mAskLevels;

    struct OrderDetails {
        Side side;
        Price price;
        Volume originalVolume;
        std::map<Price, Volume, std::greater<Price>>::iterator bidIt;
        std::map<Price, Volume, std::less<Price>>::iterator askIt;
    };
    std::unordered_map<OrderId, OrderDetails> mOrders;

public:
    void AddOrder(OrderId orderId, Side side, Price price, Volume volume) override {
        if (mOrders.find(orderId) != mOrders.end()) {
            return;
        }

        OrderDetails details;
        details.side = side;
        details.price = price;
        details.originalVolume = volume;

        if (side == Side::Bid) {
            auto [it, inserted] = mBidLevels.try_emplace(price, volume);
            if (!inserted) {
                it->second += volume;
            }
            details.bidIt = it;
        } else {
            auto [it, inserted] = mAskLevels.try_emplace(price, volume);
            if (!inserted) {
                it->second += volume;
            }
            details.askIt = it;
        }
        mOrders.emplace(orderId, details);
    }

    void ModifyOrder(OrderId orderId, Volume newVolume) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) {
            return;
        }

        OrderDetails& details = order_it->second;
        Volume volumeDiff = newVolume - details.originalVolume;

        if (details.side == Side::Bid) {
            details.bidIt->second += volumeDiff;
            if (details.bidIt->second <= 0) {
                mBidLevels.erase(details.bidIt);
                mOrders.erase(order_it);
                return;
            }
        } else {
            details.askIt->second += volumeDiff;
            if (details.askIt->second <= 0) {
                mAskLevels.erase(details.askIt);
                mOrders.erase(order_it);
                return;
            }
        }
        details.originalVolume = newVolume;
    }

    void DeleteOrder(OrderId orderId) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) {
            return;
        }

        OrderDetails& details = order_it->second;
        if (details.side == Side::Bid) {
            details.bidIt->second -= details.originalVolume;
            if (details.bidIt->second <= 0) {
                mBidLevels.erase(details.bidIt);
            }
        } else {
            details.askIt->second -= details.originalVolume;
            if (details.askIt->second <= 0) {
                mAskLevels.erase(details.askIt);
            }
        }
        mOrders.erase(order_it);
    }

    std::pair<Price, Price> GetBestPrices() const override {
        if (mBidLevels.empty() || mAskLevels.empty()) {
            return {0, 0};
        }
        return {mBidLevels.begin()->first, mAskLevels.begin()->first};
    }
};

class OrderBookVector : public IOrderBook {
private:
    std::vector<PriceLevel> mBidLevels;
    std::vector<PriceLevel> mAskLevels;

    struct OrderDetails {
        Side side;
        Price price;
        Volume originalVolume;
    };
    std::unordered_map<OrderId, OrderDetails> mOrders;

    template <class Compare>
    void AddOrderImpl(std::vector<PriceLevel>& levels, OrderId orderId, Side side,
                      Price price, Volume volume, Compare comp) {
        auto it = std::lower_bound(levels.begin(), levels.end(), PriceLevel{price, 0},
                                   [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                       return comp(p1.price, p2.price);
                                   });

        if (it != levels.end() && it->price == price) {
            it->volume += volume;
        } else {
            levels.insert(it, {price, volume});
        }
        mOrders.emplace(orderId, OrderDetails{side, price, volume});
    }

public:
    void AddOrder(OrderId orderId, Side side, Price price, Volume volume) override {
        if (mOrders.find(orderId) != mOrders.end()) return;

        if (side == Side::Bid) {
            AddOrderImpl(mBidLevels, orderId, side, price, volume, std::greater<Price>());
        } else {
            AddOrderImpl(mAskLevels, orderId, side, price, volume, std::less<Price>());
        }
    }

    void ModifyOrder(OrderId orderId, Volume newVolume) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        Volume volumeDiff = newVolume - details.originalVolume;

        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;
        auto comp = (details.side == Side::Bid) ?
                    [](Price p1, Price p2) { return p1 > p2; } :
                    [](Price p1, Price p2) { return p1 < p2; };

        auto it = std::lower_bound(levels.begin(), levels.end(), PriceLevel{details.price, 0},
                                   [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                       return comp(p1.price, p2.price);
                                   });

        if (it != levels.end() && it->price == details.price) {
            it->volume += volumeDiff;
            if (it->volume <= 0) {
                levels.erase(it);
                mOrders.erase(order_it);
                return;
            }
            details.originalVolume = newVolume;
        }
    }

    void DeleteOrder(OrderId orderId) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;
        auto comp = (details.side == Side::Bid) ?
                    [](Price p1, Price p2) { return p1 > p2; } :
                    [](Price p1, Price p2) { return p1 < p2; };

        auto it = std::lower_bound(levels.begin(), levels.end(), PriceLevel{details.price, 0},
                                   [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                       return comp(p1.price, p2.price);
                                   });

        if (it != levels.end() && it->price == details.price) {
            it->volume -= details.originalVolume;
            if (it->volume <= 0) {
                levels.erase(it);
            }
        }
        mOrders.erase(order_it);
    }

    std::pair<Price, Price> GetBestPrices() const override {
        if (mBidLevels.empty() || mAskLevels.empty()) {
            return {0, 0};
        }
        return {mBidLevels.front().price, mAskLevels.front().price};
    }
};

class OrderBookVectorEfficient : public IOrderBook {
private:
    std::vector<PriceLevel> mBidLevels;
    std::vector<PriceLevel> mAskLevels;

    struct OrderDetails {
        Side side;
        Price price;
        Volume originalVolume;
    };
    std::unordered_map<OrderId, OrderDetails> mOrders;

    template <class Compare>
    void AddOrderImpl(std::vector<PriceLevel>& levels, OrderId orderId, Side side,
                      Price price, Volume volume, Compare comp) {
        auto it = std::lower_bound(levels.begin(), levels.end(), PriceLevel{price, 0},
                                   [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                       return comp(p1.price, p2.price);
                                   });

        if (it != levels.end() && it->price == price) {
            it->volume += volume;
        } else {
            levels.insert(it, {price, volume});
        }
        mOrders.emplace(orderId, OrderDetails{side, price, volume});
    }

public:
    void AddOrder(OrderId orderId, Side side, Price price, Volume volume) override {
        if (mOrders.find(orderId) != mOrders.end()) return;

        if (side == Side::Bid) {
            AddOrderImpl(mBidLevels, orderId, side, price, volume, std::less<Price>());
        } else {
            AddOrderImpl(mAskLevels, orderId, side, price, volume, std::greater<Price>());
        }
    }

    void ModifyOrder(OrderId orderId, Volume newVolume) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        Volume volumeDiff = newVolume - details.originalVolume;

        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;
        auto comp = (details.side == Side::Bid) ?
                    [](Price p1, Price p2) { return p1 < p2; } :
                    [](Price p1, Price p2) { return p1 > p2; };

        auto it = std::lower_bound(levels.begin(), levels.end(), PriceLevel{details.price, 0},
                                   [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                       return comp(p1.price, p2.price);
                                   });

        if (it != levels.end() && it->price == details.price) {
            it->volume += volumeDiff;
            if (it->volume <= 0) {
                levels.erase(it);
                mOrders.erase(order_it);
                return;
            }
            details.originalVolume = newVolume;
        }
    }

    void DeleteOrder(OrderId orderId) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;
        auto comp = (details.side == Side::Bid) ?
                    [](Price p1, Price p2) { return p1 < p2; } :
                    [](Price p1, Price p2) { return p1 > p2; };

        auto it = std::lower_bound(levels.begin(), levels.end(), PriceLevel{details.price, 0},
                                   [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                       return comp(p1.price, p2.price);
                                   });

        if (it != levels.end() && it->price == details.price) {
            it->volume -= details.originalVolume;
            if (it->volume <= 0) {
                levels.erase(it);
            }
        }
        mOrders.erase(order_it);
    }

    std::pair<Price, Price> GetBestPrices() const override {
        if (mBidLevels.empty() || mAskLevels.empty()) {
            return {0, 0};
        }
        return {mBidLevels.back().price, mAskLevels.back().price};
    }
};

template <class ForwardIt, class T, class Compare>
ForwardIt branchless_lower_bound(ForwardIt first, ForwardIt last, const T& value, Compare comp) {
    auto length = std::distance(first, last);
    while (length > 0) {
        auto half = length / 2;
        ForwardIt mid = first + half;
        first += comp(*mid, value) * (length - half);
        length = half;
    }
    return first;
}

class OrderBookVectorBranchless : public IOrderBook {
private:
    std::vector<PriceLevel> mBidLevels;
    std::vector<PriceLevel> mAskLevels;

    struct OrderDetails {
        Side side;
        Price price;
        Volume originalVolume;
    };
    std::unordered_map<OrderId, OrderDetails> mOrders;

    template <class Compare>
    void AddOrderImpl(std::vector<PriceLevel>& levels, OrderId orderId, Side side,
                      Price price, Volume volume, Compare comp) {
        auto it = branchless_lower_bound(levels.begin(), levels.end(), PriceLevel{price, 0},
                                         [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                             return comp(p1.price, p2.price);
                                         });

        if (it != levels.end() && it->price == price) {
            it->volume += volume;
        } else {
            levels.insert(it, {price, volume});
        }
        mOrders.emplace(orderId, OrderDetails{side, price, volume});
    }

public:
    void AddOrder(OrderId orderId, Side side, Price price, Volume volume) override {
        if (mOrders.find(orderId) != mOrders.end()) return;

        if (side == Side::Bid) {
            AddOrderImpl(mBidLevels, orderId, side, price, volume, std::less<Price>());
        } else {
            AddOrderImpl(mAskLevels, orderId, side, price, volume, std::greater<Price>());
        }
    }

    void ModifyOrder(OrderId orderId, Volume newVolume) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        Volume volumeDiff = newVolume - details.originalVolume;

        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;
        auto comp = (details.side == Side::Bid) ?
                    [](Price p1, Price p2) { return p1 < p2; } :
                    [](Price p1, Price p2) { return p1 > p2; };

        auto it = branchless_lower_bound(levels.begin(), levels.end(), PriceLevel{details.price, 0},
                                         [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                             return comp(p1.price, p2.price);
                                         });

        if (it != levels.end() && it->price == details.price) {
            it->volume += volumeDiff;
            if (it->volume <= 0) {
                levels.erase(it);
                mOrders.erase(order_it);
                return;
            }
            details.originalVolume = newVolume;
        }
    }

    void DeleteOrder(OrderId orderId) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;
        auto comp = (details.side == Side::Bid) ?
                    [](Price p1, Price p2) { return p1 < p2; } :
                    [](Price p1, Price p2) { return p1 > p2; };

        auto it = branchless_lower_bound(levels.begin(), levels.end(), PriceLevel{details.price, 0},
                                         [&comp](const PriceLevel& p1, const PriceLevel& p2) {
                                             return comp(p1.price, p2.price);
                                         });

        if (it != levels.end() && it->price == details.price) {
            it->volume -= details.originalVolume;
            if (it->volume <= 0) {
                levels.erase(it);
            }
        }
        mOrders.erase(order_it);
    }

    std::pair<Price, Price> GetBestPrices() const override {
        if (mBidLevels.empty() || mAskLevels.empty()) {
            return {0, 0};
        }
        return {mBidLevels.back().price, mAskLevels.back().price};
    }
};

class OrderBookVectorLinear : public IOrderBook {
private:
    std::vector<PriceLevel> mBidLevels;
    std::vector<PriceLevel> mAskLevels;

    struct OrderDetails {
        Side side;
        Price price;
        Volume originalVolume;
    };
    std::unordered_map<OrderId, OrderDetails> mOrders;

    template <class Compare>
    void AddOrderImpl(std::vector<PriceLevel>& levels, OrderId orderId, Side side,
                      Price price, Volume volume, Compare comp) {
        auto it = std::find_if(levels.begin(), levels.end(),
                               [&](const PriceLevel& pl) {
                                   return !comp(pl.price, price);
                               });

        if (it != levels.end() && it->price == price) [[likely]] {
            it->volume += volume;
        } else {
            levels.insert(it, {price, volume});
        }
        mOrders.emplace(orderId, OrderDetails{side, price, volume});
    }

public:
    void AddOrder(OrderId orderId, Side side, Price price, Volume volume) override {
        if (mOrders.find(orderId) != mOrders.end()) return;

        if (side == Side::Bid) {
            AddOrderImpl(mBidLevels, orderId, side, price, volume, std::less<Price>());
        } else {
            AddOrderImpl(mAskLevels, orderId, side, price, volume, std::greater<Price>());
        }
    }

    void ModifyOrder(OrderId orderId, Volume newVolume) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        Volume volumeDiff = newVolume - details.originalVolume;

        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;

        auto it = std::find_if(levels.begin(), levels.end(),
                               [&](const PriceLevel& pl) {
                                   return pl.price == details.price;
                               });

        if (it != levels.end()) {
            it->volume += volumeDiff;
            if (it->volume <= 0) {
                levels.erase(it);
                mOrders.erase(order_it);
                return;
            }
            details.originalVolume = newVolume;
        }
    }

    void DeleteOrder(OrderId orderId) override {
        auto order_it = mOrders.find(orderId);
        if (order_it == mOrders.end()) return;

        OrderDetails& details = order_it->second;
        auto& levels = (details.side == Side::Bid) ? mBidLevels : mAskLevels;

        auto it = std::find_if(levels.begin(), levels.end(),
                               [&](const PriceLevel& pl) {
                                   return pl.price == details.price;
                               });

        if (it != levels.end()) {
            it->volume -= details.originalVolume;
            if (it->volume <= 0) {
                levels.erase(it);
            }
        }
        mOrders.erase(order_it);
    }

    std::pair<Price, Price> GetBestPrices() const override {
        if (mBidLevels.empty() || mAskLevels.empty()) {
            return {0, 0};
        }
        return {mBidLevels.back().price, mAskLevels.back().price};
    }
};

struct LatencyMeasurement {
    std::vector<long long> durations_ns;

    void record(long long duration_ns) {
        durations_ns.push_back(duration_ns);
    }
    

    void save_to_csv(const std::string& filename) const {
        std::ofstream file(filename);
        for (const auto& duration : durations_ns) {
            file << duration << "\n";
        }
    }

    void print_stats(const std::string& title) const {
        if (durations_ns.empty()) {
            std::cout << title << ": No data collected." << std::endl;
            return;
        }

        auto sorted = durations_ns;
        std::sort(sorted.begin(), sorted.end());
        double avg = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();

        double median;
        size_t size = sorted.size();
        if (size % 2 == 0) {
            median = (sorted[size / 2 - 1] + sorted[size / 2]) / 2.0;
        } else {
            median = sorted[size / 2];
        }

        std::cout << "=== " << title << " ===" << std::endl;
        std::cout << "Operations: " << sorted.size() << std::endl;
        std::cout << "Min: " << sorted.front() << " ns" << std::endl;
        std::cout << "Max: " << sorted.back() << " ns" << std::endl;
        std::cout << "Avg: " << avg << " ns" << std::endl;
        std::cout << "Median: " << median << " ns" << std::endl; 
        std::cout << "P50: " << sorted[static_cast<size_t>(size * 0.5)] << " ns" << std::endl;
        std::cout << "P90: " << sorted[static_cast<size_t>(size * 0.9)] << " ns" << std::endl;
        std::cout << "P99: " << sorted[static_cast<size_t>(size * 0.99)] << " ns" << std::endl;
        std::cout << std::endl;
    }
};

struct ScopedTimer {
    LatencyMeasurement& measurement;
    std::chrono::high_resolution_clock::time_point start;

    ScopedTimer(LatencyMeasurement& m) : measurement(m),
        start(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        measurement.record(duration_ns);
    }
};

void simulateMarket(IOrderBook& orderBook, LatencyMeasurement& latency, size_t updates) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> op_dist(0, 2);
    std::uniform_int_distribution<int> price_dist(10, 200);
    std::uniform_int_distribution<int> volume_dist(10, 100);

    std::vector<OrderId> activeOrders;
    uint64_t next_id = 1;

    for (size_t i = 0; i < updates; ++i) {
        ScopedTimer timer(latency);

        int op = op_dist(gen);
        Side side = (op_dist(gen) % 2 == 0) ? Side::Bid : Side::Ask;

        try {
            if (op == 0 || activeOrders.empty()) {
                OrderId id = next_id++;
                Price price = price_dist(gen);
                Volume volume = volume_dist(gen);
                orderBook.AddOrder(id, side, price, volume);
                activeOrders.push_back(id);
            } else if (op == 1) {
                size_t idx = std::uniform_int_distribution<size_t>(0, activeOrders.size()-1)(gen);
                Volume volume = volume_dist(gen);
                orderBook.ModifyOrder(activeOrders[idx], volume);
            } else {
                size_t idx = std::uniform_int_distribution<size_t>(0, activeOrders.size()-1)(gen);
                orderBook.DeleteOrder(activeOrders[idx]);
                activeOrders.erase(activeOrders.begin() + idx);
            }
        } catch (...) {
            continue;
        }
    }
}

void simulateMarketRandomizedHeap(IOrderBook& orderBook, LatencyMeasurement& latency, size_t updates) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> op_dist(0, 2);
    std::uniform_int_distribution<int> price_dist(10, 200);
    std::uniform_int_distribution<int> volume_dist(10, 100);

    std::vector<OrderId> activeOrders;
    uint64_t next_id = 1;

    for (size_t i = 0; i < updates; ++i) {
        std::vector<char> dummy_alloc(1024 + (i % 512));

        ScopedTimer timer(latency);

        int op = op_dist(gen);
        Side side = (op_dist(gen) % 2 == 0) ? Side::Bid : Side::Ask;

        try {
            if (op == 0 || activeOrders.empty()) {
                OrderId id = next_id++;
                Price price = price_dist(gen);
                Volume volume = volume_dist(gen);
                orderBook.AddOrder(id, side, price, volume);
                activeOrders.push_back(id);
            } else if (op == 1) {
                size_t idx = std::uniform_int_distribution<size_t>(0, activeOrders.size()-1)(gen);
                Volume volume = volume_dist(gen);
                orderBook.ModifyOrder(activeOrders[idx], volume);
            } else {
                size_t idx = std::uniform_int_distribution<size_t>(0, activeOrders.size()-1)(gen);
                orderBook.DeleteOrder(activeOrders[idx]);
                activeOrders.erase(activeOrders.begin() + idx);
            }
        } catch (...) {
            continue;
        }
    }
}

int main() {
    const size_t NUM_OPERATIONS = 1e5;

    std::cout << "Order Book Performance Comparison" << std::endl;
    std::cout << "==================================" << std::endl << std::endl;

    {
        OrderBookMap ob;
        LatencyMeasurement latency;
        simulateMarket(ob, latency, NUM_OPERATIONS);
        latency.print_stats("std::map Implementation");
        latency.save_to_csv("map_latencies.csv");
    }

    {
        OrderBookMap ob;
        LatencyMeasurement latency;
        simulateMarketRandomizedHeap(ob, latency, NUM_OPERATIONS);
        latency.print_stats("std::map Randomized Heap");
        latency.save_to_csv("map_random_latencies.csv");
    }

    {
        OrderBookVector ob;
        LatencyMeasurement latency;
        simulateMarket(ob, latency, NUM_OPERATIONS);
        latency.print_stats("std::vector Intuitive Order");
        latency.save_to_csv("vector_intuitive_latencies.csv");
    }

    {
        OrderBookVectorEfficient ob;
        LatencyMeasurement latency;
        simulateMarket(ob, latency, NUM_OPERATIONS);
        latency.print_stats("std::vector Efficient Order");
        latency.save_to_csv("vector_efficient_latencies.csv");
    }
    
    {
        OrderBookVectorBranchless ob;
        LatencyMeasurement latency;
        simulateMarket(ob, latency, NUM_OPERATIONS);
        latency.print_stats("Branchless Binary Search");
        latency.save_to_csv("branchless_latencies.csv");
    }

    {
        OrderBookVectorLinear ob;
        LatencyMeasurement latency;
        simulateMarket(ob, latency, NUM_OPERATIONS);
        latency.print_stats("Linear Search (Winner!)");
        latency.save_to_csv("linear_search_latencies.csv");
    }

    return 0;
}