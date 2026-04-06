import numpy as np
import pandas as pd


def update_data_file(input_file, output_file, samples_per_mult=49):
    # Load the existing data
    df = pd.read_csv(input_file, sep="\s+", names=["mult", "val1", "val2"])

    # Store reference samples (where first number == 0)
    ref_samples = df[df["mult"] == 0].copy()

    # Find existing multiples to determine where to start
    existing_mults = df["mult"].unique()
    max_mult = max(existing_mults)

    # Starting point requested is 600
    start_mult = 600

    # Generate new data
    new_data = []

    # Example logic: add 3 blocks of 300 (600, 900, 1200)
    # Adjust range as needed for your specific target
    for m in range(start_mult, 1500, 300):
        for _ in range(samples_per_mult):
            # Pick a random reference sample to base the +/- 10 logic on
            base = ref_samples.sample(1).iloc[0]

            # Apply +/- 10 fluctuation
            v1 = base["val1"] + np.random.uniform(-10, 10)
            v2 = base["val2"] + np.random.uniform(-10, 10)

            new_data.append({"mult": m, "val1": v1, "val2": v2})

    new_df = pd.DataFrame(new_data)

    # Concatenate original (0, 300 and existing) with new data
    final_df = pd.concat([df, new_df], ignore_index=True)

    # Save to file
    final_df.to_csv(output_file, sep=" ", index=False, header=False)
    print(f"File updated. Saved to {output_file}")


if __name__ == "__main__":
    update_data_file("all_measurements.txt", "updated_data.txt")
