#!/usr/bin/env python3
"""Generate engineering diagrams for the battery safety controller family.

Outputs SVG, PNG and PDF files into this directory. The diagrams are intended
for design reviews, enclosure placement discussions and product documentation.
They are not fabrication drawings; use the KiCad package for production.
"""

from __future__ import annotations

import textwrap
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Circle, FancyArrowPatch, FancyBboxPatch, Polygon, Rectangle

OUT = Path(__file__).resolve().parent

BG = "#f7f9fc"
INK = "#1f2937"
MUTED = "#4b5563"
GRID = "#cbd5e1"

COLORS = {
    "mcu": ("#dbeafe", "#2f5d9e"),
    "sensor": ("#dcfce7", "#2f7d4f"),
    "power": ("#fef3c7", "#a16207"),
    "output": ("#fee2e2", "#b33a3a"),
    "comm": ("#ede9fe", "#6d4bb3"),
    "safety": ("#fce7f3", "#a83a75"),
    "neutral": ("#f1f5f9", "#64748b"),
    "hv": ("#ffedd5", "#c2410c"),
}


def new_fig(width: float = 16.0, height: float = 10.0):
    fig, ax = plt.subplots(figsize=(width, height), facecolor=BG)
    ax.set_facecolor(BG)
    ax.set_xlim(0, width)
    ax.set_ylim(0, height)
    ax.axis("off")
    return fig, ax


def box(
    ax,
    cx: float,
    cy: float,
    w: float,
    h: float,
    title: str,
    body: str = "",
    kind: str = "neutral",
    title_fs: float = 10.5,
    body_fs: float = 8.8,
    lw: float = 1.4,
):
    fc, ec = COLORS[kind]
    patch = FancyBboxPatch(
        (cx - w / 2, cy - h / 2),
        w,
        h,
        boxstyle="round,pad=0.02,rounding_size=0.09",
        linewidth=lw,
        edgecolor=ec,
        facecolor=fc,
        zorder=3,
    )
    ax.add_patch(patch)
    if body:
        ax.text(
            cx,
            cy + h * 0.16,
            title,
            ha="center",
            va="center",
            fontsize=title_fs,
            fontweight="bold",
            color=INK,
            zorder=4,
        )
        ax.text(
            cx,
            cy - h * 0.16,
            textwrap.fill(body, 30),
            ha="center",
            va="center",
            fontsize=body_fs,
            color=MUTED,
            zorder=4,
        )
    else:
        ax.text(
            cx,
            cy,
            textwrap.fill(title, 28),
            ha="center",
            va="center",
            fontsize=title_fs,
            fontweight="bold",
            color=INK,
            zorder=4,
        )
    return patch


def arrow(ax, start, end, label: str = "", color: str = MUTED, rad: float = 0.0, lw: float = 1.5):
    patch = FancyArrowPatch(
        start,
        end,
        arrowstyle="-|>",
        mutation_scale=14,
        linewidth=lw,
        color=color,
        connectionstyle=f"arc3,rad={rad}",
        shrinkA=4,
        shrinkB=4,
        zorder=2,
    )
    ax.add_patch(patch)
    if label:
        ax.text(
            (start[0] + end[0]) / 2,
            (start[1] + end[1]) / 2 + 0.12,
            label,
            ha="center",
            va="bottom",
            fontsize=7.6,
            color=color,
            zorder=5,
        )


def title(ax, text: str, subtitle: str = ""):
    ax.text(0.45, 9.62, text, ha="left", va="center", fontsize=18, fontweight="bold", color=INK)
    if subtitle:
        ax.text(0.45, 9.26, subtitle, ha="left", va="center", fontsize=10.5, color=MUTED)
    ax.plot([0.45, 15.55], [9.08, 9.08], color=GRID, linewidth=1.0)


def footer(ax, text: str):
    ax.text(0.45, 0.28, text, ha="left", va="center", fontsize=8.2, color=MUTED)


def save(fig, name: str):
    for ext in ("svg", "png", "pdf"):
        path = OUT / f"{name}.{ext}"
        kwargs = {"bbox_inches": "tight", "facecolor": BG}
        if ext == "png":
            kwargs["dpi"] = 220
        fig.savefig(path, **kwargs)
    plt.close(fig)
    print(f"wrote {name}.svg/.png/.pdf")


