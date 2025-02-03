from flask import Flask, request, jsonify
import cv2
import numpy as np
from heapq import heappop, heappush
import matplotlib.pyplot as plt

app = Flask(__name__)

# Load the map image
map_image = cv2.imread("map.png")
map_image = cv2.cvtColor(map_image, cv2.COLOR_BGR2RGB)

# Grid dimensions
GRID_HEIGHT, GRID_WIDTH, _ = map_image.shape
grid = np.zeros((GRID_HEIGHT, GRID_WIDTH))  # 0: obstacle, 1: walkable

# Define nodes with IDs and positions
nodes = {
    1: (689, 123),
    2: (505, 383),
    3: (273, 622)
}

# Safe points (exits)
safe_points = [(632, 54), (684, 505), (191, 552)]

# Known person start point
person_location = (476, 200)


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


# A* Algorithm
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
            if (0 <= neighbor[0] < cols and 0 <= neighbor[1] < rows and grid[neighbor[1], neighbor[0]] == 1):
                tentative_g_score = g_score[current] + 1
                if (neighbor not in g_score or tentative_g_score < g_score[neighbor]):
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g_score
                    f_score[neighbor] = tentative_g_score + min(heuristic(neighbor, goal) for goal in goals)
                    heappush(open_set, (f_score[neighbor], neighbor))
    return None


# Flask API endpoints
fire_locations = {}


@app.route('/fire-alert', methods=['POST'])
def handle_fire_alert():
    data = request.json
    esp_id = data.get("esp_id")
    location = tuple(data.get("location"))

    if esp_id and location:
        fire_locations[esp_id] = location
        grid[max(0, location[1] - 1):min(GRID_HEIGHT, location[1] + 2),
        max(0, location[0] - 1):min(GRID_WIDTH, location[0] + 2)] = 0

        # Compute the safest path for the person
        path = a_star_search(grid, person_location, safe_points)
        if path:
            visualize_paths(path)
            return jsonify({"path": path}), 200
        return jsonify({"error": "No safe path found"}), 404

    return jsonify({"error": "Invalid data"}), 400


def visualize_paths(path):
    plt.figure(figsize=(12, 10))
    plt.imshow(map_image, alpha=0.7)
    plt.title("Pathfinding Visualization")

    for esp_id, fire_position in fire_locations.items():
        plt.scatter(fire_position[0], fire_position[1], color="red", s=200, label=f"Fire: {esp_id}")

    for sp in safe_points:
        plt.scatter(sp[0], sp[1], color="blue", marker='*', s=200, label="Safe Point")

    if path:
        path_x, path_y = zip(*path)
        plt.plot(path_x, path_y, color="green", linewidth=2, label="Evacuation Path")

    plt.legend()
    plt.show()


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
