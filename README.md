# CarroESP IA Web

![Python](https://img.shields.io/badge/Python-3776AB?style=for-the-badge&logo=python&logoColor=white)
![FastAPI](https://img.shields.io/badge/FastAPI-009688?style=for-the-badge&logo=fastapi&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black)
![D3.js](https://img.shields.io/badge/D3.js-F9A03C?style=for-the-badge&logo=d3dotjs&logoColor=white)
![ESP32](https://img.shields.io/badge/ESP32-000000?style=for-the-badge&logo=espressif&logoColor=white)
![WebSocket](https://img.shields.io/badge/WebSocket-35495E?style=for-the-badge&logo=socketdotio&logoColor=white)

Interfaz web para controlar un carro robótico mediante el algoritmo `A*`, permitiendo calcular rutas sobre una cuadrícula interactiva, control manual en tiempo real, y enviar comandos a un **ESP32** por medio de **WebSocket**.

---

## Características

- **Cuadrícula 5×5** interactiva con celdas de costo aleatorio (1-20).
- **Algoritmo `A*`** con costos ponderados y heurística Manhattan.
- **Visualización del grafo** del árbol de búsqueda usando D3.js.
- **Control manual** (`/control`) con D-pad: envía lotes de comandos mientras se mantiene pulsado para movimiento fluido.
- **Envío de comandos al ESP32** por WebSocket.
- Comandos soportados: `ADELANTE`, `ATRAS`, `STOP`, `IZQUIERDA`, `DERECHA`, `GIRAR_*`.
- **Funcionamiento offline**, usando assets locales sin CDNs.
- **Estilo Arduino IDE**, con tema oscuro y acentos turquesa.
- Pruebas unitarias para backend e interfaz.

---

## Tecnologías Usadas

| Componente | Tecnología | Propósito |
|-----------|------------|-----------|
| **Backend** | ![FastAPI](https://img.shields.io/badge/FastAPI-009688?style=flat-square&logo=fastapi&logoColor=white) ![Python](https://img.shields.io/badge/Python-3776AB?style=flat-square&logo=python&logoColor=white) | API REST y WebSocket para comunicación con ESP32/ESP8266 |
| **Frontend** | ![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=flat-square&logo=html5&logoColor=white) ![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=flat-square&logo=css&logoColor=white) ![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=flat-square&logo=javascript&logoColor=black) | Interfaz web responsiva (3 páginas) |
| **Visualización** | ![D3.js](https://img.shields.io/badge/D3.js-F9A03C?style=flat-square&logo=d3dotjs&logoColor=white) | Grafo del árbol de búsqueda A* |
| **Control manual** | ![Gamepad](https://img.shields.io/badge/D--Pad-Manual_Control-9cf?style=flat-square) | Controles direccionales en tiempo real |
| **Motor físico** | ![ESP32](https://img.shields.io/badge/ESP32-000000?style=flat-square&logo=espressif&logoColor=white) ![ESP8266](https://img.shields.io/badge/ESP8266-000000?style=flat-square&logo=espressif&logoColor=white) | Control de 2 motores DC (diferencial) |
| **Testing** | ![unittest](https://img.shields.io/badge/unittest-Python-blue?style=flat-square) | Pruebas unitarias del backend y UI |
| **Offline** | ![Offline](https://img.shields.io/badge/Offline-Assets_Locales-success?style=flat-square) | Sin dependencias externas ni CDNs |

---

## Diagrama de Arquitectura

```txt
┌─────────────────┐      ┌─────────────────┐
│   Navegador     │──────│    FastAPI      │
│ index.html      │◄─────│    Python       │
│ linea.html      │      │                 │
│ control.html    │      │                 │
└─────────────────┘      └────────┬────────┘
                                    │
                           ┌────────┴────────┐
                           │   WebSocket     │
                           │    /ws/car      │
                           └────────┬────────┘
                                    │
                           ┌────────▼────────┐
                           │   ESP32/ESP8266 │
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
7. **Control manual**: ir a `/control` para operar el carro con los controles direccionales (ratón, tacto o teclado).

---

## Paneles de Control

### Cuadrícula A* (`index.html`)

| Control | Descripción |
|--------|-------------|
| **Inicio** | Define el punto de partida |
| **Fin** | Define el destino |
| **Obstáculo** | Marca paredes o bloqueos |
| **Borrar** | Limpia celdas individuales |
| **Ejemplo** | Genera un escenario aleatorio |
| **Limpiar Todo** | Reinicia toda la cuadrícula |

### Control Manual (`control.html`)

| Control | Descripción |
|--------|-------------|
| **▲ ADELANTE** | Primera pulsación = 1 comando; mantener = lotes de 2 cada 1.2 s |
| **▼ ATRAS** | Primera pulsación = 1 comando; mantener = lotes de 2 cada 1.2 s |
| **◄ IZQUIERDA** | Gira 90° a la izquierda |
| **► DERECHA** | Gira 90° a la derecha |
| **■ STOP** | Detiene inmediatamente |
| **Teclado** | Flechas + Espacio + Escape |

---

## Integración con ESP32 / ESP8266

### Conexión WebSocket

El ESP se conecta como cliente WebSocket al servidor:

```txt
ws://192.168.4.2:8000/ws/car
```

Esta IP corresponde al servidor cuando el ESP opera en modo **Access Point**.

### Telemetría

El ESP envía datos del giroscopio cada 1000 ms vía WebSocket. El servidor los retransmite a los clientes frontend conectados a `/ws/telemetry` y los guarda en `gyro_telemetry.log`.

---

## Comandos Soportados

| Comando | Descripción |
|--------|-------------|
| `ADELANTE` | Avanzar |
| `ATRAS` | Retroceder |
| `STOP` | Detener motores |
| `IZQUIERDA` | Girar 90° a la izquierda (ESP8266) |
| `DERECHA` | Girar 90° a la derecha (ESP8266) |
| `GIRAR_NORTE` | Girar hacia el norte (ESP32) |
| `GIRAR_ESTE` | Girar hacia el este (ESP32) |
| `GIRAR_SUR` | Girar hacia el sur (ESP32) |
| `GIRAR_OESTE` | Girar hacia el oeste (ESP32) |

---

## Hardware

### ESP32

| Componente | Descripción |
|-----------|-------------|
| **WiFi** | Modo AP con red `CarroESP32_ARTYOM` |
| **IP del ESP32** | `192.168.4.1` |
| **Motor Driver** | L298N con 2 canales PWM |
| **Motores** | 2 motores DC (diferencial) |
| **Giroscopio** | MPU6050 para odometría y control de giros |
| **I2C** | SDA=GPIO21, SCL=GPIO22 |

### ESP8266

| Componente | Descripción |
|-----------|-------------|
| **WiFi** | Modo AP con red `CarroESP8266_ARTYOM` |
| **IP del ESP8266** | `192.168.4.1` |
| **Motor Driver** | L298N con 2 canales PWM |
| **Motores** | 2 motores DC (diferencial) |
| **Giroscopio** | MPU6050 solo para telemetría (giros por delay fijo) |
| **I2C** | SDA=GPIO4 (D2), SCL=GPIO5 (D1) |

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
- La página `/control` carga correctamente y expone los controles manuales.

---

## Restricciones Clave

- **Offline**: todos los assets deben ser locales. No usar CDNs.
- **Sin internet**: el ESP32 opera en modo AP sin acceso a internet.
- **WebSocket**: el ESP32 se conecta al servidor, no al revés.
- **Costos**: las celdas tienen costos aleatorios entre `1` y `20`.
- **Giroscopio**: el motor de dirección no se auto-centra; se debe usar pulso en dirección opuesta.

---

## Estructura General del Proyecto

```txt
CarroESP-IA-Web/
│
├── main.py                  # FastAPI backend + A* + WebSocket
├── requirements.txt
├── test_main.py
├── AGENTS.md                # Documentación para agentes de código
│
├── static/
│   └── d3.min.js            # D3.js v7 (local)
│
├── index.html               # Cuadrícula 5×5 con A*
├── linea.html               # Seguidor de línea (grafo fijo)
├── control.html             # Control manual del carro
│
├── esp32_carro/
│   └── esp32_carro.ino      # Sketch ESP32 (giroscopio + motores)
├── esp8266_carro/
│   └── esp8266_carro.ino    # Sketch ESP8266 (2 motores diferenciales)
│
└── README.md
```

Desarrollado como proyecto académico para el control de un carro robótico con **ESP32**, **WebSocket** y algoritmo `A*`.