PROFILES = {
    "c1_bench_6v": {
        "title": "C1 Bench 6 V",
        "subtitle": "ML-enabled reference build | 8x LM35 | ACS712-05B | rain | LCD",
        "power": [
            ("6 V battery", "J1, 5 A fuse"),
            ("Reverse protection", "P-MOSFET Q1"),
            ("5 V buck", "6 V to 5 V"),
            ("3.3 V LDO", "AMS1117"),
        ],
        "sensors": [
            ("8x LM35", "0-100 C, 10 mV/C"),
            ("74HC4067", "8:1 mux, PB2-PB5"),
            ("ACS712-05B", "5 A, 2:3 divider"),
            ("Rain FC-37", "AO PA5, DO PB6"),
            ("Voltage divider", "100k/10k, 1:11"),
        ],
        "outputs": [
            ("Relay K1", "load disconnect"),
            ("Buzzer", "85 dB, PB10"),
            ("Status LED", "PC13"),
            ("LCD 16x2", "I2C 0x27"),
        ],
        "comms": [
            ("USART2 telemetry", "PA2/PA3, 115200"),
            ("E-stop", "PA8, NC loop"),
        ],
        "notes": [
            "ML inference enabled for this profile only.",
            "Calibrate ACS712 zero with the load disconnected.",
            "Keep analog return separate from relay return.",
        ],
    },
    "c2_ev_pack_48v": {
        "title": "C2 EV Pack 48 V",
        "subtitle": "HV traction pack | 8x PT100/MAX31865 | 200 A shunt | contactor",
        "power": [
            ("48 V pack", "F20 200 A"),
            ("Precharge", "R22 100 Ohm"),
            ("Isolated DC-DC", "48 V to 5 V"),
            ("3.3 V LDO", "AMS1117"),
        ],
        "sensors": [
            ("8x PT100", "3-wire, -50 to 250 C"),
            ("8x MAX31865", "RREF 430 Ohm"),
            ("74HC138", "CS decoder PB0-PB2"),
            ("2x TMP117", "0x48/0x49 ambient"),
            ("INA226", "0.5 mOhm, 200 A"),
        ],
        "outputs": [
            ("Contactor K20", "48 V 200 A"),
            ("Precharge path", "100 Ohm 50 W"),
            ("Buzzer", "HV fault alarm"),
            ("Fan", "PB11, 12 V"),
        ],
        "comms": [
            ("Isolated I2C", "ISO1540 barrier"),
            ("HVIL / E-stop", "PA8, NC loop"),
        ],
        "notes": [
            ">=8 mm creepage and clearance between HV and LV.",
            "Contactor must drop out on loss of coil power.",
            "No ML on this profile; deterministic safety rules only.",
        ],
    },
    "c3_server_ups_24v": {
        "title": "C3 Server UPS 24 V",
        "subtitle": "Rack/DIN UPS | 4x TMP117 | MLX90614 | SHT31 | 50 A bus",
        "power": [
            ("24 V input", "J30, 30 A fuse"),
            ("Isolated DC-DC", "24 V to 5 V"),
            ("3.3 V LDO", "AMS1117"),
            ("Relay K30", "30 A output"),
        ],
        "sensors": [
            ("4x TMP117", "0x48-0x4B cells"),
            ("MLX90614", "0x5A hotspot"),
            ("SHT31", "0x44 ambient RH"),
            ("INA226", "1 mOhm, 50 A"),
            ("Voltage monitor", "24 V bus"),
        ],
        "outputs": [
            ("Relay K30", "UPS output"),
            ("Fan", "40 mm, PB11"),
            ("Buzzer", "85 dB, PB10"),
            ("LCD 16x2", "I2C 0x27"),
        ],
        "comms": [
            ("RS485 gateway", "isolated USART2"),
            ("E-stop", "PA8, NC loop"),
        ],
        "notes": [
            "Place TMP117 on cell terminals, not in air.",
            "Aim MLX90614 at pack top, 5-10 cm away.",
            "SHT31 must be on the enclosure air inlet.",
        ],
    },
    "c4_solar_cabinet_12v": {
        "title": "C4 Solar Cabinet 12 V",
        "subtitle": "Outdoor IP65 | 4x NTC 10k | rain | SHT31 | 50 A bus",
        "power": [
            ("12 V battery", "J40, 20 A fuse"),
            ("Isolated DC-DC", "12 V to 5 V"),
            ("3.3 V LDO", "AMS1117"),
            ("Relay K40", "20 A output"),
        ],
        "sensors": [
            ("4x NTC 10k", "B3950, PA6-PB1"),
            ("SHT31", "0x44 ambient RH"),
            ("INA226", "1 mOhm, 50 A"),
            ("Rain FC-37", "AO PA5, DO PB6"),
            ("Voltage monitor", "12 V bus"),
        ],
        "outputs": [
            ("Relay K40", "load disconnect"),
            ("Fan", "40 mm IP68, PB11"),
            ("Buzzer", "85 dB, PB10"),
            ("LCD 16x2", "I2C 0x27"),
        ],
        "comms": [
            ("Isolated RS485", "USART2 gateway"),
            ("Door switch", "PA8, NC loop"),
        ],
        "notes": [
            "Use PG7 glands and conformal coating.",
            "Shield NTC cables from charger switching noise.",
            "Rain sensor must drain and see direct rain.",
        ],
    },
    "c5_cold_chain_12v": {
        "title": "C5 Cold Chain 12 V",
        "subtitle": "Insulated transport box | 2x TMP117 | SHT31 | ACS712-20A",
        "power": [
            ("12 V battery", "J1, 20 A fuse"),
            ("Isolated DC-DC", "12 V to 5 V"),
            ("3.3 V LDO", "AMS1117"),
            ("Relay K50", "compressor output"),
        ],
        "sensors": [
            ("2x TMP117", "0x48/0x49 battery/evap"),
            ("SHT31", "0x44 door air"),
            ("ACS712-20A", "20 A, 2:3 divider"),
            ("Voltage divider", "100k/10k, 1:11"),
        ],
        "outputs": [
            ("Relay K50", "compressor/load"),
            ("Fan", "5 V circulation"),
            ("Buzzer", "door/temp alarm"),
            ("LCD 16x2", "I2C 0x27"),
        ],
        "comms": [
            ("USART2 telemetry", "PA2/PA3, 115200"),
            ("Door switch", "PA8, NC loop"),
        ],
        "notes": [
            "Keep electronics out of the evaporator cold stream.",
            "Use a desiccant pack and gasketed door.",
            "Do not charge the battery below 0 C.",
        ],
    },
}


