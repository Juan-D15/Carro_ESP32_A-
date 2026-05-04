from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel
from pathlib import Path
import heapq
import json

app = FastAPI(title="Car A* Controller")
BASE_DIR = Path(__file__).resolve().parent
templates = Jinja2Templates(directory=str(BASE_DIR))
app.mount("/static", StaticFiles(directory=str(BASE_DIR / "static")), name="static")

# ─── WebSocket manager para ESP32 ─────────────────────────────────────────────
class ESP32Manager:
    def __init__(self):
        self.connection: WebSocket | None = None

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.connection = ws
        print("ESP32 conectado")

    def disconnect(self):
        self.connection = None
        print("ESP32 desconectado")

    async def send_commands(self, commands: list[str]):
        if self.connection:
            await self.connection.send_text(json.dumps({"commands": commands}))
            return True
        return False

esp32 = ESP32Manager()

# ─── Modelos ──────────────────────────────────────────────────────────────────
class GridRequest(BaseModel):
    grid: list[list[int]]   # 0=libre, 1=obstáculo
    start: list[int]        # [fila, col]
    end: list[int]          # [fila, col]
    costs: list[list[int]] | None = None

# ─── Algoritmo A* ─────────────────────────────────────────────────────────────
def heuristic(a, b, min_cost=1):
    return (abs(a[0] - b[0]) + abs(a[1] - b[1])) * min_cost

def validate_grid_request(grid, start, end):
    if not grid or not all(isinstance(row, list) for row in grid):
        return "La cuadrícula no puede estar vacía"

    cols = len(grid[0])
    if cols == 0 or any(len(row) != cols for row in grid):
        return "La cuadrícula debe ser rectangular"

    rows = len(grid)

    for label, point in (("inicio", start), ("fin", end)):
        if len(point) != 2:
            return f"El punto de {label} debe tener fila y columna"
        r, c = point
        if not (0 <= r < rows and 0 <= c < cols):
            return f"El punto de {label} está fuera de la cuadrícula"
        if grid[r][c] == 1:
            return f"El punto de {label} no puede estar sobre un obstáculo"

    return None

def normalized_costs(grid, costs=None):
    rows, cols = len(grid), len(grid[0])
    if costs is None:
        return [[1 if grid[r][c] == 0 else float('inf') for c in range(cols)] for r in range(rows)]
    return [[costs[r][c] if grid[r][c] == 0 else float('inf') for c in range(cols)] for r in range(rows)]

def astar(grid, start, end, costs=None):
    rows, cols = len(grid), len(grid[0])
    start, end = tuple(start), tuple(end)
    node_costs = normalized_costs(grid, costs)
    min_cost = 1

    open_set = []
    start_h = heuristic(start, end, min_cost)
    heapq.heappush(open_set, (start_h, start_h, start))

    came_from = {}
    g_score = {start: 0}
    f_score = {start: start_h}
    node_details = {
        start: {
            "cost": node_costs[start[0]][start[1]],
            "g": 0,
            "h": start_h,
            "f": start_h,
            "parent": None,
        }
    }
    explored = []
    closed_set = set()

    directions = [(-1,0),(1,0),(0,-1),(0,1)]

    while open_set:
        _, _, current = heapq.heappop(open_set)

        if current in closed_set:
            continue

        if current == end:
            path = []
            while current in came_from:
                path.append(list(current))
                current = came_from[current]
            path.append(list(start))
            path.reverse()
            return path, explored, came_from, node_details, g_score[end]

        closed_set.add(current)
        explored.append(list(current))

        for dr, dc in directions:
            nr, nc = current[0] + dr, current[1] + dc
            neighbor = (nr, nc)
            if 0 <= nr < rows and 0 <= nc < cols and grid[nr][nc] == 0 and neighbor not in closed_set:
                tentative_g = g_score[current] + node_costs[nr][nc]
                if tentative_g < g_score.get(neighbor, float('inf')):
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g
                    h = heuristic(neighbor, end, min_cost)
                    f_score[neighbor] = tentative_g + h
                    node_details[neighbor] = {
                        "cost": node_costs[nr][nc],
                        "g": tentative_g,
                        "h": h,
                        "f": f_score[neighbor],
                        "parent": current,
                    }
                    heapq.heappush(open_set, (f_score[neighbor], h, neighbor))

    return None, explored, came_from, node_details, None  # Sin camino

