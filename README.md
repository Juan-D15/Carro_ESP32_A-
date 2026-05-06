# CarroESP IA Web

![Python](https://img.shields.io/badge/Python-3776AB?style=for-the-badge&logo=python&logoColor=white)
![FastAPI](https://img.shields.io/badge/FastAPI-009688?style=for-the-badge&logo=fastapi&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black)
![D3.js](https://img.shields.io/badge/D3.js-F9A03C?style=for-the-badge&logo=d3dotjs&logoColor=white)
![ESP32](https://img.shields.io/badge/ESP32-000000?style=for-the-badge&logo=espressif&logoColor=white)
![WebSocket](https://img.shields.io/badge/WebSocket-35495E?style=for-the-badge&logo=socketdotio&logoColor=white)

Interfaz web para controlar un carro robótico mediante el algoritmo `A*`, permitiendo calcular rutas sobre una cuadrícula interactiva y enviar comandos a un **ESP32** por medio de **WebSocket**.

---

## Características

- **Cuadrícula 8x8** interactiva con celdas de costo aleatorio.
- **Algoritmo `A*`** con costos ponderados y heurística Manhattan.
- **Visualización del grafo** del árbol de búsqueda usando D3.js.
- **Envío de comandos al ESP32** por WebSocket.
- Comandos soportados: `ADELANTE`, `ATRAS`, `STOP`, `GIRAR_*`.
- **Funcionamiento offline**, usando assets locales sin CDNs.
- **Estilo Arduino IDE**, con tema oscuro y acentos turquesa.
- Pruebas unitarias para backend e interfaz.

---

## Tecnologías Usadas

| Componente | Tecnología | Propósito |
|-----------|------------|-----------|
| **Backend** | ![FastAPI](https://img.shields.io/badge/FastAPI-009688?style=flat-square&logo=fastapi&logoColor=white) ![Python](https://img.shields.io/badge/Python-3776AB?style=flat-square&logo=python&logoColor=white) | API REST y WebSocket para comunicación con ESP32 |
| **Frontend** | ![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=flat-square&logo=html5&logoColor=white) ![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=flat-square&logo=css&logoColor=white) ![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=flat-square&logo=javascript&logoColor=black) | Interfaz web responsiva |
| **Visualización** | ![D3.js](https://img.shields.io/badge/D3.js-F9A03C?style=flat-square&logo=d3dotjs&logoColor=white) | Grafo del árbol de búsqueda |
| **Motor físico** | ![ESP32](https://img.shields.io/badge/ESP32-000000?style=flat-square&logo=espressif&logoColor=white) ![L298N](https://img.shields.io/badge/L298N-Motor_Driver-red?style=flat-square) | Control de 2 motores DC |
| **Testing** | ![unittest](https://img.shields.io/badge/unittest-Python-blue?style=flat-square) | Pruebas unitarias del backend y UI |
| **Offline** | ![Offline](https://img.shields.io/badge/Offline-Assets_Locales-success?style=flat-square) | Sin dependencias externas ni CDNs |

---

## Diagrama de Arquitectura

```txt
┌─────────────────┐      ┌─────────────────┐
│   Navegador     │──────│    FastAPI      │
│   index.html    │◄─────│    Python       │
└─────────────────┘      └────────┬────────┘
                                   │
                          ┌────────┴────────┐
                          │   WebSocket     │
                          │    /ws/car      │
                          └────────┬────────┘
                                   │
                          ┌────────▼────────┐
                          │     ESP32       │
                          │    WiFi AP      │
                          └─────────────────┘
```

---

## Instalación

```powershell
# Crear entorno virtual
python -m venv .venv

# Activar entorno virtual en Windows
.venv\Scripts\activate.ps1

# Instalar dependencias
pip install -r requirements.txt
```

---

## Dependencias

```txt
fastapi
uvicorn
websockets
pydantic
jinja2
python-multipart
```

---

## Ejecución

```powershell
# Iniciar servidor
.venv\Scripts\python.exe main.py
```

El servidor iniciará en:

```txt
http://localhost:8000
```

---

## Uso del Sistema

1. **Seleccionar inicio**: hacer click en la cuadrícula para definir el punto inicial.
2. **Seleccionar fin**: hacer click en otra celda para definir el destino.
3. **Colocar obstáculos**: usar click o arrastrar en modo pared.
4. **Calcular ruta**: presionar el botón `CALCULAR RUTA`.
5. **Ver grafo**: expandir el modal con `VER GRAFO COMPLETO`.
6. **Enviar al carro**: presionar `ENVIAR AL CARRO`.

---

## Panel de Controles

| Control | Descripción |
|--------|-------------|
| **Inicio** | Define el punto de partida |
| **Fin** | Define el destino |
| **Obstáculo** | Marca paredes o bloqueos |
| **Borrar** | Limpia celdas individuales |
| **Ejemplo** | Genera un escenario aleatorio |
| **Limpiar Todo** | Reinicia toda la cuadrícula |

---

## Integración con ESP32

### Conexión WebSocket

El ESP32 se conecta como cliente WebSocket al servidor:

```txt
ws://192.168.4.2:8000/ws/car
```

Esta IP corresponde al servidor cuando el ESP32 opera en modo **Access Point**.

---

## Comandos Soportados

| Comando | Descripción |
|--------|-------------|
| `ADELANTE` | Avanzar |
| `ATRAS` | Retroceder |
| `STOP` | Detener motores |
| `GIRAR_NORTE` | Girar hacia el norte |
| `GIRAR_ESTE` | Girar hacia el este |
| `GIRAR_SUR` | Girar hacia el sur |
| `GIRAR_OESTE` | Girar hacia el oeste |

---

## Hardware ESP32

| Componente | Descripción |
|-----------|-------------|
| **WiFi** | Modo AP con red `CARRO_ESP` |
| **IP del ESP32** | `192.168.4.1` |
| **Motor Driver** | L298N con 2 canales PWM |
| **Motores** | 2 motores DC |
| **Giroscopio** | MPU6050 opcional para odometría |

---

## Testing

```powershell
# Ejecutar todas las pruebas
.venv\Scripts\python.exe -m unittest test_main -v
```

---

## Cobertura de Pruebas

- El algoritmo A* encuentra la ruta óptima en una grilla con obstáculos.
- El frontend renderiza correctamente la cuadrícula, costos y grafo D3.
- Los botones y paneles mantienen la estructura esperada.
- El estilo tipo Arduino IDE se aplica correctamente.
- Las celdas de inicio y fin mantienen colores fijos.

---

## Restricciones Clave

- **Offline**: todos los assets deben ser locales. No usar CDNs.
- **Sin internet**: el ESP32 opera en modo AP sin acceso a internet.
- **WebSocket**: el ESP32 se conecta al servidor, no al revés.
- **Costos**: las celdas tienen costos aleatorios entre `1` y `9`.
- **Giroscopio**: el motor de dirección no se auto-centra; se debe usar pulso en dirección opuesta.

---

## Estructura General del Proyecto

```txt
CarroESP-IA-Web/
│
├── main.py
├── requirements.txt
├── test_main.py
│
├── static/
│   ├── css/
│   ├── js/
│   └── assets/
│
├── templates/
│   └── index.html
│
└── README.md
```

---

## Licencia

Este proyecto está bajo la licencia **MIT**.

---

Desarrollado como proyecto académico para el control de un carro robótico con **ESP32**, **WebSocket** y algoritmo `A*`.