def system_architecture():
    fig, ax = new_fig()
    title(
        ax,
        "Battery Safety Controller - System Architecture",
        "Five application profiles | deterministic safety core | optional on-device ML | NAS telemetry",
    )

    box(ax, 2.45, 7.85, 3.5, 1.05, "Battery + environment", "cells, terminals, ambient, rain", "sensor")
    box(ax, 2.45, 6.35, 3.5, 1.05, "Sensor front ends", "LM35 / NTC / PT100 / TMP117 / MLX / SHT31", "sensor")
    box(ax, 2.45, 4.85, 3.5, 1.05, "Power sensing", "voltage divider / ACS712 / INA226", "sensor")
    box(ax, 2.45, 3.35, 3.5, 1.05, "Safety inputs", "e-stop / HVIL / door / tamper", "safety")

    box(ax, 8.0, 6.0, 4.0, 4.9, "STM32F411CEU6", "sensor manager\nfeature builder\nassessment state machine\nsafety controller\nRF model (bench only)", "mcu", title_fs=12.5, body_fs=9.2)

    box(ax, 13.45, 7.75, 3.4, 1.0, "Load disconnect", "relay / contactor / precharge", "output")
    box(ax, 13.45, 6.45, 3.4, 1.0, "Local alerts", "buzzer / status LED / LCD", "output")
    box(ax, 13.45, 5.15, 3.4, 1.0, "Thermal control", "fan / derating / charge inhibit", "output")
    box(ax, 13.45, 3.85, 3.4, 1.0, "Service", "SWD / UART1 / reset / CAL", "comm")

    for y in (7.85, 6.35, 4.85, 3.35):
        arrow(ax, (4.2, y), (6.0, 6.0 + (y - 6.0) * 0.32))
    for y in (7.75, 6.45, 5.15, 3.85):
        arrow(ax, (10.0, 6.0 + (y - 6.0) * 0.32), (11.75, y))

    ax.add_patch(
        Rectangle((5.55, 2.55), 12.0 - 5.55, 5.9, fill=False, linestyle=(0, (6, 4)), linewidth=1.2, edgecolor="#a83a75", zorder=1)
    )
    ax.text(5.7, 8.25, "3.3 V logic + safety domain", fontsize=8.5, color="#a83a75")

    box(ax, 4.0, 1.45, 3.2, 0.9, "USART2 telemetry", "$JSON*CRC16", "comm")
    box(ax, 8.0, 1.45, 3.2, 0.9, "Gateway", "paho-mqtt / buffer", "comm")
    box(ax, 12.0, 1.45, 3.2, 0.9, "NAS + dashboard", "Mosquitto / InfluxDB / Grafana", "comm")
    arrow(ax, (5.6, 1.45), (6.4, 1.45))
    arrow(ax, (9.6, 1.45), (10.4, 1.45))
    arrow(ax, (8.0, 3.35), (8.0, 1.9), "uplink")

    box(ax, 15.15, 9.35, 1.1, 0.35, "HV", "48 V", "hv", title_fs=8, body_fs=7)
    footer(ax, "Generated by hardware/diagrams/generate_diagrams.py | prototype design package | not a certified product")
    save(fig, "system_architecture")


