#!/usr/bin/env python3
"""
Scheduling Ships GUI - Visualizador de un solo hilo.

Regla de defensa:
- Este archivo NO calendariza barcos.
- Este archivo NO mueve barcos por cuenta propia.
- Este archivo NO decide prioridades, RR, STRN, Equidad, Letrero ni Tico.
- Solo lee líneas que imprime el programa C/ESP32 y dibuja el estado observado.
- No usa threading. Todo se hace con tkinter.after(...).

Uso recomendado:
    python3 gui.py --port /dev/ttyUSB0 --baud 115200

Uso para revisar un log guardado:
    python3 gui.py --input-log salida_monitor.txt
"""

from __future__ import annotations

import argparse
import json
import queue
import re
import sys
import time
import tkinter as tk
from tkinter import ttk
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any


try:
    import serial  # type: ignore
except Exception:  # pragma: no cover
    serial = None


# -----------------------------------------------------------------------------
# Modelo visual: solo representa el ultimo estado recibido desde el ESP32/log.
# -----------------------------------------------------------------------------


@dataclass
class BoatView:
    label: str
    name: str = ""
    boat_type: str = "Normal"
    remaining: Optional[int] = None
    priority: Optional[int] = None
    deadline: Optional[int] = None


@dataclass
class GUIState:
    scheduler: str = "Sin seleccionar"
    flow: str = "Sin seleccionar"
    direction: str = "L2R"
    channel_length: int = 10
    channel: List[str] = field(default_factory=lambda: [""] * 10)
    left_queue: List[BoatView] = field(default_factory=list)
    right_queue: List[BoatView] = field(default_factory=list)
    gates_down: bool = False
    interrupt_active: bool = False
    status: str = "Esperando datos del ESP32..."
    last_event: str = ""
    connected: bool = False
    source: str = "sin fuente"
    warning: str = ""


# -----------------------------------------------------------------------------
# Parser de salida del proyecto C.
# Acepta dos formatos:
# 1) Formato recomendado futuro: GUI_STATE { ...json... }
# 2) Logs actuales: [CANAL] [L1][  ]..., Cola izquierda..., etc.
# -----------------------------------------------------------------------------


