import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def generate_latency_grid_plot():
    """
    Reads latency data from various CSV files and generates a grid of
    histograms, adapting plot ranges to the data.
    """
    # All 6 benchmark results to plot.
    # (CSV filename, Title for the plot, Color)
    files_to_plot = [
        ("map_latencies.csv", "std::map", "#1f77b4"),
        ("map_random_latencies.csv", "std::map (Randomized)", "#ff7f0e"),
        ("vector_intuitive_latencies.csv", "std::vector (Intuitive)", "#2ca02c"),
        ("vector_efficient_latencies.csv", "std::vector (Efficient)", "#d62728"),
        ("branchless_latencies.csv", "Branchless Binary Search", "#9467bd"),
        ("linear_search_latencies.csv", "Linear Search (Winner!)", "#8c564b")
    ]

    # --- Plotting Setup ---
    # Create a figure and a 3x2 grid of subplots.
    fig, axes = plt.subplots(3, 2, figsize=(15, 12))
    fig.suptitle('Orderbook Latency Distribution Comparison', fontsize=20, y=0.98)
    axes = axes.flatten()

    # --- Loop Through Data and Plot on Each Subplot ---
    print("Generating subplot grid...")
    for ax, (filename, title, color) in zip(axes, files_to_plot):
        try:
            # Read the latency data from the CSV file.
            df = pd.read_csv(filename, header=None, names=['latency'])
            latencies = df['latency']
            
            # Dynamic plot parameters
            bins = 'auto'
            upper_limit = latencies.quantile(0.999) * 1.1 

            # Histogram
            counts, bin_edges, patches = ax.hist(
                latencies, bins=bins, color=color, density=True, alpha=0.6
            )

            # Compute bin centers
            bin_centers = 0.5 * (bin_edges[1:] + bin_edges[:-1])

            # Overlay line plot
            ax.plot(bin_centers, counts, color='black', linewidth=2, marker='o')

            # Median line
            median_val = latencies.median()
            ax.axvline(median_val, color='red', linestyle='--', linewidth=2, 
                    label=f'Median: {median_val:.1f} ns')

            # Customize each subplot
            ax.set_title(title, fontsize=14)
            ax.set_xlabel('Latency (ns)')
            ax.set_ylabel('Frequency (Density)')
            ax.legend()
            ax.set_xlim(0, upper_limit)

        except FileNotFoundError:
            print(f"Warning: Data file '{filename}' not found. Skipping.")
            ax.set_title(f"{title}\n(Data file not found)", fontsize=12)
            ax.set_yticklabels([])


    # --- Final Layout Adjustment and Saving ---
    # Adjust layout to prevent labels from overlapping.
    plt.tight_layout(rect=[0, 0, 1, 0.95])

    # Save the final grid plot.
    output_filename = 'latency_distribution_grid_corrected.png'
    plt.savefig(output_filename, dpi=300)
    
    print(f"\nGrid plot successfully saved as '{output_filename}'")

if __name__ == '__main__':
    generate_latency_grid_plot()