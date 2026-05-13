import argparse
import tkinter as tk


class SchedulingShipsGUI:
    def __init__(self, root, demo=False):
        self.root = root
        self.demo = demo

        self.root.title("Scheduling Ships - Interfaz")
        self.root.geometry("1180x780")
        self.root.resizable(False, False)

        self.root.bind("w", self.stop)
        self.root.bind("W", self.stop)
        self.root.bind("i", self.trigger_interrupt)
        self.root.bind("I", self.trigger_interrupt)

        self.running = True

        self.scheduler = "RR"
        self.flow = "Equidad W=2"
        self.channel_length = 10
        self.channel_speed = "variable por tipo"

        self.left_queue = ["L1_Normal", "L2_Patrulla"]
        self.right_queue = ["R1_Pesquera", "R2_Normal"]

        self.current_ship = None
        self.direction = "L2R"
        self.status = "Esperando inicio de simulación"

        self.progress = 0.0
        self.demo_index = -1
        self.wait_ticks = 0

        self.interrupt_active = False
        self.interrupt_ticks = 0
        self.sensor_message = "Sensores sin alerta"

        self.demo_events = [
            {
                "ship": "L1_Normal",
                "direction": "L2R",
                "left_queue": ["L2_Patrulla"],
                "right_queue": ["R1_Pesquera", "R2_Normal"],
                "status": "L1_Normal cruzando de izquierda a derecha",
            },
            {
                "ship": "L2_Patrulla",
                "direction": "L2R",
                "left_queue": [],
                "right_queue": ["R1_Pesquera", "R2_Normal"],
                "status": "L2_Patrulla cruzando de izquierda a derecha",
            },
            {
                "ship": "R1_Pesquera",
                "direction": "R2L",
                "left_queue": [],
                "right_queue": ["R2_Normal"],
                "status": "R1_Pesquera cruzando de derecha a izquierda",
            },
            {
                "ship": "R2_Normal",
                "direction": "R2L",
                "left_queue": [],
                "right_queue": [],
                "status": "R2_Normal cruzando de derecha a izquierda",
            },
        ]

        self.canvas = tk.Canvas(
            root,
            width=1180,
            height=780,
            bg="#eef3f5",
            highlightthickness=0
        )
        self.canvas.pack(fill="both", expand=True)

        self.next_demo_event()
        self.update_loop()

    def stop(self, event=None):
        self.running = False
        self.root.destroy()

    def trigger_interrupt(self, event=None):
        self.interrupt_active = True
        self.interrupt_ticks = 45
        self.sensor_message = "ALERTA: sensor detectó aproximación"

        if self.current_ship is not None:
            returned_ship = self.current_ship

            if self.direction == "L2R":
                self.left_queue.insert(0, returned_ship)
            else:
                self.right_queue.insert(0, returned_ship)

            self.current_ship = None
            self.progress = 0.0
            self.status = f"Interrupción: {returned_ship} fue devuelto a la cola"
        else:
            self.status = "Interrupción: agujas abajo, canal protegido"

    def update_loop(self):
        if not self.running:
            return

        if self.interrupt_active:
            self.interrupt_ticks -= 1

            if self.interrupt_ticks <= 0:
                self.interrupt_active = False
                self.sensor_message = "Sensores sin alerta"
                self.status = "Interrupción resuelta. Canal disponible."

                if self.demo:
                    self.next_demo_event()

        elif self.demo:
            self.update_demo_state()

        self.draw()
        self.root.after(40, self.update_loop)

    def update_demo_state(self):
        if self.current_ship is None:
            return

        self.progress += self.get_ship_speed(self.current_ship)

        if self.progress >= 1.0:
            self.progress = 1.0
            self.wait_ticks += 1

            if self.wait_ticks >= 18:
                self.next_demo_event()

    def next_demo_event(self):
        self.demo_index += 1

        if self.demo_index >= len(self.demo_events):
            self.current_ship = None
            self.left_queue = []
            self.right_queue = []
            self.status = "Todas las colas vacías. Canal inactivo."
            return

        event = self.demo_events[self.demo_index]

        self.current_ship = event["ship"]
        self.direction = event["direction"]
        self.left_queue = event["left_queue"].copy()
        self.right_queue = event["right_queue"].copy()
        self.status = event["status"]

        self.progress = 0.0
        self.wait_ticks = 0

    def get_ship_type(self, ship_name):
        if ship_name is None:
            return "Normal"
        if "Patrulla" in ship_name:
            return "Patrulla"
        if "Pesquera" in ship_name:
            return "Pesquera"
        return "Normal"

    def get_ship_speed(self, ship_name):
        ship_type = self.get_ship_type(ship_name)
        if ship_type == "Patrulla":
            return 0.020
        if ship_type == "Pesquera":
            return 0.016
        return 0.012

    def short_name(self, ship_name):
        if ship_name is None:
            return ""
        return ship_name.split("_")[0]

    def ship_color(self, ship_name):
        ship_type = self.get_ship_type(ship_name)
        if ship_type == "Patrulla":
            return "#ef5350"
        if ship_type == "Pesquera":
            return "#42a5f5"
        return "#b0bec5"

    def draw_ship_shape(self, x, y, ship_name, size=22):
        ship_type = self.get_ship_type(ship_name)
        color = self.ship_color(ship_name)
        label = self.short_name(ship_name)

        if ship_type == "Normal":
            self.canvas.create_oval(
                x - size, y - size,
                x + size, y + size,
                fill=color,
                outline="#263238",
                width=2
            )

        elif ship_type == "Pesquera":
            self.canvas.create_polygon(
                x, y - size - 4,
                x - size - 5, y + size,
                x + size + 5, y + size,
                fill=color,
                outline="#263238",
                width=2
            )

        elif ship_type == "Patrulla":
            self.canvas.create_polygon(
                x, y - size - 5,
                x + size + 5, y,
                x, y + size + 5,
                x - size - 5, y,
                fill=color,
                outline="#263238",
                width=2
            )

        self.canvas.create_text(
            x,
            y,
            text=label,
            font=("Arial", 9, "bold"),
            fill="#111111"
        )

    def draw_header(self):
        self.canvas.create_rectangle(0, 0, 1180, 86, fill="#223038", outline="")

        self.canvas.create_text(
            28,
            30,
            text="Scheduling Ships",
            anchor="w",
            font=("Arial", 24, "bold"),
            fill="white"
        )

        self.canvas.create_text(
            28,
            61,
            text="W: salir | I: simular interrupción/sensor",
            anchor="w",
            font=("Arial", 11),
            fill="#d7e1e5"
        )

        self.canvas.create_text(
            1150,
            34,
            text=f"Scheduler: {self.scheduler}     |     Flujo: {self.flow}",
            anchor="e",
            font=("Arial", 14, "bold"),
            fill="white"
        )

        self.canvas.create_text(
            1150,
            62,
            text=f"Largo canal: {self.channel_length} posiciones     |     Velocidad: {self.channel_speed}",
            anchor="e",
            font=("Arial", 10),
            fill="#d7e1e5"
        )

    def draw_panel(self, x1, y1, x2, y2, title):
        self.canvas.create_rectangle(x1, y1, x2, y2, fill="white", outline="#c9d3d8", width=2)
        self.canvas.create_rectangle(x1, y1, x2, y1 + 44, fill="#e8eef1", outline="#c9d3d8")
        self.canvas.create_text(
            (x1 + x2) // 2,
            y1 + 23,
            text=title,
            font=("Arial", 14, "bold"),
            fill="#223038"
        )

    def draw_queue(self, x1, y1, x2, y2, title, ships, ocean_name):
        self.draw_panel(x1, y1, x2, y2, title)

        self.canvas.create_text(
            (x1 + x2) // 2,
            y1 + 70,
            text=ocean_name,
            font=("Arial", 11),
            fill="#607d8b"
        )

        self.canvas.create_text(
            (x1 + x2) // 2,
            y1 + 96,
            text=f"Barcos en cola: {len(ships)} / 4",
            font=("Arial", 10, "bold"),
            fill="#223038"
        )

        if not ships:
            self.canvas.create_text(
                (x1 + x2) // 2,
                (y1 + y2) // 2 + 20,
                text="Cola vacía",
                font=("Arial", 13, "italic"),
                fill="#78909c"
            )
            return

        start_y = y1 + 128

        for i, ship in enumerate(ships[:4]):
            by = start_y + i * 66

            self.canvas.create_rectangle(
                x1 + 25,
                by,
                x2 - 25,
                by + 52,
                fill="#fbfbfb",
                outline="#cfd8dc"
            )

            self.draw_ship_shape(x1 + 58, by + 26, ship, size=14)

            self.canvas.create_text(
                x1 + 100,
                by + 17,
                text=self.short_name(ship),
                anchor="w",
                font=("Arial", 12, "bold"),
                fill="#223038"
            )

            self.canvas.create_text(
                x1 + 100,
                by + 37,
                text=self.get_ship_type(ship),
                anchor="w",
                font=("Arial", 10),
                fill="#607d8b"
            )

    def draw_direction_sign(self):
        x1, y1 = 365, 112
        x2, y2 = 815, 190

        self.canvas.create_rectangle(x1, y1, x2, y2, fill="white", outline="#c9d3d8", width=2)

        if self.interrupt_active:
            title = "LETRERO: CANAL CERRADO POR INTERRUPCIÓN"
            arrow = "X     CANAL BLOQUEADO     X"
            color = "#c62828"
        elif self.direction == "L2R":
            title = "LETRERO: IZQUIERDA  ->  DERECHA"
            arrow = "L  ----->  R"
            color = "#1565c0"
        else:
            title = "LETRERO: DERECHA  ->  IZQUIERDA"
            arrow = "L  <-----  R"
            color = "#1565c0"

        self.canvas.create_text(
            590,
            138,
            text=title,
            font=("Arial", 13, "bold"),
            fill="#223038"
        )

        self.canvas.create_text(
            590,
            166,
            text=arrow,
            font=("Arial", 18, "bold"),
            fill=color
        )

    def draw_sensor_and_gate(self, x, y, side):
        sensor_color = "#c62828" if self.interrupt_active else "#2e7d32"

        gate_down = self.current_ship is not None or self.interrupt_active
        gate_color = "#c62828" if gate_down else "#2e7d32"
        gate_state = "BAJA" if gate_down else "ALTA"

        if side == "left":
            sensor_label = "Sensor IZQ"
            gate_label = "Aguja IZQ"
        else:
            sensor_label = "Sensor DER"
            gate_label = "Aguja DER"

        self.canvas.create_oval(
            x - 9,
            y - 9,
            x + 9,
            y + 9,
            fill=sensor_color,
            outline="#223038"
        )

        self.canvas.create_text(
            x,
            y + 24,
            text=sensor_label,
            font=("Arial", 8, "bold"),
            fill="#223038"
        )

        post_y = y + 63

        self.canvas.create_line(x, post_y - 24, x, post_y + 24, fill="#223038", width=4)

        if gate_down:
            self.canvas.create_line(
                x - 28,
                post_y,
                x + 28,
                post_y,
                fill=gate_color,
                width=5
            )
        else:
            self.canvas.create_line(
                x,
                post_y,
                x + 28,
                post_y - 28,
                fill=gate_color,
                width=5
            )

        self.canvas.create_text(
            x,
            post_y + 42,
            text=f"{gate_label}: {gate_state}",
            font=("Arial", 8, "bold"),
            fill="#223038"
        )

    def draw_channel(self):
        x1, y1 = 360, 278
        x2, y2 = 820, 382

        self.canvas.create_text(
            590,
            238,
            text="CANAL / CPU",
            font=("Arial", 21, "bold"),
            fill="#223038"
        )

        self.canvas.create_rectangle(
            x1,
            y1,
            x2,
            y2,
            fill="#bbdefb",
            outline="#1565c0",
            width=4
        )

        for i in range(self.channel_length + 1):
            px = x1 + 20 + i * ((x2 - x1 - 40) / self.channel_length)

            self.canvas.create_line(px, y1 + 6, px, y1 + 20, fill="#1565c0", width=1)

            self.canvas.create_text(
                px,
                y1 - 11,
                text=str(i),
                font=("Arial", 8),
                fill="#607d8b"
            )

        for i in range(0, x2 - x1, 34):
            self.canvas.create_line(
                x1 + i,
                y1 + 26,
                x1 + i + 18,
                y1 + 44,
                fill="#90caf9",
                width=2
            )

            self.canvas.create_line(
                x1 + i,
                y2 - 26,
                x1 + i + 18,
                y2 - 44,
                fill="#90caf9",
                width=2
            )

        self.draw_sensor_and_gate(x1 - 43, 307, "left")
        self.draw_sensor_and_gate(x2 + 43, 307, "right")

        if self.current_ship:
            start_x = x1 + 36
            end_x = x2 - 36

            if self.direction == "L2R":
                ship_x = start_x + (end_x - start_x) * self.progress
            else:
                ship_x = end_x - (end_x - start_x) * self.progress

            self.draw_ship_shape(ship_x, 330, self.current_ship, size=24)

        if self.interrupt_active:
            busy_text = "Canal cerrado por interrupción"
            busy_color = "#c62828"
        elif self.current_ship is not None:
            busy_text = "Canal ocupado: 1 barco"
            busy_color = "#ef6c00"
        else:
            busy_text = "Canal libre"
            busy_color = "#2e7d32"

        self.canvas.create_rectangle(415, 414, 765, 464, fill="white", outline="#c9d3d8", width=2)
        self.canvas.create_text(
            590,
            439,
            text=busy_text,
            font=("Arial", 14, "bold"),
            fill=busy_color
        )

        current = self.current_ship if self.current_ship else "ninguno"

        self.canvas.create_text(
            590,
            495,
            text=f"Barco actual: {current}",
            font=("Arial", 14, "bold"),
            fill="#223038"
        )

    def draw_legend_centered(self):
        x1, y1 = 360, 530
        x2, y2 = 820, 630

        self.canvas.create_rectangle(x1, y1, x2, y2, fill="white", outline="#c9d3d8", width=2)

        self.canvas.create_text(
            590,
            y1 + 20,
            text="Tipos de barco",
            font=("Arial", 14, "bold"),
            fill="#223038"
        )

        items = [
            ("Normal", "lento", "L1_Normal", 455),
            ("Pesquera", "velocidad media", "R1_Pesquera", 590),
            ("Patrulla", "rápida / urgente", "L2_Patrulla", 725),
        ]

        for name, desc, sample, x in items:
            self.draw_ship_shape(x, y1 + 55, sample, size=13)

            self.canvas.create_text(
                x,
                y1 + 78,
                text=name,
                font=("Arial", 9, "bold"),
                fill="#223038"
            )

            self.canvas.create_text(
                x,
                y1 + 93,
                text=desc,
                font=("Arial", 8),
                fill="#607d8b"
            )

    def draw_status(self):
        self.canvas.create_rectangle(0, 660, 1180, 780, fill="#223038", outline="")

        self.canvas.create_text(
            28,
            688,
            text="Estado:",
            anchor="w",
            font=("Arial", 14, "bold"),
            fill="white"
        )

        self.canvas.create_text(
            105,
            688,
            text=self.status,
            anchor="w",
            font=("Arial", 14),
            fill="#eceff1"
        )

        self.canvas.create_text(
            28,
            720,
            text=f"Seguridad: {self.sensor_message}. No se permite cruce en ambos sentidos ni dos barcos en la misma posición.",
            anchor="w",
            font=("Arial", 11),
            fill="#cfd8dc"
        )

        self.canvas.create_text(
            28,
            747,
            text="Interrupción: presione I para simular sensor de proximidad; las agujas bajan y el barco vuelve a cola.",
            anchor="w",
            font=("Arial", 11),
            fill="#cfd8dc"
        )

    def draw(self):
        self.canvas.delete("all")

        self.draw_header()
        self.draw_direction_sign()

        self.draw_queue(
            30,
            125,
            305,
            630,
            "Cola izquierda",
            self.left_queue,
            "Océano izquierdo"
        )

        self.draw_queue(
            875,
            125,
            1150,
            630,
            "Cola derecha",
            self.right_queue,
            "Océano derecho"
        )

        self.draw_channel()
        self.draw_legend_centered()
        self.draw_status()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--demo",
        action="store_true",
        help="Ejecuta la interfaz con datos simulados"
    )
    args = parser.parse_args()

    root = tk.Tk()
    SchedulingShipsGUI(root, demo=args.demo)
    root.mainloop()


if __name__ == "__main__":
    main()