class ProjectLogParser:
    QUEUE_SHIP_RE = re.compile(
        r"(?P<name>[A-Za-z0-9_]+)"
        r"\(id=(?P<id>\d+),\s*tipo=(?P<type>[^,]+),\s*restante=(?P<remaining>-?\d+),"
        r"\s*prioridad=(?P<priority>-?\d+),\s*deadline=(?P<deadline>-?\d+)\)"
    )

    def __init__(self, state: GUIState):
        self.state = state
        # Registro visual: el log del canal normalmente muestra solo etiquetas cortas
        # como L1/R3. Guardamos el tipo a partir de las lineas de cola/creacion
        # para poder dibujar la forma correcta sin que Python decida scheduling.
        self.boat_types_by_label: Dict[str, str] = {}
        self.boat_names_by_label: Dict[str, str] = {}
        self.pending_created_name: Optional[str] = None

    def parse_line(self, raw_line: str) -> None:
        line = raw_line.strip("\r\n")
        if not line:
            return

        if line.startswith("GUI_STATE"):
            self._parse_json_state(line)
            return

        self._parse_text_log(line)

    def _queue_for_label(self, label_or_name: str) -> Optional[List[BoatView]]:
        label = self.short_label(label_or_name)
        if label.startswith("L"):
            return self.state.left_queue
        if label.startswith("R"):
            return self.state.right_queue
        return None

    def _remove_from_queues(self, label_or_name: str) -> None:
        label = self.short_label(label_or_name)
        if not label:
            return

        def keep(boat: BoatView) -> bool:
            return boat.label != label

        self.state.left_queue = [boat for boat in self.state.left_queue if keep(boat)]
        self.state.right_queue = [boat for boat in self.state.right_queue if keep(boat)]

    def _remove_active_channel_boats_from_queues(self) -> None:
        for label in self.state.channel:
            if label:
                self._remove_from_queues(label)

    def _add_to_queue_if_missing(self, label_or_name: str, front: bool = False) -> None:
        label = self.short_label(label_or_name)
        if not label:
            return

        target_queue = self._queue_for_label(label)
        if target_queue is None:
            return

        # Evita duplicados visuales. La cola real sigue estando en C/FreeRTOS;
        # aquí solo reflejamos eventos impresos por el programa.
        if any(boat.label == label for boat in target_queue):
            return

        boat = self.resolve_boat(label_or_name)
        if front:
            target_queue.insert(0, boat)
        else:
            target_queue.append(boat)

    @staticmethod
    def _extract_ship_before(text: str, token: str) -> str:
        try:
            before = text.split(token, 1)[0]
        except Exception:
            return ""
        # Quita prefijos tipo [CANAL] o [CALENDARIZADOR]
        before = ProjectLogParser._clean_prefix(before)
        return before.strip().split()[-1] if before.strip() else ""

    def _parse_json_state(self, line: str) -> None:
        try:
            payload = line.split("GUI_STATE", 1)[1].strip()
            data = json.loads(payload)
        except Exception as exc:
            self.state.warning = f"No se pudo parsear GUI_STATE: {exc}"
            return

        self.state.scheduler = str(data.get("scheduler", self.state.scheduler))
        self.state.flow = str(data.get("flow", self.state.flow))
        self.state.direction = str(data.get("direction", self.state.direction))
        self.state.channel_length = int(data.get("channel_length", self.state.channel_length))

        channel = data.get("channel")
        if isinstance(channel, list):
            # Acepta ["", "L1", ...] o [{"label":"L1","pos":1}, ...]
            cells = [""] * max(1, self.state.channel_length)
            for i, item in enumerate(channel):
                if isinstance(item, str) and i < len(cells):
                    cells[i] = item
                elif isinstance(item, dict):
                    pos = int(item.get("pos", -1))
                    name = str(item.get("name", item.get("label", "")))
                    label = str(item.get("label", self.short_label(name)))
                    boat_type = str(item.get("type", self.infer_type(name)))
                    self._register_boat(name or label, boat_type)
                    if 0 <= pos < len(cells):
                        cells[pos] = label
            self.state.channel = cells

        self.state.left_queue = self._json_queue_to_boats(data.get("left_queue", []))
        self.state.right_queue = self._json_queue_to_boats(data.get("right_queue", []))
        self.state.gates_down = bool(data.get("gates_down", self.state.gates_down))
        self.state.interrupt_active = bool(data.get("interrupt", self.state.interrupt_active))
        self.state.status = str(data.get("status", self.state.status))

    def _json_queue_to_boats(self, value: Any) -> List[BoatView]:
        boats: List[BoatView] = []
        if not isinstance(value, list):
            return boats

        for item in value:
            if isinstance(item, str):
                boat = self.resolve_boat(item)
                boats.append(boat)
            elif isinstance(item, dict):
                name = str(item.get("name", item.get("label", "")))
                label = str(item.get("label", self.short_label(name)))
                boat_type = str(item.get("type", self.infer_type(name)))
                self._register_boat(name or label, boat_type)
                boats.append(
                    BoatView(
                        label=label,
                        name=name,
                        boat_type=boat_type,
                        remaining=item.get("remaining"),
                        priority=item.get("priority"),
                        deadline=item.get("deadline"),
                    )
                )

        return boats

    def _parse_text_log(self, line: str) -> None:
        if "[CREADA] Task real para barco" in line:
            self.pending_created_name = line.rsplit(" ", 1)[-1].strip()
            return

        if self.pending_created_name and line.startswith("ID:") and "Tipo:" in line:
            match = re.search(r"Tipo:\s*([^|]+)", line)
            if match:
                self._register_boat(self.pending_created_name, match.group(1).strip())
            self.pending_created_name = None
            return

        if "[SCHEDULER] Calendarizador seleccionado:" in line:
            self.state.scheduler = line.split("Calendarizador seleccionado:", 1)[1].strip()
            self.state.status = f"Scheduler activo: {self.state.scheduler}"
            return

        if "[CANAL] Iniciando control de flujo:" in line:
            self.state.flow = line.split("control de flujo:", 1)[1].strip()
            self.state.status = f"Control de flujo activo: {self.state.flow}"
            return

        if line.startswith("Cola izquierda"):
            self.state.left_queue = self._parse_queue_line(line)
            return

        if line.startswith("Cola derecha"):
            self.state.right_queue = self._parse_queue_line(line)
            return

        if "[CANAL]" in line and "][" in line:
            parsed = self._parse_channel_cells(line)
            if parsed is not None:
                self.state.channel = parsed
                self.state.channel_length = len(parsed)
                self._remove_active_channel_boats_from_queues()
                self._validate_channel()
            return

        if "retoma el canal" in line:
            ship_name = self._extract_ship_before(line, " retoma el canal")
            self._remove_from_queues(ship_name)
            self.state.status = self._clean_prefix(line)
            self._parse_direction_from_line(line)
            return

        if "entra al canal desde la Izquierda" in line:
            ship_name = self._extract_ship_before(line, " entra al canal")
            self._remove_from_queues(ship_name)
            self.state.direction = "L2R"
            self.state.status = self._clean_prefix(line)
            return

        if "entra al canal desde la Derecha" in line:
            ship_name = self._extract_ship_before(line, " entra al canal")
            self._remove_from_queues(ship_name)
            self.state.direction = "R2L"
            self.state.status = self._clean_prefix(line)
            return

        if "El letrero cambió. Nuevo sentido: Izquierda" in line:
            self.state.direction = "L2R"
            self.state.status = "Letrero: pasan barcos del lado izquierdo"
            return

        if "El letrero cambió. Nuevo sentido: Derecha" in line:
            self.state.direction = "R2L"
            self.state.status = "Letrero: pasan barcos del lado derecho"
            return

        if "sale temporalmente del canal y vuelve a cola" in line:
            ship_name = self._extract_ship_before(line, " sale temporalmente")
            self._add_to_queue_if_missing(ship_name)
            self.state.last_event = self._clean_prefix(line)
            self.state.status = self.state.last_event
            return

        if "salió del canal por el extremo contrario" in line or "salio del canal por el extremo contrario" in line:
            ship_name = self._extract_ship_before(line, " sal")
            self._remove_from_queues(ship_name)
            self.state.status = self._clean_prefix(line)
            return

        if "Bajando agujas" in line:
            self.state.gates_down = True
            self.state.interrupt_active = True
            self.state.status = "Interrupción: agujas abajo, canal protegido"
            return

        if "Levantando agujas" in line:
            self.state.gates_down = False
            self.state.interrupt_active = False
            self.state.status = "Interrupción resuelta: agujas arriba"
            return

        if "[SENSOR]" in line or "[INTERRUPCION]" in line:
            self.state.last_event = self._clean_prefix(line)
            self.state.status = self.state.last_event
            return

        if "agotó su quantum" in line:
            ship_name = self._extract_ship_before(line, " agotó su quantum")
            self._add_to_queue_if_missing(ship_name)
            self.state.last_event = self._clean_prefix(line)
            return

        if "no avanza: posicion" in line:
            self.state.last_event = self._clean_prefix(line)
            return

        if "Todas las colas estan vacias" in line or "Todas las colas están vacías" in line:
            self.state.left_queue = []
            self.state.right_queue = []
            self.state.channel = [""] * max(1, self.state.channel_length)
            self.state.status = "Todas las colas vacías. Canal inactivo."
            return

        if "Calendarizacion terminada" in line:
            self.state.status = "Calendarización terminada"
            return

    def _parse_queue_line(self, line: str) -> List[BoatView]:
        boats: List[BoatView] = []
        for match in self.QUEUE_SHIP_RE.finditer(line):
            name = match.group("name")
            boat_type = match.group("type").strip()
            self._register_boat(name, boat_type)
            boats.append(
                BoatView(
                    label=self.short_label(name),
                    name=name,
                    boat_type=boat_type,
                    remaining=int(match.group("remaining")),
                    priority=int(match.group("priority")),
                    deadline=int(match.group("deadline")),
                )
            )
        return boats

    def _parse_channel_cells(self, line: str) -> Optional[List[str]]:
        if "[CANAL]" not in line:
            return None

        rest = line.split("[CANAL]", 1)[1].strip()
        cells = re.findall(r"\[([^\]]*)\]", rest)

        if not cells:
            return None

        # Filtra falsos positivos: una representacion real del canal tiene varias celdas.
        if len(cells) < 3:
            return None

        return [cell.strip() for cell in cells]

    def _parse_direction_from_line(self, line: str) -> None:
        if "Izquierda -> Derecha" in line:
            self.state.direction = "L2R"
        elif "Derecha -> Izquierda" in line:
            self.state.direction = "R2L"

    def _validate_channel(self) -> None:
        occupied = [c for c in self.state.channel if c]
        if len(occupied) != len(set(occupied)):
            self.state.warning = "Advertencia: un barco aparece repetido en el canal."
        else:
            self.state.warning = ""

    def _register_boat(self, name: str, boat_type: str) -> None:
        label = self.short_label(name)
        if not label:
            return
        self.boat_types_by_label[label] = boat_type
        self.boat_names_by_label[label] = name

    def resolve_boat(self, label_or_name: str) -> BoatView:
        label = self.short_label(label_or_name)
        boat_type = self.boat_types_by_label.get(label, self.infer_type(label_or_name))
        name = self.boat_names_by_label.get(label, label_or_name)
        return BoatView(label=label, name=name, boat_type=boat_type)

    @staticmethod
    def _clean_prefix(line: str) -> str:
        for prefix in ("[CANAL]", "[SENSOR]", "[INTERRUPCION]", "[CALENDARIZADOR]", "[TECLADO]"):
            line = line.replace(prefix, "").strip()
        return line

    @staticmethod
    def infer_type(name: str) -> str:
        if "Patrulla" in name:
            return "Patrulla"
        if "Pesquera" in name:
            return "Pesquera"
        return "Normal"

    @staticmethod
    def short_label(name: str) -> str:
        if not name:
            return ""
        return name.split("_", 1)[0]


