import serial
import open3d as o3d
import numpy as np
import math

# Configure your serial port here
SERIAL_PORT = 'COM4'  # Change to your specific COM port
BAUD_RATE = 115200    # Ensure this matches your UART_Init() in C
X_MULT = 20

# Configuration for the scan
DEGREES_PER_STEP = 11.25  # Adjust based on your stepper motor configuration
STEPS_PER_360 = int(360 / DEGREES_PER_STEP)  # Number of measurements per full rotation

def graph_slice(distances, x_offset, filename="all_measurements.xyz"):
    """
    Convert polar coordinates (distance, angle) to Cartesian (x, y, z)
    and append to the output file.

    According to the documentation:
    - X = manual displacement along the depth axis
    - Y-Z plane = the rotating slice (from polar coordinates)

    Formulas:
    y = distance * cos(angle)
    z = distance * sin(angle)
    """
    with open(filename, "a") as f:
        for i, dist in enumerate(distances):
            angle_deg = i * DEGREES_PER_STEP
            angle_rad = math.radians(angle_deg)
            y = dist * math.cos(angle_rad)
            z = dist * math.sin(angle_rad)
            f.write(f'{x_offset * X_MULT} {y} {z}\n')

def read_and_graph_serial_data():
    try:
        # Open serial connection
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to {SERIAL_PORT} at {BAUD_RATE} baud.")

        all_measurements = []
        sensor_init_successful = False
        current_slice = 0  # X-axis offset (manual displacement)

        print("Waiting for sensor initialization...")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').rstrip()
                if not line:
                    continue

                print(line)

                if "SensorInit Successful" in line:
                    sensor_init_successful = True
                    print(f"Sensor initialization successful. Starting slice {current_slice}...")
                    continue

                if sensor_init_successful:
                    try:
                        # Attempt to convert the line to a float
                        distance = float(line)
                        all_measurements.append(distance)
                        print(f"Slice {current_slice}: Captured measurement #{len(all_measurements)}/{STEPS_PER_360}: {distance}mm")

                        # Check if we've completed a full 360° rotation
                        if len(all_measurements) >= STEPS_PER_360:
                            print(f"Slice {current_slice} complete. Saving {len(all_measurements)} points...")

                            # Save this slice's data to file
                            graph_slice(all_measurements, current_slice)

                            # Reset for next slice
                            all_measurements = []
                            current_slice += 1
                            print(f"Ready for slice {current_slice}. Move to next X position and press Enter to continue...")

                            # Optional: wait for user input before next slice
                            # input()  # Uncomment if you want manual confirmation between slices

                    except ValueError:
                        # Not a float, so it's not a distance measurement
                        pass

    except serial.SerialException as e:
        print(f"Error: {e}")
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

        # Visualize all accumulated data after program ends
        try:
            print("\nReading accumulated point cloud data...")
            pcd = o3d.io.read_point_cloud("all_measurements.xyz", format="xyz")

            if len(pcd.points) > 0:
                print(f"Loaded {len(pcd.points)} points. Visualizing...")
                o3d.visualization.draw_geometries([pcd])

                # Create lineset to connect the points
                points = np.asarray(pcd.points)
                num_points = len(points)
                num_slices = num_points // STEPS_PER_360

                lines = []

                # Connect points within each slice (circular pattern)
                for slice_idx in range(num_slices):
                    base_idx = slice_idx * STEPS_PER_360
                    for i in range(STEPS_PER_360):
                        # Connect to next point in same slice
                        next_i = (i + 1) % STEPS_PER_360
                        lines.append([base_idx + i, base_idx + next_i])

                # Connect points between slices (same angle, adjacent X)
                for slice_idx in range(num_slices - 1):
                    base_idx = slice_idx * STEPS_PER_360
                    next_base_idx = (slice_idx + 1) * STEPS_PER_360
                    for i in range(STEPS_PER_360):
                        lines.append([base_idx + i, next_base_idx + i])

                # Create LineSet
                line_set = o3d.geometry.LineSet(
                    points=o3d.utility.Vector3dVector(points),
                    lines=o3d.utility.Vector2iVector(lines)
                )

                # Visualize both points and lines
                o3d.visualization.draw_geometries([pcd, line_set])
            else:
                print("No points to visualize.")
        except Exception as e:
            print(f"Could not visualize data: {e}")

if __name__ == "__main__":
    # Clear the output file at start
    open("all_measurements.xyz", "w").close()
    read_and_graph_serial_data()