def schematic(name: str, spec: dict):
    fig, ax = new_fig()
    title(ax, f"{spec['title']} - Circuit Block Schematic", spec["subtitle"])

    power_y = 8.35
    xs = [2.0, 5.9, 9.8, 13.7]
    for i, (label, body) in enumerate(spec["power"]):
        box(ax, xs[i], power_y, 3.3, 0.78, label, body, "power", title_fs=9.8, body_fs=8.0)
        if i < len(spec["power"]) - 1:
            arrow(ax, (xs[i] + 1.65, power_y), (xs[i + 1] - 1.65, power_y))
    ax.text(0.45, 8.75, "POWER TREE", fontsize=8.5, fontweight="bold", color="#a16207")

    box(
        ax,
        8.0,
        5.25,
        4.2,
        2.6,
        "STM32F411CEU6",
        "sensor manager\nfeature builder\nassessment + safety\nRF model (C1 only)",
        "mcu",
        title_fs=12.0,
        body_fs=8.8,
    )

    sensor_y = [7.15, 5.95, 4.75, 3.55, 2.35][: len(spec["sensors"])]
    for y, (label, body) in zip(sensor_y, spec["sensors"]):
        box(ax, 2.35, y, 3.1, 0.82, label, body, "sensor", title_fs=9.6, body_fs=7.8)
        arrow(ax, (3.9, y), (5.9, y if y <= 6.0 else 6.0))

    output_y = [7.05, 5.75, 4.45, 3.15][: len(spec["outputs"])]
    for y, (label, body) in zip(output_y, spec["outputs"]):
        box(ax, 13.65, y, 3.2, 0.82, label, body, "output", title_fs=9.6, body_fs=7.8)
        arrow(ax, (10.1, y if y <= 6.0 else 6.0), (12.05, y))

    comm_y = [1.45, 1.45]
    comm_x = [6.2, 11.3]
    for x, y, (label, body) in zip(comm_x, comm_y, spec["comms"]):
        box(ax, x, y, 3.4, 0.78, label, body, "comm", title_fs=9.5, body_fs=7.8)
        arrow(ax, (x, 1.84), (8.0, 3.95), "")

    ax.add_patch(
        Rectangle((0.65, 1.85), 14.7, 6.75, fill=False, linestyle=(0, (6, 4)), linewidth=1.1, edgecolor="#a83a75", zorder=1)
    )
    ax.text(0.8, 8.45, "Safety boundary: watchdog + fail-safe outputs", fontsize=8.5, color="#a83a75")

    notes = "\n".join(f"- {item}" for item in spec["notes"])
    ax.text(
        0.65,
        2.05,
        "Design notes\n" + textwrap.fill(notes, 30),
        ha="left",
        va="top",
        fontsize=7.8,
        color=MUTED,
        bbox=dict(boxstyle="round,pad=0.3", facecolor="#ffffff", edgecolor=GRID),
    )
    footer(ax, "Generated by hardware/diagrams/generate_diagrams.py | verify all ratings against the BOM and datasheets")
    save(fig, f"schematic_{name}")


