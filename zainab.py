#In This code we have 3 nodes , and I have commented the part that receives the fire node from the arduino and send the path to the arduino

import cv2
import matplotlib.pyplot as plt
import numpy as np
from heapq import heappop, heappush
# import serial  # Uncomment for Arduino communication
# import time  # Uncomment for Arduino communication

# Load the map image
map_image = cv2.imread("map.png")
map_image = cv2.cvtColor(map_image, cv2.COLOR_BGR2RGB)

# Grid dimensions (based on the image size)
GRID_HEIGHT, GRID_WIDTH, _ = map_image.shape
grid = np.zeros((GRID_HEIGHT, GRID_WIDTH))  # 0: obstacle, 1: walkable

# Define nodes with IDs and positions
nodes = {
    1: (689, 123),  # Example position for Node 1
    2: (505, 383),  # Example position for Node 2
    3: (273, 622)   # Example position for Node 3
}

# Safe points (e.g., exits)
safe_points = [(632, 54), (684, 505), (191, 552)]

# Function to mark walkable areas
def mark_rectangle(grid, x1, y1, x2, y2, value=1):
    x1, x2 = min(x1, x2), max(x1, x2)
    y1, y2 = min(y1, y2), max(y1, y2)
    grid[y1:y2 + 1, x1:x2 + 1] = value

# Mark walkable paths
mark_rectangle(grid, 446, 309, 592, 320, value=1)
mark_rectangle(grid, 446, 192, 592, 204, value=1)
mark_rectangle(grid, 446, 430, 592, 440, value=1)
mark_rectangle(grid, 633, 179, 750, 187, value=1)
mark_rectangle(grid, 592, 31, 634, 460, value=1)
mark_rectangle(grid, 464, 458, 690, 611, value=1)
mark_rectangle(grid, 185, 550, 464, 576, value=1)

# A* algorithm
def heuristic(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])

def a_star_search(grid, start, goals):
    rows, cols = grid.shape

    open_set = []
    heappush(open_set, (0, start))
    came_from = {}
    g_score = {start: 0}
    f_score = {start: min(heuristic(start, goal) for goal in goals)}

    while open_set:
        _, current = heappop(open_set)

        if current in goals:
            path = []
            while current in came_from:
                path.append(current)
                current = came_from[current]
            path.append(start)
            path.reverse()
            return path

        for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            neighbor = (current[0] + dx, current[1] + dy)

            if (0 <= neighbor[0] < cols and
                    0 <= neighbor[1] < rows and
                    grid[neighbor[1], neighbor[0]] == 1):

                tentative_g_score = g_score[current] + 1

                if (neighbor not in g_score or
                        tentative_g_score < g_score[neighbor]):
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g_score
                    f_score[neighbor] = tentative_g_score + min(heuristic(neighbor, goal) for goal in goals)
                    heappush(open_set, (f_score[neighbor], neighbor))

    return None

# Simulate receiving fire node information from Arduino
fire_node_id = None
"""
# Uncomment to receive from Arduino
try:
    arduino = serial.Serial(port='COM3', baudrate=9600, timeout=1)  # Replace 'COM3' with your port
    fire_node_id = int(arduino.readline().decode().strip())  # Receive fire node ID
    print(f"Fire detected at Node {fire_node_id}")
    arduino.close()
except Exception as e:
    print("Error receiving fire node ID from Arduino:", e)
"""

# For testing, assume fire detected at Node 1
fire_node_id = 1  # Example for simulation
fire_position = nodes[fire_node_id]
print(f"Fire detected at Node {fire_node_id} at position {fire_position}")

# Mark fire node as non-walkable
x, y = fire_position
grid[max(0, y - 1):min(GRID_HEIGHT, y + 2), max(0, x - 1):min(GRID_WIDTH, x + 2)] = 0

# Visualize grid after marking fire
print("Grid updated with fire node marked as obstacle.")

# Let the user select the start point
plt.imshow(map_image)
plt.title("Select the Start Point")
plt.axis("off")
start_point = None

def onclick_start(event):
    global start_point
    start_point = (int(event.xdata), int(event.ydata))
    plt.close()

cid = plt.gcf().canvas.mpl_connect('button_press_event', onclick_start)
plt.show()

if start_point is None:
    print("No start point selected. Exiting.")
    exit()

# Ensure start point is valid
if grid[start_point[1], start_point[0]] == 0:
    print("Start point is not walkable. Exiting.")
    exit()

# Find the shortest path
path = a_star_search(grid, start_point, safe_points)

# Arduino communication (commented out)
if path:
    print("Path found:", path)
    """
    try:
        arduino = serial.Serial(port='COM3', baudrate=9600, timeout=1)
        time.sleep(2)  # Wait for connection
        for x, y in path:
            arduino.write(f"{x},{y}\n".encode())  # Send path coordinates
            time.sleep(0.1)  # Delay for smooth communication
        print("Path sent to Arduino.")
        arduino.close()
    except Exception as e:
        print("Error communicating with Arduino:", e)
    """
else:
    print("No safe path found!")

# Visualization
plt.figure(figsize=(12, 10))
plt.imshow(map_image, alpha=0.7)
plt.title("Pathfinding Visualization")

# Draw nodes
for node_id, position in nodes.items():
    plt.scatter(position[0], position[1], color="orange" if node_id == fire_node_id else "green", s=200, label=f"Node {node_id}")

# Draw safe points
for sp in safe_points:
    plt.scatter(sp[0], sp[1], color="blue", marker='*', s=200, label="Safe Point")

# Draw start point
plt.scatter(start_point[0], start_point[1], color="yellow", s=100, label="Start Point")

# Draw path
if path:
    path_x, path_y = zip(*path)
    plt.plot(path_x, path_y, color="red", linewidth=2, label="Shortest Path")

plt.legend()
plt.show()
