from flask import Flask, request, jsonify
import cv2
import numpy as np
from heapq import heappop, heappush
import matplotlib.pyplot as plt
import matplotlib.patches as patches

app = Flask(__name__)

# Load the map image
map_image = cv2.imread("map.png")
map_image = cv2.cvtColor(map_image, cv2.COLOR_BGR2RGB)

# Grid dimensions
GRID_HEIGHT, GRID_WIDTH, _ = map_image.shape
grid = np.zeros((GRID_HEIGHT, GRID_WIDTH))  # 0: obstacle, 1: walkable

# Define ESP locations (room doors)
esp_door_locations = {
    "ESP1": (594, 191), #this is the correct door
    "ESP2": (633, 293),
    "ESP3": (354, 573)
}

# Safe points (exits)
safe_points = [(632, 54), (684, 505), (191, 552)]

# Define ESP room areas
esp_rooms = {
    "ESP1": (446, 192, 592, 204),
    "ESP2": (643, 202, 747, 322),
    "ESP3": (356, 583, 443, 660)
}

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
    print("🔥 Fire alert received!")
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
            print(f"🔥 Fire detected in ESP {esp_id}'s room. Blocked area: {room_coords}")

            # Compute safest path **from ESP's door location**
            start_location = esp_door_locations[esp_id]
            path = a_star_search(grid, start_location, safe_points)

            if path:
                visualize_paths(path)
                print(f"✅ Path computed for {esp_id}: {path}")

                # Determine direction for this ESP
                direction = get_direction_for_esp(start_location, path)
                response_data = {"direction": direction}
                print(f"📡 Sending response: {response_data}")  # Log response

                return jsonify(response_data), 200
            else:
                print(f"❌ No safe path found for {esp_id}!")
                return jsonify({"direction": "no path"}), 200  # Send no path so ESP can handle danger LED

        except Exception as e:
            print(f"🚨 Error processing request: {str(e)}")
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

def visualize_paths(path):
    fig, ax = plt.subplots(figsize=(12, 10))
    ax.imshow(map_image, alpha=0.7)
    ax.set_title("Evacuation Path Based on ESP Location")

    # Draw fire areas based on ESP rooms
    for esp_id, room_coords in fire_locations.items():
        x1, y1, x2, y2 = room_coords
        width = x2 - x1
        height = y2 - y1
        fire_rect = patches.Rectangle((x1, y1), width, height, linewidth=2, edgecolor='red', facecolor='red', alpha=0.5)
        ax.add_patch(fire_rect)
        ax.text(x1, y1 - 10, f"Fire: {esp_id}", color="red", fontsize=12, weight="bold")

    # Draw safe points
    for sp in safe_points:
        ax.scatter(sp[0], sp[1], color="blue", marker='*', s=200, label="Safe Point")

    # Draw path
    if path:
        path_x, path_y = zip(*path)
        ax.plot(path_x, path_y, color="green", linewidth=2, label="Evacuation Path")

    ax.legend()
    plt.show()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