PLACEMENTS = {
    "c1_bench_6v": {
        "enclosure": (320, 220, 120),
        "battery": (0.08, 0.50, 0.30, 0.30, "6 V battery"),
        "pcb": (0.52, 0.50, 0.36, 0.30, "Controller PCB"),
        "lcd": (0.55, 0.18, 0.28, 0.14, "LCD"),
        "sensors": [
            (0.12, 0.83, "T1 cell -"),
            (0.22, 0.83, "T2 cell +"),
            (0.12, 0.43, "T3 terminal"),
            (0.22, 0.43, "T4 ambient"),
            (0.33, 0.83, "T5..T8 array"),
            (0.33, 0.43, "Rain sensor"),
        ],
        "airflow": [(0.42, 0.50, 0.50, 0.50)],
        "glands": [(0.06, 0.95, "BAT"), (0.88, 0.95, "LOAD"), (0.70, 0.06, "UART")],
        "hot": [],
    },
    "c2_ev_pack_48v": {
        "enclosure": (520, 340, 180),
        "battery": (0.06, 0.14, 0.46, 0.68, "48 V LFP pack (4 modules)"),
        "pcb": (0.60, 0.48, 0.32, 0.34, "Controller PCB"),
        "lcd": (0.60, 0.18, 0.20, 0.12, "Service display"),
        "sensors": [
            (0.12, 0.74, "PT100 cell 1"),
            (0.22, 0.74, "PT100 cell 2"),
            (0.32, 0.74, "PT100 cell 3"),
            (0.42, 0.74, "PT100 cell 4"),
            (0.12, 0.30, "PT100 cell 5"),
            (0.22, 0.30, "PT100 cell 6"),
            (0.32, 0.30, "PT100 cell 7"),
            (0.42, 0.30, "PT100 cell 8"),
            (0.50, 0.84, "TMP117 busbar"),
            (0.52, 0.10, "TMP117 ambient"),
        ],
        "airflow": [(0.56, 0.84, 0.56, 0.20)],
        "glands": [(0.04, 0.90, "HV+"), (0.04, 0.80, "HV-"), (0.94, 0.70, "LOAD"), (0.94, 0.60, "HVIL")],
        "hot": [(0.18, 0.42, 0.22, 0.20)],
    },
    "c3_server_ups_24v": {
        "enclosure": (440, 300, 88),
        "battery": (0.06, 0.18, 0.34, 0.62, "24 V LFP pack"),
        "pcb": (0.52, 0.52, 0.36, 0.30, "Controller PCB"),
        "lcd": (0.54, 0.18, 0.22, 0.13, "LCD"),
        "sensors": [
            (0.14, 0.70, "TMP117 cell 1"),
            (0.24, 0.70, "TMP117 cell 2"),
            (0.14, 0.30, "TMP117 cell 3"),
            (0.24, 0.30, "TMP117 cell 4"),
            (0.36, 0.70, "MLX90614 top"),
            (0.36, 0.30, "SHT31 inlet"),
            (0.48, 0.42, "INA226 shunt"),
        ],
        "airflow": [(0.44, 0.84, 0.80, 0.84)],
        "glands": [(0.04, 0.90, "24 V IN"), (0.94, 0.90, "UPS OUT"), (0.94, 0.20, "RS485")],
        "hot": [(0.20, 0.50, 0.20, 0.18)],
    },
    "c4_solar_cabinet_12v": {
        "enclosure": (300, 200, 120),
        "battery": (0.07, 0.16, 0.32, 0.38, "12 V battery"),
        "pcb": (0.50, 0.46, 0.38, 0.32, "Controller PCB"),
        "lcd": (0.54, 0.14, 0.22, 0.13, "LCD"),
        "sensors": [
            (0.12, 0.62, "NTC1 cell -"),
            (0.24, 0.62, "NTC2 cell +"),
            (0.12, 0.40, "NTC3 cabinet top"),
            (0.24, 0.40, "NTC4 cabinet bottom"),
            (0.40, 0.62, "SHT31 ambient"),
            (0.40, 0.30, "INA226 shunt"),
            (0.70, 0.82, "Rain sensor (external)"),
        ],
        "airflow": [(0.44, 0.82, 0.64, 0.82)],
        "glands": [(0.04, 0.90, "SOLAR/BAT"), (0.96, 0.90, "LOAD"), (0.96, 0.10, "RS485")],
        "hot": [(0.30, 0.22, 0.20, 0.14)],
    },
    "c5_cold_chain_12v": {
        "enclosure": (400, 300, 200),
        "battery": (0.07, 0.18, 0.28, 0.40, "12 V battery"),
        "pcb": (0.45, 0.58, 0.32, 0.26, "Controller PCB"),
        "lcd": (0.47, 0.22, 0.20, 0.12, "LCD (door)"),
        "sensors": [
            (0.13, 0.68, "TMP117 battery"),
            (0.72, 0.62, "TMP117 evap"),
            (0.57, 0.36, "SHT31 door air"),
            (0.80, 0.30, "ACS712 compressor"),
            (0.40, 0.84, "Voltage divider"),
        ],
        "airflow": [(0.40, 0.30, 0.66, 0.30)],
        "glands": [(0.04, 0.90, "BAT"), (0.96, 0.90, "LOAD"), (0.96, 0.10, "UART")],
        "hot": [(0.78, 0.44, 0.14, 0.12)],
    },
}


