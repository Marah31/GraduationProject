import cv2
import matplotlib.pyplot as plt
import numpy as np
# Load the map image
map_image = cv2.imread('map.png', cv2.IMREAD_COLOR)

# Convert the image to RGB (OpenCV loads images in BGR format)
map_image = cv2.cvtColor(map_image, cv2.COLOR_BGR2RGB)
# Load the original image
image = cv2.imread("map.png")

# Define the region of interest (ROI) - the usable area of the building
x_start, y_start = 469, 17  # Top-left corner of the building
x_end, y_end = 741, 617    # Bottom-right corner of the building

# Crop the image
cropped_image = image[y_start:y_end, x_start:x_end]

# Display the cropped image
cv2.imshow("Cropped Building", cropped_image)
cv2.waitKey(0)
cv2.destroyAllWindows()

# Save the cropped image if needed
cv2.imwrite("cropped_map.png", cropped_image)

# Display the map using Matplotlib
plt.imshow(map_image)
plt.title("Building Map")
plt.axis("off")
plt.show()


# Define grid size (smaller values for finer grid)
grid_size = 20
height, width, _ = map_image.shape

# Create an empty grid
rows = height // grid_size
cols = width // grid_size
grid = np.zeros((rows, cols), dtype=np.int32)

# Overlay the grid on the map
for i in range(rows):
    for j in range(cols):
        # Define each cell's rectangle
        start_x, start_y = j * grid_size, i * grid_size
        end_x, end_y = (j + 1) * grid_size, (i + 1) * grid_size
        # Check if the cell is walkable (e.g., white or light-colored)
        cell = map_image[start_y:end_y, start_x:end_x]
        if np.mean(cell) > 200:  # Adjust threshold based on the map
            grid[i, j] = 0  # Walkable
        else:
            grid[i, j] = 1  # Obstacle
        # Draw the grid on the map for visualization
        cv2.rectangle(map_image, (start_x, start_y), (end_x, end_y), (255, 0, 0), 1)

# Display the grid-overlayed map
plt.imshow(map_image)
plt.title("Grid Overlay")
plt.axis("off")
plt.show()
import heapq

# A* Algorithm
def a_star(grid, start, goal):
    rows, cols = grid.shape
    open_list = []
    heapq.heappush(open_list, (0, start))  # (cost, (x, y))
    came_from = {}
    g_score = {start: 0}
    f_score = {start: heuristic(start, goal)}

    while open_list:
        _, current = heapq.heappop(open_list)
        if current == goal:
            return reconstruct_path(came_from, current)

        for neighbor in get_neighbors(current, grid):
            tentative_g_score = g_score[current] + 1
            if tentative_g_score < g_score.get(neighbor, float('inf')):
                came_from[neighbor] = current
                g_score[neighbor] = tentative_g_score
                f_score[neighbor] = tentative_g_score + heuristic(neighbor, goal)
                if neighbor not in [item[1] for item in open_list]:
                    heapq.heappush(open_list, (f_score[neighbor], neighbor))
    return None

# Helper functions
def heuristic(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])

def get_neighbors(node, grid):
    x, y = node
    neighbors = []
    for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
        nx, ny = x + dx, y + dy
        if 0 <= nx < grid.shape[0] and 0 <= ny < grid.shape[1] and grid[nx, ny] == 0:
            neighbors.append((nx, ny))
    return neighbors

def reconstruct_path(came_from, current):
    path = [current]
    while current in came_from:
        current = came_from[current]
        path.append(current)
    return path[::-1]
# Example start and goal points
start = (5, 5)  # (row, col)
goal = (15, 20)  # (row, col)

# Run A* and get the path
path = a_star(grid, start, goal)

# Visualize the path
for (row, col) in path:
    start_x, start_y = col * grid_size, row * grid_size
    end_x, end_y = (col + 1) * grid_size, (row + 1) * grid_size
    cv2.rectangle(map_image, (start_x, start_y), (end_x, end_y), (0, 255, 0), -1)

# Display the result
plt.imshow(map_image)
plt.title("Shortest Path")
plt.axis("off")
plt.show()