# -----------------------------------------------------------------------------
# Fuentes de entrada: serial no bloqueante o archivo de log.
# -----------------------------------------------------------------------------


class SerialSource:
    def __init__(self, port: str, baud: int):
        if serial is None:
            raise RuntimeError("pyserial no está instalado. Use: pip install pyserial")

        self.port = port
        self.baud = baud
        self.ser = serial.Serial(port, baudrate=baud, timeout=0)
        self.buffer = bytearray()

    def read_lines(self) -> List[str]:
        available = self.ser.in_waiting
        if available <= 0:
            return []

        chunk = self.ser.read(available)
        if chunk:
            self.buffer.extend(chunk)

        lines: List[str] = []
        while b"\n" in self.buffer:
            raw, _, rest = self.buffer.partition(b"\n")
            self.buffer = bytearray(rest)
            lines.append(raw.decode("utf-8", errors="replace").rstrip("\r"))

        return lines

    def send_key(self, key: str) -> None:
        if not key:
            return
        self.ser.write(key[0].encode("utf-8"))

    def close(self) -> None:
        self.ser.close()


class LogFileSource:
    def __init__(self, path: str, lines_per_tick: int = 1):
        self.path = path
        self.lines_per_tick = max(1, lines_per_tick)
        self.file = open(path, "r", encoding="utf-8", errors="replace")

    def read_lines(self) -> List[str]:
        lines = []
        for _ in range(self.lines_per_tick):
            line = self.file.readline()
            if not line:
                break
            lines.append(line.rstrip("\n"))
        return lines

    def send_key(self, key: str) -> None:
        # Un archivo grabado no recibe entradas.
        return

    def close(self) -> None:
        self.file.close()