def placement(name: str, spec: dict, layout: dict):
    fig, ax = new_fig()
    width_mm, height_mm, depth_mm = layout["enclosure"]
    title(
        ax,
        f"{spec['title']} - Enclosure and Sensor Placement",
        f"Enclosure {width_mm} x {height_mm} x {depth_mm} mm | front view with door removed | not to scale",
    )

    ex, ey, ew, eh = 0.8, 1.35, 14.4, 7.3
    ax.add_patch(Rectangle((ex, ey), ew, eh, facecolor="#ffffff", edgecolor=INK, linewidth=2.2, zorder=1))
    ax.add_patch(Rectangle((ex + 0.12, ey + 0.12), ew - 0.24, eh - 0.24, facecolor="none", edgecolor=GRID, linewidth=1.0, linestyle=(0, (4, 3)), zorder=1))
    ax.text(ex + 0.2, ey + eh - 0.22, "ENCLOSURE", fontsize=8.5, color=MUTED, fontweight="bold")

    def pos(nx, ny):
        return ex + nx * ew, ey + ny * eh

    for key, color, label in (
        ("battery", "#fef3c7", "BATTERY"),
        ("pcb", "#dbeafe", "PCB"),
        ("lcd", "#ede9fe", "LCD"),
    ):
        nx, ny, nw, nh, text = layout[key]
        x, y = pos(nx, ny)
        ax.add_patch(
            FancyBboxPatch(
                (x - nw * ew / 2, y - nh * eh / 2),
                nw * ew,
                nh * eh,
                boxstyle="round,pad=0.02,rounding_size=0.08",
                facecolor=color,
                edgecolor=COLORS["neutral"][1],
                linewidth=1.4,
                zorder=3,
            )
        )
        ax.text(x, y - nh * eh / 2 + 0.16, text, ha="center", va="bottom", fontsize=8.6, fontweight="bold", color=INK, zorder=4)

    for nx, ny, label in layout["sensors"]:
        x, y = pos(nx, ny)
        ax.add_patch(Circle((x, y), 0.17, facecolor="#dcfce7", edgecolor="#2f7d4f", linewidth=1.5, zorder=5))
        ax.text(x, y - 0.26, label, ha="center", va="top", fontsize=7.8, color=INK, zorder=6)

    for nx, ny, label in layout["glands"]:
        x, y = pos(nx, ny)
        ax.add_patch(Circle((x, y), 0.11, facecolor="#f1f5f9", edgecolor=MUTED, linewidth=1.2, zorder=5))
        ax.text(x, y + 0.16, label, ha="center", va="bottom", fontsize=7.2, color=MUTED, zorder=6)

    for x1, y1, x2, y2 in layout["airflow"]:
        start = pos(x1, y1)
        end = pos(x2, y2)
        arrow(ax, start, end, "", color="#0284c7", lw=1.8)
        ax.text((start[0] + end[0]) / 2, (start[1] + end[1]) / 2 + 0.15, "airflow", fontsize=7.4, color="#0284c7", ha="center")

    for nx, ny, nw, nh in layout.get("hot", []):
        x, y = pos(nx, ny)
        ax.add_patch(
            Rectangle(
                (x - nw * ew / 2, y - nh * eh / 2),
                nw * ew,
                nh * eh,
                facecolor="#fee2e2",
                edgecolor="#b33a3a",
                linewidth=1.0,
                alpha=0.55,
                zorder=2,
            )
        )
        ax.text(x, y, "HOT", ha="center", va="center", fontsize=8.0, color="#b33a3a", fontweight="bold", zorder=4)

    ax.annotate("", xy=(ex, ey - 0.18), xytext=(ex + ew, ey - 0.18), arrowprops=dict(arrowstyle="<->", color=MUTED, lw=1.0))
    ax.text(ex + ew / 2, ey - 0.42, f"{width_mm} mm", ha="center", va="center", fontsize=8.2, color=MUTED)
    ax.annotate("", xy=(ex - 0.18, ey), xytext=(ex - 0.18, ey + eh), arrowprops=dict(arrowstyle="<->", color=MUTED, lw=1.0))
    ax.text(ex - 0.42, ey + eh / 2, f"{height_mm} mm", ha="center", va="center", rotation=90, fontsize=8.2, color=MUTED)

    legend = [
        Line2D([0], [0], marker="o", color="w", markerfacecolor="#dcfce7", markeredgecolor="#2f7d4f", markersize=10, label="Temperature / environment sensor"),
        Line2D([0], [0], marker="s", color="w", markerfacecolor="#dbeafe", markeredgecolor=COLORS["neutral"][1], markersize=10, label="Controller PCB"),
        Line2D([0], [0], color="#0284c7", lw=2, label="Cooling airflow"),
        Line2D([0], [0], marker="o", color="w", markerfacecolor="#f1f5f9", markeredgecolor=MUTED, markersize=9, label="Cable gland"),
    ]
    ax.legend(handles=legend, loc="lower left", bbox_to_anchor=(0.055, 0.035), ncol=4, frameon=False, fontsize=7.8)
    footer(ax, "Generated by hardware/diagrams/generate_diagrams.py | verify clearances, cable bend radius and service access in CAD")
    save(fig, f"placement_{name}")


