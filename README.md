# Order Book Performance Comparison

A low-latency order book implementation benchmark comparing different data structures and algorithms for high-frequency trading systems - basically implementing - https://youtu.be/sX2nF1fW7kI?si=TUYW4_ZaCP5t6sZ8

### Order Book Implementations

1. **std::map Based** - Traditional approach using map which under the hood is a balanced binary search trees
2. **std::vector Intuitive** - Vector-based implementation with standard binary search
3. **std::vector Efficient** - Optimized vector ordering when reversed apparently for better cache performance
4. **Branchless Binary Search** - Custom implementation to reduce branch mispredictions, well this was unexpected but this is something because of pipelining hazard in branch binary search
5. **Linear Search** - Simple linear scan (surprisingly the fastest for under 5000 operations or so)

### Key Features

- **Complete Order Book Interface**: Add, modify, and delete orders with bid/ask price levels
- **Realistic Market Simulation**: Random order operations mimicking real trading patterns
- **Statistical Analysis**: Min, max, average, median, and percentile latency reporting
- **Data Visualization**: Python script generating distribution plots for performance comparison

### Performance Testing

- Tests with 100,000 operations per implementation
- Takes way too long time with 10^7 operations for python file to make the graph
- Measures actual operation latencies in nanoseconds, well on the youtube video that came to be around 30ns, mine was 10times at 300ns ;)
- Includes memory randomization to simulate real-world cache conditions
- Exports results to CSV for detailed analysis

## Key Findings

The linear search implementation emerged as the winner for under 5000 operations, demonstrating that for typical order book sizes, simple algorithms can outperform complex data structures due to:
- Better cache locality
- Reduced memory overhead
- Fewer branch mispredictions
- Simpler CPU instructions

## Files

- **Main C++ Implementation**: Complete order book benchmark suite
- **Python Visualization**: Latency distribution plotting and analysis
- **CSV Outputs**: Raw performance data for each implementation

This project showcases practical low-latency programming techniques and demonstrates that algorithmic complexity doesn't always translate to real-world performance improvements.
