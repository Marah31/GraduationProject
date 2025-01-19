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
# Mark walkable paths
# Left rooms
mark_rectangle(grid, 446, 309, 592, 320, value=1)
mark_rectangle(grid, 446, 192, 592, 204, value=1)
mark_rectangle(grid, 446, 430, 592, 440, value=1)

# Right rooms
mark_rectangle(grid, 633, 179, 750, 187, value=1)
mark_rectangle(grid, 633, 313, 750, 316, value=1)
mark_rectangle(grid, 633, 338, 750, 340, value=1)

# Bathrooms
mark_rectangle(grid, 474, 55, 605, 75, value=1)

# Corridors and free spaces
mark_rectangle(grid, 592, 31, 634, 460, value=1)
mark_rectangle(grid, 464, 458, 690, 611, value=1)
mark_rectangle(grid, 185, 550, 464, 576, value=1)

# Safe points (stairs)
safe_points = [(632, 54), (684, 505), (191, 552)]


def heuristic(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def a_star_search(grid, start, goals):
    rows, cols = grid.shape

    # Check if start point is walkable
    if grid[start[1], start[0]] != 1:
        print(f"Start point {start} is not on a walkable area!")
        return None

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


# Choose a start point that we know is on a walkable area
start_point = (456, 556)  # On the vertical corridor

# Debugging: Check if start point is walkable
if grid[start_point[1], start_point[0]] == 0:
    print(f"Start point {start_point} is NOT in a walkable area!")

# Find path
path = a_star_search(grid, start_point, safe_points)

# Debugging: Print the found path
print("Path:", path)

# Visualization
plt.figure(figsize=(12, 10))
plt.imshow(map_image, alpha=0.7)  # Make base image slightly more visible
plt.title("A* Pathfinding Visualization")

# Draw walkable areas with lower opacity
walkable_y, walkable_x = np.where(grid == 1)
plt.scatter(walkable_x, walkable_y, color="green", s=1, alpha=0.2, label="Walkable Areas")

# Draw safe points
for sp in safe_points:
    plt.scatter(sp[0], sp[1], color="blue", marker='*', s=200,
                label="Safe Point" if "Safe Point" not in plt.gca().get_legend_handles_labels()[1] else "")
    plt.annotate(f'Safe Point', (sp[0], sp[1]), xytext=(5, 5),
                 textcoords='offset points', color='blue', fontweight='bold')

# Draw the start point
plt.scatter(start_point[0], start_point[1], color="yellow", s=100, label="Start Point", zorder=5)
plt.annotate('Start', (start_point[0], start_point[1]), xytext=(5, 5),
             textcoords='offset points', color='black', fontweight='bold')

# Draw the path if found
if path:
    path_x, path_y = zip(*path)
    # Debugging: Print path coordinates
    print("Path coordinates (x, y):", list(zip(path_x, path_y)))

    # Draw path with high opacity and increased width
    plt.plot(path_x, path_y, color="red", linewidth=2, solid_capstyle='round',
             label="Shortest Path", zorder=4)

    # Add direction arrows along the path
    for i in range(len(path) - 1):
        mid_x = (path_x[i] + path_x[i + 1]) / 2
        mid_y = (path_y[i] + path_y[i + 1]) / 2
        dx = path_x[i + 1] - path_x[i]
        dy = path_y[i + 1] - path_y[i]
        plt.arrow(mid_x - dx * 0.2, mid_y - dy * 0.2, dx * 0.4, dy * 0.4,
                  head_width=5, head_length=10, fc='red', ec='red', zorder=6)

plt.legend()
plt.show()