def wiring_c1():
    fig, ax = new_fig()
    title(ax, "C1 Bench 6 V - Wiring and Pin Assignment", "Reference point-to-point harness | USART2 gateway | I2C LCD | 8x LM35")

    box(ax, 8.0, 5.55, 4.2, 3.2, "STM32F411CEU6", "PA0  mux out\nPA1  battery voltage\nPA4  ACS712 current\nPA5/PB6 rain\nPB2-PB5 mux select\nPB7 relay / PB10 buzzer\nPB8/PB9 I2C\nPA2/PA3 UART2\nPA8 e-stop", "mcu", title_fs=11.5, body_fs=9.0, lw=1.8)

    sensors = [
        ("8x LM35", "V+ 5 V, GND, OUT -> Y0..Y7", 2.3, 7.5),
        ("74HC4067", "SIG -> PA0, S0..S3 -> PB2..PB5", 2.3, 6.0),
        ("Divider 100k/10k", "BAT+ -> PA1, 100 nF to GND", 2.3, 4.5),
        ("ACS712-05B", "OUT -> 20k/10k -> PA4", 2.3, 3.0),
        ("Rain FC-37", "AO -> PA5, DO -> PB6", 2.3, 1.5),
    ]
    for label, body, x, y in sensors:
        box(ax, x, y, 3.4, 0.95, label, body, "sensor", title_fs=9.5, body_fs=8.0)
        arrow(ax, (4.0, y), (5.9, 5.55 + (y - 5.55) * 0.18))

    outputs = [
        ("Relay K1", "PB7 -> 2N7002 -> coil", 13.7, 7.5),
        ("Buzzer", "PB10 -> 2N7002 -> 5 V", 13.7, 6.0),
        ("I2C LCD 0x27", "SDA PB9, SCL PB8, 5 V", 13.7, 4.5),
        ("USART2 gateway", "TX PA2, RX PA3, GND", 13.7, 3.0),
        ("E-stop + reset", "PA8 NC, PC14 reset", 13.7, 1.5),
    ]
    for label, body, x, y in outputs:
        box(ax, x, y, 3.4, 0.95, label, body, "output" if y > 4 else "comm", title_fs=9.5, body_fs=8.0)
        arrow(ax, (10.1, 5.55 + (y - 5.55) * 0.18), (12.0, y))

    box(ax, 8.0, 8.55, 5.0, 0.75, "6 V battery -> F2 5 A -> Q1 -> 5 V buck -> 3.3 V LDO", "", "power", title_fs=9.0)
    ax.text(8.0, 1.1, "Wire gauge: 22 AWG signals, 18 AWG coil/fan, 12 AWG battery/load. Keep analog return separate from relay return.",
            ha="center", va="center", fontsize=8.2, color=MUTED)
    footer(ax, "Generated by hardware/diagrams/generate_diagrams.py | match the firmware pin map in hardware/wiring/pin_map.csv")
    save(fig, "wiring_c1_bench_6v")


def pcb_layout(name: str, spec: dict):
    fig, ax = new_fig()
    title(
        ax,
        f"{spec['title']} - PCB Placement Guide",
        "Common 80 x 56 mm controller outline | 4-layer stackup | top-side placement | not a fabrication drawing",
    )

    bx, by, bw, bh = 1.6, 1.55, 12.8, 7.25
    ax.add_patch(Rectangle((bx, by), bw, bh, facecolor="#f8fafc", edgecolor="#0f172a", linewidth=2.2, zorder=1))
    ax.add_patch(Rectangle((bx + 0.18, by + 0.18), bw - 0.36, bh - 0.36, fill=False, edgecolor=GRID, linestyle=(0, (4, 3)), linewidth=1.0, zorder=1))
    ax.text(bx + 0.25, by + bh - 0.28, "80 mm", fontsize=8.0, color=MUTED)
    ax.text(bx + 0.25, by + 0.25, "56 mm", fontsize=8.0, color=MUTED)

    for hx, hy in ((bx + 0.45, by + 0.45), (bx + bw - 0.45, by + 0.45), (bx + 0.45, by + bh - 0.45), (bx + bw - 0.45, by + bh - 0.45)):
        ax.add_patch(Circle((hx, hy), 0.18, facecolor="#ffffff", edgecolor=MUTED, linewidth=1.2, zorder=4))
        ax.add_patch(Circle((hx, hy), 0.07, facecolor=BG, edgecolor=MUTED, linewidth=0.8, zorder=5))

    box(ax, 4.0, 7.85, 3.2, 0.95, "Power + protection", "fuse, TVS, DC-DC, LDO", "power", title_fs=9.3, body_fs=7.8)
    box(ax, 7.7, 5.55, 3.8, 2.5, "STM32F411CEU6", "MCU, SWD, reset\nUART1 / UART2\nI2C1 / SPI2", "mcu", title_fs=10.5, body_fs=8.2)
    box(ax, 4.2, 3.95, 4.0, 2.3, "Analog front end", "dividers, filters,\nMUX / NTC / ACS712", "sensor", title_fs=9.8, body_fs=8.0)
    box(ax, 11.1, 7.65, 3.0, 1.0, "I2C sensors", "TMP117 / SHT31 /\nINA226 / MLX", "sensor", title_fs=9.2, body_fs=7.6)
    box(ax, 11.1, 5.55, 3.0, 1.55, "Relay + alerts", "relay driver,\nbuzzer, fan, LED", "output", title_fs=9.2, body_fs=7.6)
    box(ax, 2.85, 1.95, 1.9, 0.7, "J1/J2", "battery/load", "power", title_fs=8.4, body_fs=7.2)
    box(ax, 5.9, 1.95, 1.9, 0.7, "J3/J4", "I2C/SPI", "comm", title_fs=8.4, body_fs=7.2)
    box(ax, 8.95, 1.95, 1.9, 0.7, "J6/J7/J8", "UART/fan/buzzer", "comm", title_fs=8.2, body_fs=7.0)
    box(ax, 11.1, 2.55, 3.0, 1.0, "J5 / J9", "e-stop / SWD", "safety", title_fs=8.8, body_fs=7.2)

    if name == "c2_ev_pack_48v":
        ax.plot([10.0, 10.0], [2.0, 8.35], color="#c2410c", linewidth=2.2, linestyle=(0, (5, 3)), zorder=2)
        ax.text(10.08, 8.05, "ISOLATION SLOT", rotation=90, fontsize=8.0, color="#c2410c", va="top")
        ax.add_patch(Rectangle((10.2, 2.2), 3.9, 2.3, facecolor="#ffedd5", edgecolor="#c2410c", linewidth=1.2, alpha=0.5, zorder=1))
        ax.text(12.15, 3.35, "HV AREA\ncontactor / precharge\n>=8 mm clearance", ha="center", va="center", fontsize=8.0, color="#c2410c", fontweight="bold")

    if name == "c1_bench_6v":
        ax.text(7.7, 4.05, "ML model flash: 24 trees / 642 nodes", ha="center", va="center", fontsize=7.6, color=MUTED)

    ax.text(8.0, 0.85, "Stackup: 4-layer FR4, 1.6 mm, 1 oz copper | min trace/space 0.20 mm | min drill 0.30 mm | keep analog ground under front end.",
            ha="center", va="center", fontsize=8.0, color=MUTED)
    footer(ax, "Generated by hardware/diagrams/generate_diagrams.py | capture in KiCad before fabrication and run DRC/ERC")
    save(fig, f"pcb_layout_{name}")


