import serial
import open3d as o3d
import numpy as np
import math
import time

SERIAL_PORT = 'COM4'  # CHANGE TO SPECIFIC COM PORT
BAUD_RATE = 115200    # MAKE SURE THIS MATCHES VALUE IN UART_INIT
X_MULT = 20

MEASUREMENT_TIMEOUT = 3.0  # Seconds to wait for next measurement before considering slice complete

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

    Assumes measurements are evenly spaced around 360 degrees.
    """
    num_points = len(distances)
    if num_points == 0:
        return

    degrees_per_point = 360.0 / num_points

    with open(filename, "a") as f:
        for i, dist in enumerate(distances):
            angle_deg = i * degrees_per_point
            angle_rad = math.radians(angle_deg)
            y = dist * math.cos(angle_rad)
            z = dist * math.sin(angle_rad)
            f.write(f'{x_offset * X_MULT} {y} {z}\n')

def read_and_graph_serial_data():
    try:
        # Open serial connection
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)  # Short timeout for polling
        print(f"Connected to {SERIAL_PORT} at {BAUD_RATE} baud.")

        all_measurements = []
        sensor_init_successful = False
        current_slice = 0  # X-axis offset (manual displacement)
        last_measurement_time = time.time()

        print("Waiting for sensor initialization...")

        while True:
            # Check if we've timed out waiting for the next measurement
            if sensor_init_successful and all_measurements and (time.time() - last_measurement_time) > MEASUREMENT_TIMEOUT:
                print(f"Slice {current_slice} complete (timeout after {MEASUREMENT_TIMEOUT}s). Saving {len(all_measurements)} points...")

                # Save this slice's data to file
                graph_slice(all_measurements, current_slice)

                # Reset for next slice
                all_measurements = []
                current_slice += 1
                sensor_init_successful = False  # Wait for next "SensorInit Successful"
                print(f"Ready for slice {current_slice}. Waiting for sensor initialization...")

            # Try to read from serial
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8').rstrip()
                except UnicodeDecodeError:
                    continue

                if not line:
                    continue

                print(line)

                if "SensorInit Successful" in line:
                    sensor_init_successful = True
                    all_measurements = []  # Clear any previous measurements
                    last_measurement_time = time.time()
                    print(f"Sensor initialization successful. Starting slice {current_slice}...")
                    continue

                if sensor_init_successful:
                    try:
                        # Attempt to convert the line to a float
                        distance = float(line)
                        all_measurements.append(distance)
                        last_measurement_time = time.time()
                        print(f"Slice {current_slice}: Captured measurement #{len(all_measurements)}: {distance}mm")

                    except ValueError:
                        # Not a float, so it's not a distance measurement
                        pass

            # Small sleep to prevent CPU spinning
            time.sleep(0.001)

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
            else:
                print("No points to visualize.")
        except Exception as e:
            print(f"Could not visualize data: {e}")

if __name__ == "__main__":
    # Clear the output file at start
    open("all_measurements.xyz", "w").close()
    read_and_graph_serial_data()
