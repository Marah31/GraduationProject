from flask import Flask, request, jsonify
import cv2
import numpy as np
from heapq import heappop, heappush
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from threading import Lock

app = Flask(__name__)

# Load the map image
map_image = cv2.imread("map.png")
map_image = cv2.cvtColor(map_image, cv2.COLOR_BGR2RGB)

# Grid dimensions
GRID_HEIGHT, GRID_WIDTH, _ = map_image.shape
grid = np.zeros((GRID_HEIGHT, GRID_WIDTH))  # 0: obstacle, 1: walkable

# ESP door locations (used as starting points)
esp_door_locations = {
    "ESP1": (594, 191),
    "ESP2": (633, 293),
    "ESP3": (354, 573)
}

# Safe points (exits)
safe_points = [(632, 54), (684, 505), (191, 552)]

# ESP room areas
esp_rooms = {
    "ESP1": (446, 92, 582, 204),
    "ESP2": (643, 202, 747, 322),
    "ESP3": (356, 583, 443, 660)
}

# Lock for grid updates
grid_lock = Lock()

# Mark walkable paths
def mark_rectangle(grid, x1, y1, x2, y2, value=1):
    x1, x2 = min(x1, x2), max(x1, x2)
    y1, y2 = min(y1, y2), max(y1, y2)
    grid[y1:y2 + 1, x1:x2 + 1] = value

mark_rectangle(grid, 446, 309, 592, 320)
mark_rectangle(grid, 446, 192, 592, 204)
mark_rectangle(grid, 446, 430, 592, 440)
mark_rectangle(grid, 633, 179, 750, 187)
mark_rectangle(grid, 592, 31, 634, 460)
mark_rectangle(grid, 464, 458, 690, 611)
mark_rectangle(grid, 185, 550, 464, 576)

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

# Store detected fire locations
fire_locations = {}

@app.route('/fire-alert', methods=['POST'])
def handle_fire_alert():
    with grid_lock:
        print("Fire alert received!")
        data = request.json
        if not data:
            return jsonify({"error": "Invalid JSON"}), 400

        esp_id = data.get("esp_id")

        if esp_id in esp_rooms and esp_id in esp_door_locations:
            try:
                # Block the ESP’s room area
                room_coords = esp_rooms[esp_id]
                mark_rectangle(grid, *room_coords, value=0)
                fire_locations[esp_id] = room_coords  # Store room area instead of a single location
                print(f" Fire detected in ESP {esp_id}'s room. Blocked area: {room_coords}")

                # Block corridor in front of the room
                if esp_id == "ESP1":
                    mark_rectangle(grid, 594, 213, 636, 213, value=0)
                elif esp_id == "ESP2":
                    mark_rectangle(grid, 637, 285, 587, 285, value=0)
                elif esp_id == "ESP3":
                    mark_rectangle(grid, 354, 593, 390, 593, value=0)

                # Compute safest paths for ALL ESPs
                all_paths = {}
                for node_id, start_location in esp_door_locations.items():
                    path = a_star_search(grid, start_location, safe_points)
                    all_paths[node_id] = path

                visualize_paths(all_paths, esp_id)  # Highlight the fire room

                if all_paths[esp_id]:
                    direction = get_direction_for_esp(esp_door_locations[esp_id], all_paths[esp_id])
                    return jsonify({"direction": direction}), 200
                else:
                    return jsonify({"direction": "no path"}), 200

            except Exception as e:
                print(f" Error processing request: {str(e)}")
                return jsonify({"error": "Server error", "details": str(e)}), 500

        return jsonify({"error": "Invalid data"}), 400

def get_direction_for_esp(start_location, path):
    """Determines whether the ESP should show LEFT or RIGHT based on its door location and the computed path."""
    for i in range(1, len(path)):
        prev_x, prev_y = path[i - 1]
        curr_x, curr_y = path[i]

        if curr_y == prev_y:  # Horizontal movement
            if curr_x > prev_x:
                return "right"
            elif curr_x < prev_x:
                return "left"

    return "no path"

def visualize_paths(all_paths, fire_esp_id):
    fig, ax = plt.subplots(figsize=(12, 10))
    ax.imshow(map_image, alpha=0.7)
    ax.set_title("Evacuation Paths for All ESPs")

    # Highlight fire area with a different color
    for esp_id, room_coords in fire_locations.items():
        x1, y1, x2, y2 = room_coords
        width = x2 - x1
        height = y2 - y1
        color = "darkred" if esp_id == fire_esp_id else "red"
        fire_rect = patches.Rectangle((x1, y1), width, height, linewidth=2, edgecolor=color, facecolor=color, alpha=0.5)
        ax.add_patch(fire_rect)
        ax.text(x1, y1 - 10, f" Fire: {esp_id}", color="white", fontsize=12, weight="bold")

    # Draw paths with small displacement
    for esp_id, path in all_paths.items():
        if path:
            offset_path = [(x + 2, y + 2) for x, y in path]
            path_x, path_y = zip(*offset_path)
            color = "yellow" if esp_id == fire_esp_id else "green"
            ax.plot(path_x, path_y, color=color, linewidth=4, label=f"Path for {esp_id}")

    # Draw safe points last (to be on top)
    for sp in safe_points:
        ax.scatter(sp[0], sp[1], color="blue", marker='*', s=350, edgecolors='black', linewidths=2, zorder=5, label="Safe Point")

    ax.legend()
    plt.show()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True, threaded=True)