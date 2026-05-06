import unittest
from collections import deque

from fastapi.testclient import TestClient

import main


client = TestClient(main.app)


class AppTests(unittest.TestCase):
    def test_home_page_loads_index_html(self):
        response = client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertIn("Car A* Controller", response.text)

    def test_solve_returns_astar_path_and_commands(self):
        payload = {
            "grid": [[0 for _ in range(5)] for _ in range(5)],
            "start": [0, 0],
            "end": [0, 2],
        }

        response = client.post("/api/solve", json=payload)

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["path"], [[0, 0], [0, 1], [0, 2]])
        self.assertEqual(response.json()["commands"], ["ADELANTE", "ADELANTE", "STOP"])

    def test_graph_renderer_uses_vertical_tree_layout(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("flattenSearchTree", html)
        self.assertIn("buildRouteTree", html)
        self.assertNotIn("node.col * cellStep", html)
        self.assertNotIn("graph-grid-v", html)

    def test_home_page_uses_only_local_assets(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn('/static/d3.min.js', html)
        self.assertNotIn('https://', html)
        self.assertNotIn('http://', html)
        self.assertNotIn('googleapis', html)
        self.assertNotIn('cdnjs', html)

    def test_local_d3_asset_is_served(self):
        response = client.get('/static/d3.min.js')

        self.assertEqual(response.status_code, 200)
        self.assertIn('javascript', response.headers['content-type'])
        self.assertIn('d3', response.text[:500].lower())

    def test_line_page_loads_line_follower_template(self):
        response = client.get("/linea")

        self.assertEqual(response.status_code, 200)
        self.assertIn("Modulo Seguidor de Linea", response.text)
        self.assertIn("Grafo fisico fijo", response.text)

    def test_line_page_has_fixed_astar_graph_ui(self):
        html = (main.BASE_DIR / "linea.html").read_text(encoding="utf-8")

        self.assertIn('/static/d3.min.js', html)
        self.assertNotIn('https://', html)
        self.assertNotIn('http://', html)
        self.assertIn("const lineGraph", html)
        self.assertIn("id: 'A'", html)
        self.assertIn("id: 'J'", html)
        self.assertIn("label: 'A'", html)
        self.assertIn("label: 'J'", html)
        self.assertEqual(html.count("id: '"), 10)
        self.assertIn("{ id: 'A', label: 'A', x: 100, y: 280, cost: 6 }", html)
        self.assertIn("{ id: 'B', label: 'B', x: 230, y: 280, cost: 14 }", html)
        self.assertIn("{ id: 'C', label: 'C', x: 370, y: 280, cost: 3 }", html)
        self.assertIn("{ id: 'D', label: 'D', x: 510, y: 280, cost: 11 }", html)
        self.assertIn("{ id: 'E', label: 'E', x: 650, y: 280, cost: 18 }", html)
        self.assertIn("{ id: 'F', label: 'F', x: 230, y: 120, cost: 7 }", html)
        self.assertIn("{ id: 'G', label: 'G', x: 510, y: 120, cost: 20 }", html)
        self.assertIn("{ id: 'H', label: 'H', x: 510, y: 440, cost: 2 }", html)
        self.assertIn("{ id: 'I', label: 'I', x: 650, y: 440, cost: 16 }", html)
        self.assertIn("{ id: 'J', label: 'J', x: 790, y: 440, cost: 9 }", html)
        self.assertIn("{ source: 'A', target: 'B' }", html)
        self.assertIn("{ source: 'C', target: 'D' }", html)
        self.assertIn("{ source: 'D', target: 'E' }", html)
        self.assertIn("{ source: 'B', target: 'F' }", html)
        self.assertIn("{ source: 'D', target: 'G' }", html)
        self.assertIn("{ source: 'D', target: 'H' }", html)
        self.assertIn("{ source: 'E', target: 'I' }", html)
        self.assertIn("{ source: 'H', target: 'I' }", html)
        self.assertIn("{ source: 'I', target: 'J' }", html)
        self.assertIn("const tentativeG = gScore.get(current.id) + nodesById.get(neighbor.id).cost", html)
        self.assertIn("function selectEndpoint(type, nodeId)", html)
        self.assertIn("function solveLineGraph()", html)
        self.assertIn("function astarGraph(startId, endId)", html)
        self.assertIn("function renderSearchTree(tree)", html)
        self.assertIn("fetch('/api/send'", html)
        self.assertIn("ADELANTE", html)
        self.assertIn("STOP", html)

    def test_line_page_has_static_svg_fallback_graph(self):
        html = (main.BASE_DIR / "linea.html").read_text(encoding="utf-8")

        self.assertIn('id="line-graph-fallback"', html)
        self.assertEqual(html.count('class="fallback-node"'), 10)
        self.assertIn('A c:6', html)
        self.assertIn('J c:9', html)
        self.assertIn('#line-graph-svg { height: 540px;', html)

    def test_line_page_tree_svg_has_explicit_render_area(self):
        html = (main.BASE_DIR / "linea.html").read_text(encoding="utf-8")

        self.assertIn('#search-tree-svg { height: 520px;', html)
        self.assertIn("treeSvg.setAttribute('viewBox', `0 0 ${treeWidth} ${treeHeight}`)", html)
        self.assertIn("const treeWidth = 1200", html)
        self.assertIn("const treeHeight = 520", html)
        self.assertIn("const renderedTreeHeight = Math.max(treeHeight, 140 + maxDepth * 105)", html)
        self.assertIn("treeSvg.style.height = `${treeHeight}px`", html)

    def test_line_page_uses_native_svg_rendering_for_graph_and_tree(self):
        html = (main.BASE_DIR / "linea.html").read_text(encoding="utf-8")

        self.assertIn("function renderLineGraph()", html)
        self.assertIn("function renderSearchTree(tree)", html)
        self.assertIn("line-graph-node", html)
        self.assertIn("onclick=\"selectEndpoint", html)
        self.assertIn("function flattenSearchTree", html)
        self.assertIn("function renderOptimalRoute(result)", html)
        self.assertIn('id="optimal-route"', html)
        self.assertIn("Árbol de recorrido", html)
        self.assertIn("Nodo ${node.id} | c:${enteredCost}", html)
        self.assertNotIn("Nodo ${node.id} (${node.x},${node.y})", html)
        self.assertNotIn("d3.select('#line-graph-svg')", html)
        self.assertNotIn("d3.hierarchy(tree)", html)

    def test_line_follower_esp32_template_exists(self):
        sketch = main.BASE_DIR / "esp32_carro_linea" / "esp32_carro_linea.ino"
        code = sketch.read_text(encoding="utf-8")

        self.assertIn("PIN_L298N_IN1", code)
        self.assertIn("PIN_TCRT_LEFT", code)
        self.assertIn("PIN_TCRT_RIGHT", code)
        self.assertIn("MPU6050_ADDR", code)
        self.assertIn("followLineUntilIntersection", code)
        self.assertIn("executeTurnToHeading", code)
        self.assertIn("WebSocketsClient", code)
        self.assertIn("GIRAR_NORTE", code)
        self.assertIn("quoteStart = msg.indexOf", code)
        self.assertIn("executeCommand(command)", code)
        self.assertNotIn("commands.indexOf(\"\\\",\\\"\", pos)", code)

    def test_home_page_links_to_line_follower_module(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn('href="/linea"', html)
        self.assertIn("SEGUIDOR DE LINEA", html)

    def test_solve_rejects_start_on_obstacle(self):
        payload = {
            "grid": [[1, 0], [0, 0]],
            "start": [0, 0],
            "end": [1, 1],
        }

        response = client.post("/api/solve", json=payload)

        self.assertEqual(response.status_code, 200)
        self.assertFalse(response.json()["success"])
        self.assertIn("inicio", response.json()["message"].lower())

    def test_solve_rejects_end_outside_grid(self):
        payload = {
            "grid": [[0, 0], [0, 0]],
            "start": [0, 0],
            "end": [2, 0],
        }

        response = client.post("/api/solve", json=payload)

        self.assertEqual(response.status_code, 200)
        self.assertFalse(response.json()["success"])
        self.assertIn("fin", response.json()["message"].lower())

    def test_solve_rejects_non_rectangular_grid(self):
        payload = {
            "grid": [[0, 0], [0]],
            "start": [0, 0],
            "end": [0, 1],
        }

        response = client.post("/api/solve", json=payload)

        self.assertEqual(response.status_code, 200)
        self.assertFalse(response.json()["success"])
        self.assertIn("rectangular", response.json()["message"].lower())

    def test_home_page_has_random_example_button(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("loadExample()", html)
        self.assertIn("EJEMPLO", html)
        self.assertIn("generateExampleScenario", html)

    def test_graph_panel_is_larger(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("grid-template-columns: minmax(820px, 1fr)", html)
        self.assertIn("#graph { grid-column: 1 / -1; }", html)
        self.assertIn("height: 760px", html)
        self.assertIn("@media", html)

    def test_graph_panel_is_below_grid_not_inside_right_column(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        right_col_start = html.index('<div class="right-col">')
        right_col_end = html.index('<!-- Graph Panel Below Grid -->')
        graph_start = html.index('<div class="panel" id="graph"')

        self.assertGreater(graph_start, right_col_end)
        self.assertNotIn('id="graph"', html[right_col_start:right_col_end])

    def test_graph_renderer_uses_readable_large_node_labels(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn('r="30"', html)
        self.assertIn("tree-node", html)
        self.assertIn("tree-edge", html)
        self.assertIn("number-text", html)
        self.assertIn("node-cost", html)
        self.assertIn("graph-content", html)
        self.assertIn("renderGraphInto('graph-svg', graph, {", html)
        self.assertIn("width: 1200", html)

    def test_graph_renderer_has_bottom_space_for_terminal_node_costs(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("depth * 105", html)
        self.assertIn("140 + maxDepth", html)
        self.assertIn("renderedHeight", html)

    def test_ui_renders_optimal_route_cost_summary(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn('id="optimal-route"', html)
        self.assertIn("function renderOptimalRoute(path, graphNodes, totalCost)", html)
        self.assertIn("renderOptimalRoute(data.path, data.graph.nodes, data.totalCost)", html)
        self.assertIn("Recorrido óptimo", html)
        self.assertIn("Costo total óptimo", html)

    def test_commands_and_optimal_route_are_separate_panels(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        commands_title = html.index("// Comandos generados")
        route_panel = html.index('id="optimal-route-panel"')
        commands_panel_end = html.index("<!-- Optimal Route Panel -->")

        self.assertGreater(route_panel, commands_panel_end)
        self.assertNotIn('id="optimal-route"', html[commands_title:commands_panel_end])

    def test_solve_returns_search_tree_graph_with_path_highlight(self):
        payload = {
            "grid": [[0 for _ in range(4)] for _ in range(4)],
            "start": [0, 0],
            "end": [3, 3],
        }

        response = client.post("/api/solve", json=payload)
        data = response.json()

        self.assertEqual(response.status_code, 200)
        self.assertTrue(data["success"])
        self.assertGreater(len(data["graph"]["nodes"]), len(data["path"]))
        self.assertTrue(any(node["inPath"] for node in data["graph"]["nodes"]))
        self.assertTrue(any(not node["inPath"] for node in data["graph"]["nodes"]))
        self.assertEqual([node["id"] for node in data["graph"]["nodes"] if node["isStart"]], ["0-0"])
        self.assertEqual([node["id"] for node in data["graph"]["nodes"] if node["isEnd"]], ["3-3"])
        self.assertTrue(any(edge["inPath"] for edge in data["graph"]["edges"]))
        self.assertTrue(any(not edge["inPath"] for edge in data["graph"]["edges"]))

    def test_solve_returns_unique_grid_number_for_graph_nodes(self):
        payload = {
            "grid": [[0 for _ in range(4)] for _ in range(4)],
            "start": [0, 0],
            "end": [3, 3],
        }

        response = client.post("/api/solve", json=payload)
        data = response.json()

        self.assertEqual(response.status_code, 200)
        self.assertTrue(data["success"])
        for node in data["graph"]["nodes"]:
            self.assertEqual(node["number"], node["row"] * 4 + node["col"] + 1)

    def test_graph_has_fullscreen_modal_controls(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("VER GRAFO COMPLETO", html)
        self.assertIn("openGraphModal()", html)
        self.assertIn("closeGraphModal()", html)
        self.assertIn("graph-modal-svg", html)

    def test_graph_renderer_uses_orthogonal_paths(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("renderGraphInto", html)
        self.assertIn("tree-edge", html)
        self.assertIn("V${midY} H${item.x}", html)

    def test_graph_renderer_uses_hierarchy_layout_data(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("flattenSearchTree(rootData)", html)
        self.assertIn("flatNodes.filter(item => item.data.parent)", html)
        self.assertIn("item.x", html)
        self.assertIn("item.y", html)
        self.assertNotIn(".data(nodes)\n    .join('g')", html)

    def test_graph_renderer_maps_tree_depth_vertically(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("depth * 105", html)
        self.assertIn("item.x = ((index + 1) * W)", html)
        self.assertIn("item.y = 70 + depth", html)
        self.assertNotIn("d.y = margin.left + treeX", html)

    def test_graph_renderer_uses_larger_nodes_and_offset_arrows(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn('r="30"', html)
        self.assertIn("parent.y + 30", html)
        self.assertIn("item.y - 30", html)
        self.assertIn("tree-node", html)

    def test_astar_returns_optimal_path_for_obstacle_grid(self):
        grid = [
            [0, 0, 0, 0, 0],
            [1, 1, 1, 1, 0],
            [0, 0, 0, 0, 0],
            [0, 1, 1, 1, 1],
            [0, 0, 0, 0, 0],
        ]
        start = [0, 0]
        end = [4, 4]

        path, explored, came_from, node_details, total_cost = main.astar(grid, start, end)

        self.assertEqual(path[0], start)
        self.assertEqual(path[-1], end)
        self.assertEqual(len(path), self._shortest_path_length(grid, tuple(start), tuple(end)))
        self.assertGreater(len(explored), 0)
        self.assertIn(tuple(end), came_from)
        self.assertEqual(total_cost, len(path) - 1)
        self.assertEqual(node_details[tuple(end)]["g"], total_cost)

    def test_astar_uses_neighbor_costs_and_returns_cost_details(self):
        grid = [[0, 0, 0], [0, 0, 0]]
        costs = [[1, 9, 1], [1, 1, 1]]

        path, explored, came_from, node_details, total_cost = main.astar(grid, [0, 0], [0, 2], costs)

        self.assertEqual(path, [[0, 0], [1, 0], [1, 1], [1, 2], [0, 2]])
        self.assertEqual(total_cost, 4)
        self.assertEqual(came_from[(0, 2)], (1, 2))
        self.assertEqual(node_details[(0, 0)]["g"], 0)
        self.assertEqual(node_details[(0, 0)]["h"], 2)
        self.assertEqual(node_details[(0, 0)]["f"], 2)
        self.assertEqual(node_details[(0, 2)]["cost"], 1)
        self.assertEqual(node_details[(0, 2)]["g"], 4)
        self.assertEqual(node_details[(0, 2)]["h"], 0)
        self.assertEqual(node_details[(0, 2)]["f"], 4)
        self.assertEqual(node_details[(0, 2)]["parent"], (1, 2))

    def test_astar_5x5_manhattan_heuristic_f_g_h_values(self):
        grid = [[0] * 5 for _ in range(5)]
        start = [0, 0]
        end = [4, 4]

        path, explored, came_from, node_details, total_cost = main.astar(grid, start, end)

        h_start = abs(0 - 4) + abs(0 - 4)
        h_end = abs(4 - 4) + abs(4 - 4)

        self.assertEqual(node_details[(0, 0)]["g"], 0)
        self.assertEqual(node_details[(0, 0)]["h"], h_start)
        self.assertEqual(node_details[(0, 0)]["f"], h_start)

        self.assertEqual(node_details[(4, 4)]["g"], total_cost)
        self.assertEqual(node_details[(4, 4)]["h"], h_end)
        self.assertEqual(node_details[(4, 4)]["f"], total_cost)

        intermediate = path[len(path) // 2]
        self.assertIn(tuple(intermediate), node_details)
        self.assertGreater(node_details[tuple(intermediate)]["g"], 0)
        self.assertGreater(node_details[tuple(intermediate)]["h"], 0)
        self.assertEqual(node_details[tuple(intermediate)]["f"], node_details[tuple(intermediate)]["g"] + node_details[tuple(intermediate)]["h"])

    def test_solve_returns_weighted_graph_costs_and_total_cost(self):
        payload = {
            "grid": [[0, 0, 0], [0, 0, 0]],
            "costs": [[1, 9, 1], [1, 1, 1]],
            "start": [0, 0],
            "end": [0, 2],
        }

        response = client.post("/api/solve", json=payload)
        data = response.json()

        self.assertEqual(response.status_code, 200)
        self.assertTrue(data["success"])
        self.assertEqual(data["path"], [[0, 0], [1, 0], [1, 1], [1, 2], [0, 2]])
        self.assertEqual(data["totalCost"], 4)
        end_node = next(node for node in data["graph"]["nodes"] if node["isEnd"])
        self.assertEqual(end_node["cost"], 1)
        self.assertEqual(end_node["g"], 4)
        self.assertEqual(end_node["h"], 0)
        self.assertEqual(end_node["f"], 4)
        self.assertEqual(end_node["parent"], "1-2")

    def _shortest_path_length(self, grid, start, end):
        queue = deque([(start, 1)])
        visited = {start}

        while queue:
            (row, col), distance = queue.popleft()
            if (row, col) == end:
                return distance

            for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nr, nc = row + dr, col + dc
                neighbor = (nr, nc)
                if 0 <= nr < len(grid) and 0 <= nc < len(grid[0]) and grid[nr][nc] == 0 and neighbor not in visited:
                    visited.add(neighbor)
                    queue.append((neighbor, distance + 1))

        return None

    def test_graph_modal_has_zoom_controls(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("zoomGraphModal(-0.2)", html)
        self.assertIn("zoomGraphModal(0.2)", html)
        self.assertIn("function zoomGraphModal(delta)", html)
        self.assertIn("applyGraphZoom", html)

    def test_graph_modal_has_scrollable_viewport_for_zoomed_graph(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn('class="graph-modal-viewport"', html)
        self.assertIn(".graph-modal-viewport", html)
        self.assertIn("overflow: auto", html)
        self.assertIn("transform-origin: 0 0", html)

    def test_graph_modal_zoom_resizes_canvas_for_full_navigation(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("baseWidth", html)
        self.assertIn("baseHeight", html)
        self.assertIn("setGraphCanvasSize(svgEl, W, renderedHeight)", html)
        self.assertIn("svg.dataset.baseWidth", html)
        self.assertIn("svg.style.width = `${baseWidth * graphModalZoom}px`", html)
        self.assertIn("svg.style.height = `${baseHeight * graphModalZoom}px`", html)
        self.assertIn("fixedCanvas: true", html)
        self.assertIn("tree-node", html)

    def test_grid_and_graph_show_unique_cell_numbers(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("function cellNumber(r, c)", html)
        self.assertIn('<span class="cell-num">${cellNumber(r, c)}</span>', html)
        self.assertIn('<span class="coord">${r},${c}</span>', html)
        self.assertIn('class="number-text"', html)

    def test_ui_renders_astar_costs_and_legend(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("let costGrid", html)
        self.assertIn("function randomCellCost()", html)
        self.assertIn("function renderCellContent(r, c, metrics = null)", html)
        self.assertIn("c = costo de entrar al nodo", html)
        self.assertIn("g = costo acumulado desde el inicio", html)
        self.assertIn("h = estimación hasta la meta", html)
        self.assertIn("f = g + h", html)
        self.assertIn("renderCostMetrics(data.graph.nodes)", html)
        self.assertIn("Costo total", html)
        self.assertIn('class="node-cost"', html)
        self.assertIn('class="small"', html)

    def test_ui_uses_arduino_ide_dark_palette(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("--bg: #11191f", html)
        self.assertIn("--surface: #182226", html)
        self.assertIn("--accent: #00a6a6", html)
        self.assertIn("border-radius: 999px", html)

    def test_grid_is_larger_for_readability(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertIn("grid-template-columns: minmax(820px, 1fr)", html)
        self.assertIn("grid-template-columns: repeat(5, 76px)", html)
        self.assertIn("grid-template-rows: repeat(5, 76px)", html)
        self.assertIn("width: 76px; height: 76px", html)
        self.assertIn("font-size: 1.25rem", html)

    def test_start_and_end_cells_use_fixed_colors_without_letters(self):
        html = (main.BASE_DIR / "index.html").read_text(encoding="utf-8")

        self.assertNotIn(".cell.start::after", html)
        self.assertNotIn(".cell.end::after", html)
        self.assertIn(".cell.start.path { background: rgba(0,184,169,0.18); border: 2px solid var(--green); }", html)
        self.assertIn(".cell.end.path { background: rgba(228,113,40,0.18); border: 2px solid var(--accent2); }", html)
        self.assertIn(".cell.start.path::before, .cell.end.path::before { display: none; }", html)

    def test_readme_documents_project_setup_and_esp32_flow(self):
        readme = (main.BASE_DIR / "README.md").read_text(encoding="utf-8")

        self.assertIn("# CarroESP IA Web", readme)
        self.assertIn("FastAPI", readme)
        self.assertIn("A*", readme)
        self.assertIn("WebSocket", readme)
        self.assertIn(".venv\\Scripts\\python.exe main.py", readme)
        self.assertIn(".venv\\Scripts\\python.exe -m unittest test_main -v", readme)
        self.assertIn("192.168.4.2:8000/ws/car", readme)
        self.assertIn("ADELANTE", readme)


if __name__ == "__main__":
    unittest.main()