def enclosure_section():
    fig, ax = new_fig()
    title(ax, "Common Enclosure Section", "IP65/ IP67 construction guidance | gasket, standoffs, cable glands, airflow path")
    ax.add_patch(Rectangle((1.0, 1.4), 14.0, 7.2, facecolor="#e2e8f0", edgecolor=INK, linewidth=2.0))
    ax.add_patch(Rectangle((1.25, 1.65), 13.5, 6.7, facecolor="#ffffff", edgecolor=GRID, linewidth=1.2))
    ax.add_patch(Rectangle((1.45, 1.85), 13.1, 6.3, facecolor="none", edgecolor="#94a3b8", linewidth=1.0, linestyle=(0, (3, 2))))
    ax.text(1.7, 8.15, "Outer shell", fontsize=9.0, color=MUTED)
    ax.text(1.7, 7.85, "Gasket groove + IP-rated seal", fontsize=8.0, color=MUTED)

    ax.add_patch(Rectangle((2.1, 2.15), 4.6, 2.6, facecolor="#fef3c7", edgecolor="#a16207", linewidth=1.4))
    ax.text(4.4, 3.45, "Battery / heat source", ha="center", va="center", fontsize=9.0, fontweight="bold", color=INK)

    ax.add_patch(Rectangle((8.6, 4.7), 4.4, 2.4, facecolor="#dbeafe", edgecolor="#2f5d9e", linewidth=1.4))
    ax.text(10.8, 5.9, "Controller PCB", ha="center", va="center", fontsize=9.0, fontweight="bold", color=INK)
    for x in (9.0, 12.6):
        ax.add_patch(Rectangle((x, 4.2), 0.2, 0.5, facecolor="#94a3b8", edgecolor=MUTED))
    ax.text(10.8, 4.35, "Standoffs >=6 mm", ha="center", va="center", fontsize=7.8, color=MUTED)

    arrow(ax, (7.0, 2.7), (8.4, 2.7), "cool air inlet", color="#0284c7")
    arrow(ax, (8.4, 6.5), (12.9, 6.5), "warm air exhaust", color="#b33a3a")
    ax.add_patch(Rectangle((2.0, 1.55), 0.9, 0.25, facecolor="#f1f5f9", edgecolor=MUTED))
    ax.text(2.45, 1.25, "PG7 gland", ha="center", va="top", fontsize=7.8, color=MUTED)
    ax.add_patch(Rectangle((13.4, 1.55), 0.9, 0.25, facecolor="#f1f5f9", edgecolor=MUTED))
    ax.text(13.85, 1.25, "PG7 gland", ha="center", va="top", fontsize=7.8, color=MUTED)

    ax.text(8.0, 0.65, "Rules: gasket compression 30-40%, desiccant, drip loop, service loop, separate HV/LV glands, conformal coating.",
            ha="center", va="center", fontsize=8.4, color=MUTED)
    footer(ax, "Generated by hardware/diagrams/generate_diagrams.py | final enclosure must pass IP and thermal validation")
    save(fig, "enclosure_section")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    system_architecture()
    enclosure_section()
    wiring_c1()
    for name, spec in PROFILES.items():
        schematic(name, spec)
        placement(name, spec, PLACEMENTS[name])
        pcb_layout(name, spec)
    print(f"all diagrams written to {OUT}")


if __name__ == "__main__":
    main()
