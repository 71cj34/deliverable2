import os
import sys

import numpy as np
import open3d as o3d


def visualize_xyz(file_path):
    """
    Reads an .xyz file and displays it using Open3D.
    """
    if not os.path.exists(file_path):
        print(f"Error: The file '{file_path}' was not found.")
        return

    print(f"Loading {file_path}...")
    pcd = o3d.io.read_point_cloud(file_path, format="xyz")

    if not pcd.has_points():
        print(
            "Error: No points found in file. Please ensure the format is 'X Y Z' space-separated."
        )
        return

    print(f"Loaded {len(pcd.points)} points.")
    print("Preview of data (first 5 points):")
    print(np.asarray(pcd.points)[:5])

    print("Opening visualization window...")
    o3d.visualization.draw_geometries([pcd])


if __name__ == "__main__":
    filename = "all_measurements copy.xyz"

    if len(sys.argv) > 1:
        filename = sys.argv[1]

    visualize_xyz(filename)