# -----------------------------------------------------------------------------
# Widgets: colas con scrollbar.
# -----------------------------------------------------------------------------


class QueuePanel(ttk.Frame):
    def __init__(self, parent: tk.Widget, title: str):
        super().__init__(parent, padding=8)
        self.title = title
        self._last_signature: Optional[tuple] = None

        title_label = ttk.Label(self, text=title, style="PanelTitle.TLabel")
        title_label.pack(anchor="center", pady=(0, 6))

        self.count_label = ttk.Label(self, text="0 barcos", style="Small.TLabel")
        self.count_label.pack(anchor="center", pady=(0, 6))

        self.canvas = tk.Canvas(self, width=250, height=470, bg="#ffffff", highlightthickness=1, highlightbackground="#cfd8dc")
        self.scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.canvas.yview)
        self.inner = ttk.Frame(self.canvas)

        self.inner.bind(
            "<Configure>",
            lambda _event: self.canvas.configure(scrollregion=self.canvas.bbox("all"))
        )

        self.canvas_window = self.canvas.create_window((0, 0), window=self.inner, anchor="nw")
        self.canvas.configure(yscrollcommand=self.scrollbar.set)

        self.canvas.pack(side="left", fill="both", expand=True)
        self.scrollbar.pack(side="right", fill="y")

        self.canvas.bind("<Configure>", self._on_canvas_configure)

    def _on_canvas_configure(self, event: tk.Event) -> None:
        self.canvas.itemconfig(self.canvas_window, width=event.width)

    def update_boats(self, boats: List[BoatView]) -> None:
        signature = tuple(
            (boat.label, boat.name, boat.boat_type, boat.remaining, boat.priority, boat.deadline)
            for boat in boats
        )
        if signature == self._last_signature:
            return
        self._last_signature = signature

        for child in self.inner.winfo_children():
            child.destroy()

        self.count_label.configure(text=f"{len(boats)} barcos en cola")

        if not boats:
            empty = ttk.Label(self.inner, text="Cola vacía", style="Empty.TLabel")
            empty.pack(fill="x", padx=10, pady=30)
            return

        for index, boat in enumerate(boats, start=1):
            card = ttk.Frame(self.inner, style="Card.TFrame", padding=8)
            card.pack(fill="x", padx=8, pady=5)

            top = ttk.Frame(card, style="Card.TFrame")
            top.pack(fill="x")

            badge = tk.Canvas(top, width=34, height=34, bg="#ffffff", highlightthickness=0)
            badge.pack(side="left", padx=(0, 8))
            draw_boat_icon(badge, 17, 17, boat, size=12)

            text_frame = ttk.Frame(top, style="Card.TFrame")
            text_frame.pack(side="left", fill="x", expand=True)

            ttk.Label(
                text_frame,
                text=f"{index}. {boat.label}",
                style="BoatName.TLabel"
            ).pack(anchor="w")

            ttk.Label(
                text_frame,
                text=boat.name or boat.boat_type,
                style="Small.TLabel"
            ).pack(anchor="w")

            meta_parts = []
            if boat.remaining is not None:
                meta_parts.append(f"rest={boat.remaining}")
            if boat.priority is not None:
                meta_parts.append(f"prio={boat.priority}")
            if boat.deadline is not None:
                meta_parts.append(f"dl={boat.deadline}")

            if meta_parts:
                ttk.Label(
                    card,
                    text=" | ".join(meta_parts),
                    style="Meta.TLabel"
                ).pack(anchor="w", pady=(5, 0))