def path_to_commands(path: list) -> list[str]:
    """Convierte la lista de celdas en comandos de movimiento."""
    if not path or len(path) < 2:
        return []

    direction_map = {
        (-1, 0): "NORTE",
        (1,  0): "SUR",
        (0, -1): "OESTE",
        (0,  1): "ESTE",
    }

    commands = []
    current_dir = None

    for i in range(1, len(path)):
        dr = path[i][0] - path[i-1][0]
        dc = path[i][1] - path[i-1][1]
        new_dir = direction_map.get((dr, dc))

        if new_dir != current_dir:
            if current_dir is not None:
                commands.append(f"GIRAR_{new_dir}")
            current_dir = new_dir

        commands.append("ADELANTE")

    commands.append("STOP")
    return commands

# ─── Rutas ────────────────────────────────────────────────────────────────────
@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse(request, "index.html")

@app.post("/api/solve")
async def solve(data: GridRequest):
    validation_error = validate_grid_request(data.grid, data.start, data.end)
    if validation_error:
        return {"success": False, "message": validation_error}

    path, explored, came_from, node_details, total_cost = astar(data.grid, data.start, data.end, data.costs)

    if path is None:
        return {"success": False, "message": "No existe camino disponible"}

    commands = path_to_commands(path)

    # Construir árbol de búsqueda de A* y resaltar la ruta final elegida.
    path_points = [tuple(p) for p in path]
    path_set = set(path_points)
    path_edges = {(path_points[i], path_points[i + 1]) for i in range(len(path_points) - 1)}
    start_point = tuple(data.start)
    end_point = tuple(data.end)
    graph_points = sorted(set(came_from.keys()) | {tuple(data.start)})

    nodes = [
        {
            "id": f"{r}-{c}",
            "row": r,
            "col": c,
            "number": r * len(data.grid[0]) + c + 1,
            "cost": node_details[(r, c)]["cost"],
            "g": node_details[(r, c)]["g"],
            "h": node_details[(r, c)]["h"],
            "f": node_details[(r, c)]["f"],
            "parent": f"{node_details[(r, c)]['parent'][0]}-{node_details[(r, c)]['parent'][1]}" if node_details[(r, c)]["parent"] else None,
            "inPath": (r, c) in path_set,
            "isStart": (r, c) == start_point,
            "isEnd": (r, c) == end_point,
        }
        for r, c in graph_points
    ]
    edges = [
        {
            "source": f"{parent[0]}-{parent[1]}",
            "target": f"{child[0]}-{child[1]}",
            "inPath": (parent, child) in path_edges,
        }
        for child, parent in came_from.items()
    ]

    return {
        "success": True,
        "path": path,
        "explored": explored,
        "commands": commands,
        "totalCost": total_cost,
        "graph": {"nodes": nodes, "edges": edges},
    }

@app.post("/api/send")
async def send_to_car(data: dict):
    commands = data.get("commands", [])
    sent = await esp32.send_commands(commands)
    if sent:
        return {"success": True, "message": f"{len(commands)} comandos enviados al carro"}
    return {"success": False, "message": "ESP32 no conectado"}

@app.get("/api/status")
async def status():
    return {"esp32_connected": esp32.connection is not None}

@app.websocket("/ws/car")
async def websocket_car(ws: WebSocket):
    await esp32.connect(ws)
    try:
        while True:
            data = await ws.receive_text()
            print(f"ESP32 dice: {data}")
    except WebSocketDisconnect:
        esp32.disconnect()
