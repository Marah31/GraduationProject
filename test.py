#In This code you can select the fire points and the start point and the code will find the shortest path to the safe points
#It needs updating the path finding algorithm to include the fire buffer in the validation

import cv2
import matplotlib.pyplot as plt
import numpy as np
from heapq import heappop, heappush

# Load the map image
map_image = cv2.imread("map.png")
map_image = cv2.cvtColor(map_image, cv2.COLOR_BGR2RGB)

# Grid dimensions (based on the image size)
GRID_HEIGHT, GRID_WIDTH, _ = map_image.shape
grid = np.zeros((GRID_HEIGHT, GRID_WIDTH))  # 0: obstacle, 1: walkable

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
mark_rectangle(grid, 633, 313, 750, 316, value=1)
mark_rectangle(grid, 633, 338, 750, 340, value=1)
mark_rectangle(grid, 474, 55, 605, 75, value=1)
mark_rectangle(grid, 592, 31, 634, 460, value=1)
mark_rectangle(grid, 464, 458, 690, 611, value=1)
mark_rectangle(grid, 185, 550, 464, 576, value=1)

# Safe points
safe_points = [(632, 54), (684, 505), (191, 552)]

# Heuristic function for A* (Manhattan distance)
def heuristic(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])

# A* search algorithm
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

# Let the user choose fire points
plt.imshow(map_image)
plt.title("Select Fire Points (Click on each fire area)")
plt.axis("off")
fire_points = []

def onclick_fire(event):
    fire_points.append((int(event.xdata), int(event.ydata)))

cid = plt.gcf().canvas.mpl_connect('button_press_event', onclick_fire)
plt.show()

# Print chosen fire points
print("Chosen Fire Points (x, y):", fire_points)

# Update fire handling to include the entire buffer in validation
expanded_fire_points = []  # Store all fire buffer cells

# Mark fire points as non-walkable and expand the fire_points list
for fire_point in fire_points:
    x, y = fire_point
    # Mark 3x3 area as obstacles
    grid[
        max(0, y - 1) : min(GRID_HEIGHT, y + 2),
        max(0, x - 1) : min(GRID_WIDTH, x + 2),
    ] = 0
    # Add all cells in the 3x3 buffer to expanded_fire_points
    for dx in [-1, 0, 1]:
        for dy in [-1, 0, 1]:
            nx = x + dx
            ny = y + dy
            if 0 <= nx < GRID_WIDTH and 0 <= ny < GRID_HEIGHT:
                expanded_fire_points.append((nx, ny))

# Debug: Print the grid to verify fire points
print("Updated Grid with Fire Points Marked as Obstacles:")
print(grid)

# Let the user choose the start point
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

# Ensure start point is valid or adjust
def find_nearest_walkable(grid, point):
    x, y = point
    if grid[y, x] == 1:
        return point

    rows, cols = grid.shape
    directions = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    visited = set()
    queue = [(x, y)]

    while queue:
        current_x, current_y = queue.pop(0)
        for dx, dy in directions:
            nx, ny = current_x + dx, current_y + dy
            if (0 <= nx < cols and 0 <= ny < rows and (nx, ny) not in visited):
                visited.add((nx, ny))
                if grid[ny, nx] == 1:
                    return nx, ny
                queue.append((nx, ny))
    return None

nearest_start = find_nearest_walkable(grid, start_point)
if nearest_start is None:
    print("No walkable area found near the selected start point. Exiting.")
    exit()

print(f"Selected start point: {start_point}")
print(f"Nearest walkable start point: {nearest_start}")

# Calculate the shortest path
path = a_star_search(grid, nearest_start, safe_points)

# Check if the path passes through any fire buffer cells
if path and any((x, y) in expanded_fire_points for x, y in path):
    print("Path passes through fire buffer. Invalidating path.")
    path = None

# Commented Arduino communication part
if path:
    print("Path found:", path)
    # Uncomment this section to send the path to Arduino
    """
    try:
        # Initialize serial communication with Arduino
        arduino = serial.Serial(port='COM3', baudrate=9600, timeout=1)  # Replace 'COM3' with your port
        time.sleep(2)  # Wait for connection to establish

        for (x, y) in path:
            data = f"{x},{y}\n"
            arduino.write(data.encode())
            time.sleep(0.1)  # Short delay for smooth communication

        print("Path sent to Arduino.")
        arduino.close()

    except Exception as e:
        print("Error communicating with Arduino:", e)
    """
else:
    print("No safe path found! All paths are blocked by fire.")

# Visualization
plt.figure(figsize=(12, 10))
plt.imshow(map_image, alpha=0.7)
plt.title("A* Pathfinding Visualization")

# Draw walkable areas
walkable_y, walkable_x = np.where(grid == 1)
plt.scatter(walkable_x, walkable_y, color="green", s=1, alpha=0.2, label="Walkable Areas")

# Draw safe points
for sp in safe_points:
    plt.scatter(sp[0], sp[1], color="blue", marker='*', s=200, label="Safe Point")

# Draw fire points
for fire_point in fire_points:
    plt.scatter(fire_point[0], fire_point[1], color="orange", s=100, label="Fire Point")

# Draw start point
plt.scatter(nearest_start[0], nearest_start[1], color="yellow", s=100, label="Start Point")

# Draw path
if path:
    path_x, path_y = zip(*path)
    plt.plot(path_x, path_y, color="red", linewidth=2, label="Shortest Path")

plt.legend()
plt.show()