# -----------------------------------------------------------------------------
# GUI principal.
# -----------------------------------------------------------------------------


class SchedulingShipsGUI:
    POLL_MS = 40

    def __init__(self, root: tk.Tk, source: Optional[Any], state: GUIState):
        self.root = root
        self.source = source
        self.state = state
        self.parser = ProjectLogParser(self.state)
        self.log_lines: List[str] = []

        self.root.title("Scheduling Ships - Visualizador")
        self.root.geometry("1360x820")
        self.root.minsize(1180, 720)

        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.bind("w", lambda _e: self.send_key_and_close("w"))
        self.root.bind("W", lambda _e: self.send_key_and_close("w"))

        for key in ("1", "2", "3", "4", "5", "6", "p", "P"):
            self.root.bind(key, lambda _e, k=key: self.send_key(k))

        self._configure_styles()
        self._build_layout()
        self._refresh()
        self._poll()

    def _configure_styles(self) -> None:
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except Exception:
            pass

        style.configure("Root.TFrame", background="#edf3f6")
        style.configure("Header.TFrame", background="#1f3038")
        style.configure("HeaderTitle.TLabel", background="#1f3038", foreground="white", font=("Arial", 22, "bold"))
        style.configure("HeaderSub.TLabel", background="#1f3038", foreground="#cfd8dc", font=("Arial", 10))
        style.configure("Panel.TFrame", background="#ffffff", relief="solid", borderwidth=1)
        style.configure("PanelTitle.TLabel", background="#ffffff", foreground="#1f3038", font=("Arial", 14, "bold"))
        style.configure("Small.TLabel", background="#ffffff", foreground="#607d8b", font=("Arial", 9))
        style.configure("Empty.TLabel", background="#ffffff", foreground="#78909c", font=("Arial", 12, "italic"))
        style.configure("Card.TFrame", background="#ffffff", relief="solid", borderwidth=1)
        style.configure("BoatName.TLabel", background="#ffffff", foreground="#1f3038", font=("Arial", 11, "bold"))
        style.configure("Meta.TLabel", background="#ffffff", foreground="#455a64", font=("Consolas", 8))
        style.configure("Status.TFrame", background="#1f3038")
        style.configure("Status.TLabel", background="#1f3038", foreground="white", font=("Arial", 10))
        style.configure("Warning.TLabel", background="#1f3038", foreground="#ffcc80", font=("Arial", 10, "bold"))

    def _build_layout(self) -> None:
        root_frame = ttk.Frame(self.root, style="Root.TFrame")
        root_frame.pack(fill="both", expand=True)

        header = ttk.Frame(root_frame, style="Header.TFrame", padding=(18, 12))
        header.pack(fill="x")

        self.title_label = ttk.Label(header, text="Scheduling Ships", style="HeaderTitle.TLabel")
        self.title_label.pack(side="left")

        self.header_info = ttk.Label(header, text="", style="HeaderSub.TLabel", justify="right")
        self.header_info.pack(side="right")

        content = ttk.Frame(root_frame, style="Root.TFrame", padding=12)
        content.pack(fill="both", expand=True)

        content.columnconfigure(0, weight=0)
        content.columnconfigure(1, weight=1)
        content.columnconfigure(2, weight=0)
        content.rowconfigure(0, weight=1)

        self.left_panel = QueuePanel(content, "Cola izquierda")
        self.left_panel.grid(row=0, column=0, sticky="ns", padx=(0, 10))

        center = ttk.Frame(content, style="Root.TFrame")
        center.grid(row=0, column=1, sticky="nsew")
        center.rowconfigure(1, weight=1)
        center.columnconfigure(0, weight=1)

        self.sign_canvas = tk.Canvas(center, height=95, bg="#ffffff", highlightthickness=1, highlightbackground="#cfd8dc")
        self.sign_canvas.grid(row=0, column=0, sticky="ew", pady=(0, 10))

        self.channel_canvas = tk.Canvas(center, bg="#e7f4fb", highlightthickness=1, highlightbackground="#90a4ae")
        self.channel_canvas.grid(row=1, column=0, sticky="nsew")

        bottom = ttk.Frame(center, style="Root.TFrame")
        bottom.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        bottom.columnconfigure(0, weight=1)
        bottom.columnconfigure(1, weight=1)

        self.event_panel = tk.Text(
            bottom,
            height=8,
            bg="#ffffff",
            fg="#263238",
            font=("Consolas", 9),
            wrap="word"
        )
        self.event_panel.grid(row=0, column=0, sticky="nsew", padx=(0, 5))

        controls = ttk.Frame(bottom, style="Panel.TFrame", padding=10)
        controls.grid(row=0, column=1, sticky="nsew", padx=(5, 0))

        ttk.Label(controls, text="Entradas hacia ESP32", style="PanelTitle.TLabel").pack(anchor="w")
        ttk.Label(
            controls,
            text="Estas teclas solo se envían al programa C. Python no crea barcos ni decide movimiento.",
            style="Small.TLabel",
            wraplength=360,
        ).pack(anchor="w", pady=(0, 8))

        grid = ttk.Frame(controls, style="Panel.TFrame")
        grid.pack(anchor="w")

        buttons = [
            ("1 Normal-Izq", "1"), ("2 Pesquera-Izq", "2"), ("3 Patrulla-Izq", "3"),
            ("4 Normal-Der", "4"), ("5 Pesquera-Der", "5"), ("6 Patrulla-Der", "6"),
            ("P Sensor", "P"), ("W Salir", "w"),
        ]

        for i, (label, key) in enumerate(buttons):
            ttk.Button(grid, text=label, command=lambda k=key: self.send_key_and_close(k) if k == "w" else self.send_key(k)).grid(
                row=i // 2,
                column=i % 2,
                padx=4,
                pady=4,
                sticky="ew"
            )

        self.right_panel = QueuePanel(content, "Cola derecha")
        self.right_panel.grid(row=0, column=2, sticky="ns", padx=(10, 0))

        status = ttk.Frame(root_frame, style="Status.TFrame", padding=(16, 8))
        status.pack(fill="x")

        self.status_label = ttk.Label(status, text="", style="Status.TLabel")
        self.status_label.pack(side="left", fill="x", expand=True)

        self.warning_label = ttk.Label(status, text="", style="Warning.TLabel")
        self.warning_label.pack(side="right")

    def _poll(self) -> None:
        if self.source is not None:
            try:
                for line in self.source.read_lines():
                    self._append_log(line)
                    self.parser.parse_line(line)
            except Exception as exc:
                self.state.warning = f"Error leyendo fuente: {exc}"

        self._refresh()
        self.root.after(self.POLL_MS, self._poll)

    def _append_log(self, line: str) -> None:
        if not line:
            return

        self.log_lines.append(line)
        if len(self.log_lines) > 150:
            self.log_lines = self.log_lines[-150:]

        self.event_panel.configure(state="normal")
        self.event_panel.insert("end", line + "\n")
        self.event_panel.see("end")
        self.event_panel.configure(state="disabled")

    def _refresh(self) -> None:
        self.header_info.configure(
            text=(
                f"Scheduler: {self.state.scheduler}    |    Flujo: {self.state.flow}\n"
                f"Fuente: {self.state.source}    |    Largo canal: {self.state.channel_length}"
            )
        )

        self.left_panel.update_boats(self.state.left_queue)
        self.right_panel.update_boats(self.state.right_queue)

        self._draw_sign()
        self._draw_channel()

        gates = "ABAJO" if self.state.gates_down else "ARRIBA"
        direction = "Izquierda → Derecha" if self.state.direction == "L2R" else "Derecha → Izquierda"

        self.status_label.configure(
            text=f"Estado: {self.state.status}  |  Dirección: {direction}  |  Agujas: {gates}  |  Último evento: {self.state.last_event}"
        )
        self.warning_label.configure(text=self.state.warning)

    def _draw_sign(self) -> None:
        c = self.sign_canvas
        c.delete("all")

        w = max(c.winfo_width(), 600)
        h = max(c.winfo_height(), 95)

        c.create_rectangle(0, 0, w, h, fill="#ffffff", outline="")

        if self.state.interrupt_active or self.state.gates_down:
            title = "CANAL PROTEGIDO POR SENSOR"
            arrow = "AGUJAS ABAJO"
            color = "#c62828"
        elif self.state.direction == "L2R":
            title = "LETRERO: pasan barcos del lado IZQUIERDO"
            arrow = "IZQUIERDA  →  DERECHA"
            color = "#1565c0"
        else:
            title = "LETRERO: pasan barcos del lado DERECHO"
            arrow = "DERECHA  →  IZQUIERDA"
            color = "#1565c0"

        c.create_text(w / 2, 28, text=title, font=("Arial", 14, "bold"), fill="#1f3038")
        c.create_text(w / 2, 62, text=arrow, font=("Arial", 20, "bold"), fill=color)

        # LED visual del letrero
        led_color = "#c62828" if self.state.gates_down else ("#2e7d32" if self.state.direction == "L2R" else "#ffb300")
        c.create_oval(w - 72, 28, w - 40, 60, fill=led_color, outline="#263238", width=2)
        c.create_text(w - 56, 73, text="LED", font=("Arial", 8, "bold"), fill="#263238")

    def _draw_channel(self) -> None:
        c = self.channel_canvas
        c.delete("all")

        w = max(c.winfo_width(), 600)
        h = max(c.winfo_height(), 350)

        margin_x = 70
        top_y = 105
        channel_h = 130
        x1 = margin_x
        x2 = w - margin_x
        y1 = top_y
        y2 = top_y + channel_h

        c.create_text(w / 2, 35, text="CANAL / CPU", font=("Arial", 22, "bold"), fill="#1f3038")

        # Océanos
        c.create_text(25, y1 + channel_h / 2, text="Océano\nIzq", font=("Arial", 11, "bold"), fill="#1565c0")
        c.create_text(w - 25, y1 + channel_h / 2, text="Océano\nDer", font=("Arial", 11, "bold"), fill="#1565c0")

        # Agujas físicas simuladas
        self._draw_gate(c, x1 - 28, y1 + channel_h / 2, side="IZQ")
        self._draw_gate(c, x2 + 28, y1 + channel_h / 2, side="DER")

        # Canal
        c.create_rectangle(x1, y1, x2, y2, fill="#bbdefb", outline="#1565c0", width=4)

        length = max(1, self.state.channel_length)
        cell_w = (x2 - x1) / length

        # Celdas reales del canal
        for i in range(length):
            cx1 = x1 + i * cell_w
            cx2 = x1 + (i + 1) * cell_w
            fill = "#e3f2fd" if i % 2 == 0 else "#d7eefc"
            c.create_rectangle(cx1, y1, cx2, y2, fill=fill, outline="#90caf9")

            # Etiquetas: si hay demasiadas posiciones, no saturar.
            if length <= 20 or i % max(1, length // 10) == 0:
                c.create_text((cx1 + cx2) / 2, y1 - 12, text=str(i), font=("Arial", 8), fill="#607d8b")

        # Oleaje suave
        for offset in range(0, int(x2 - x1), 42):
            c.create_line(x1 + offset, y1 + 28, x1 + offset + 22, y1 + 45, fill="#90caf9", width=2)
            c.create_line(x1 + offset, y2 - 28, x1 + offset + 22, y2 - 45, fill="#90caf9", width=2)

        # Barcos en canal desde el estado real.
        for i, label in enumerate(self.state.channel):
            if not label:
                continue
            cx = x1 + (i + 0.5) * cell_w
            cy = y1 + channel_h / 2

            boat = self.parser.resolve_boat(label)
            draw_boat_icon(c, cx, cy, boat, size=max(12, min(22, cell_w * 0.32)))

            # Nombre abajo si cabe.
            if cell_w >= 34:
                c.create_text(cx, cy + 32, text=label, font=("Arial", 8, "bold"), fill="#1f3038")

        # Indicador de seguridad
        occupied = len([x for x in self.state.channel if x])
        if self.state.gates_down:
            status = "AGUJAS ABAJO: canal protegido"
            color = "#c62828"
        elif occupied > 0:
            status = f"Canal ocupado: {occupied} barco(s)"
            color = "#ef6c00"
        else:
            status = "Canal libre"
            color = "#2e7d32"

        c.create_rectangle(w / 2 - 210, y2 + 35, w / 2 + 210, y2 + 88, fill="#ffffff", outline="#cfd8dc", width=2)
        c.create_text(w / 2, y2 + 62, text=status, font=("Arial", 15, "bold"), fill=color)

        # Leyenda
        legend_y = h - 70
        c.create_rectangle(w / 2 - 250, legend_y - 28, w / 2 + 250, legend_y + 35, fill="#ffffff", outline="#cfd8dc")
        c.create_text(w / 2, legend_y - 10, text="Tipos de barco", font=("Arial", 12, "bold"), fill="#1f3038")

        items = [
            BoatView(label="N", name="Normal", boat_type="Normal"),
            BoatView(label="Pq", name="Pesquera", boat_type="Pesquera"),
            BoatView(label="Pt", name="Patrulla", boat_type="Patrulla"),
        ]
        labels = ["Normal", "Pesquera", "Patrulla"]
        for idx, (boat, text) in enumerate(zip(items, labels)):
            x = w / 2 - 150 + idx * 150
            draw_boat_icon(c, x, legend_y + 16, boat, size=13)
            c.create_text(x + 42, legend_y + 16, text=text, anchor="w", font=("Arial", 9), fill="#263238")

    def _draw_gate(self, c: tk.Canvas, x: float, y: float, side: str) -> None:
        sensor_color = "#c62828" if self.state.interrupt_active else "#2e7d32"
        gate_color = "#c62828" if self.state.gates_down else "#2e7d32"

        c.create_oval(x - 8, y - 55, x + 8, y - 39, fill=sensor_color, outline="#263238")
        c.create_text(x, y - 28, text=f"Sensor {side}", font=("Arial", 7, "bold"), fill="#263238")

        c.create_line(x, y - 10, x, y + 42, fill="#263238", width=4)

        if self.state.gates_down:
            c.create_line(x - 25, y + 8, x + 25, y + 8, fill=gate_color, width=5)
            text = "BAJA"
        else:
            c.create_line(x, y + 8, x + 26, y - 20, fill=gate_color, width=5)
            text = "ALTA"

        c.create_text(x, y + 60, text=f"Aguja {side}\n{text}", font=("Arial", 7, "bold"), fill="#263238")

    def send_key(self, key: str) -> None:
        self._append_log(f"[GUI] Enviando tecla al ESP32: {key}")

        if self.source is None:
            self._append_log("[GUI] No hay fuente serial conectada; tecla ignorada.")
            return

        try:
            self.source.send_key(key)
        except Exception as exc:
            self.state.warning = f"No se pudo enviar tecla: {exc}"

    def send_key_and_close(self, key: str) -> None:
        self.send_key(key)
        self.close()

    def close(self) -> None:
        try:
            if self.source is not None:
                self.source.close()
        finally:
            self.root.destroy()


# -----------------------------------------------------------------------------
# Dibujo de barcos.
# -----------------------------------------------------------------------------


def boat_color(boat_type: str) -> str:
    if "Patrulla" in boat_type:
        return "#ef5350"
    if "Pesquera" in boat_type:
        return "#42a5f5"
    return "#b0bec5"


def draw_boat_icon(canvas: tk.Canvas, x: float, y: float, boat: BoatView, size: float = 18) -> None:
    color = boat_color(boat.boat_type)

    if "Patrulla" in boat.boat_type:
        canvas.create_polygon(
            x,
            y - size - 4,
            x + size + 5,
            y,
            x,
            y + size + 4,
            x - size - 5,
            y,
            fill=color,
            outline="#263238",
            width=2,
        )
    elif "Pesquera" in boat.boat_type:
        canvas.create_polygon(
            x,
            y - size - 4,
            x - size - 7,
            y + size,
            x + size + 7,
            y + size,
            fill=color,
            outline="#263238",
            width=2,
        )
    else:
        canvas.create_oval(
            x - size,
            y - size,
            x + size,
            y + size,
            fill=color,
            outline="#263238",
            width=2,
        )

    canvas.create_text(
        x,
        y,
        text=boat.label,
        font=("Arial", max(7, int(size * 0.42)), "bold"),
        fill="#111111",
    )


# -----------------------------------------------------------------------------
# main
# -----------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(description="GUI visual de Scheduling Ships sin threading.")
    parser.add_argument("--port", help="Puerto serial del ESP32, por ejemplo /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate serial. Default: 115200")
    parser.add_argument("--input-log", help="Archivo de log para reproducir visualmente una ejecución ya capturada")
    parser.add_argument("--log-speed", type=int, default=1, help="Líneas por tick al usar --input-log")
    args = parser.parse_args()

    state = GUIState()

    source = None
    if args.port:
        source = SerialSource(args.port, args.baud)
        state.connected = True
        state.source = f"serial {args.port} @ {args.baud}"
    elif args.input_log:
        source = LogFileSource(args.input_log, lines_per_tick=args.log_speed)
        state.connected = True
        state.source = f"log {args.input_log}"
    else:
        state.source = "sin conexión"
        state.status = (
            "Sin fuente de datos. Ejecute con --port /dev/ttyUSB0 "
            "o --input-log archivo.txt"
        )

    root = tk.Tk()
    SchedulingShipsGUI(root, source, state)
    root.mainloop()


if __name__ == "__main__":
    main()