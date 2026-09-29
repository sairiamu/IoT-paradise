"""
ESP32 Motherboard Socket Designer
Draws a coloured, labelled ESP32 dev-board schematic with pin sockets.
Exports: esp32_board.pdf  and  esp32_board.png
"""

import matplotlib
matplotlib.use("Agg")          # headless renderer — change to "TkAgg" if you want a live window
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import numpy as np

# ── colour palette ────────────────────────────────────────────────────────────
C = {
    "board":   "#1a472a",   # dark-green PCB
    "silk":    "#f0f0c8",   # cream silkscreen
    "copper":  "#c8860a",   # copper pads
    "socket":  "#2c2c2c",   # black socket body
    "power":   "#e74c3c",   # red  = power pins
    "gnd":     "#2c3e50",   # navy = GND
    "gpio":    "#2980b9",   # blue = GPIO
    "adc":     "#8e44ad",   # purple = ADC
    "dac":     "#e67e22",   # orange = DAC
    "spi":     "#16a085",   # teal = SPI
    "i2c":     "#27ae60",   # green = I2C
    "uart":    "#f39c12",   # amber = UART
    "bg":      "#0d0d0d",   # near-black background
}

# ── pin definitions  (label, colour, left|right side) ─────────────────────────
LEFT_PINS = [
    ("3V3",      C["power"]),
    ("EN",       C["gpio"]),
    ("VP/ADC0",  C["adc"]),
    ("VN/ADC3",  C["adc"]),
    ("IO34/ADC6",C["adc"]),
    ("IO35/ADC7",C["adc"]),
    ("IO32/ADC4",C["adc"]),
    ("IO33/ADC5",C["adc"]),
    ("IO25/DAC1",C["dac"]),
    ("IO26/DAC2",C["dac"]),
    ("IO27",     C["gpio"]),
    ("IO14/SCK", C["spi"]),
    ("IO12/MISO",C["spi"]),
    ("GND",      C["gnd"]),
    ("IO13/MOSI",C["spi"]),
    ("SD2",      C["gpio"]),
    ("SD3",      C["gpio"]),
    ("CMD",      C["gpio"]),
    ("5V",       C["power"]),
]

RIGHT_PINS = [
    ("GND",      C["gnd"]),
    ("IO23",     C["gpio"]),
    ("IO22/SCL", C["i2c"]),
    ("TX0",      C["uart"]),
    ("RX0",      C["uart"]),
    ("IO21/SDA", C["i2c"]),
    ("GND",      C["gnd"]),
    ("IO19/MISO",C["spi"]),
    ("IO18/SCK", C["spi"]),
    ("IO5/SS",   C["spi"]),
    ("IO17",     C["gpio"]),
    ("IO16",     C["gpio"]),
    ("IO4",      C["gpio"]),
    ("IO0",      C["gpio"]),
    ("IO2",      C["gpio"]),
    ("IO15",     C["gpio"]),
    ("SD1",      C["gpio"]),
    ("SD0",      C["gpio"]),
    ("CLK",      C["gpio"]),
]

N = max(len(LEFT_PINS), len(RIGHT_PINS))

# ── canvas ────────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(14, 20))
fig.patch.set_facecolor(C["bg"])
ax.set_facecolor(C["bg"])
ax.set_xlim(0, 14)
ax.set_ylim(0, 20)
ax.set_aspect("equal")
ax.axis("off")

# board body
board_w, board_h = 7, N * 0.72 + 2.4
bx, by = 3.5, 1.2
board = FancyBboxPatch((bx, by), board_w, board_h,
                        boxstyle="round,pad=0.15",
                        linewidth=2, edgecolor=C["copper"],
                        facecolor=C["board"], zorder=2)
ax.add_patch(board)

# silkscreen outline (inner)
silk = FancyBboxPatch((bx+0.12, by+0.12), board_w-0.24, board_h-0.24,
                       boxstyle="round,pad=0.1",
                       linewidth=0.8, edgecolor=C["silk"],
                       facecolor="none", zorder=3)
ax.add_patch(silk)

# ── chip ──────────────────────────────────────────────────────────────────────
chip_cx, chip_cy = bx + board_w/2, by + board_h/2
chip_w, chip_h = 2.4, 2.0
chip = FancyBboxPatch((chip_cx - chip_w/2, chip_cy - chip_h/2), chip_w, chip_h,
                       boxstyle="round,pad=0.05",
                       linewidth=1.5, edgecolor="#aaaaaa",
                       facecolor="#222222", zorder=4)
ax.add_patch(chip)
ax.text(chip_cx, chip_cy+0.25, "ESP32", color="white",
        fontsize=12, fontweight="bold", ha="center", va="center",
        fontfamily="monospace", zorder=5)
ax.text(chip_cx, chip_cy-0.25, "WROOM-32", color="#aaaaaa",
        fontsize=7, ha="center", va="center", fontfamily="monospace", zorder=5)

# antenna stub (top of chip)
ax.plot([chip_cx, chip_cx], [chip_cy + chip_h/2, chip_cy + chip_h/2 + 0.7],
        color=C["copper"], linewidth=3, zorder=4)
ax.plot([chip_cx - 0.35, chip_cx + 0.35],
        [chip_cy + chip_h/2 + 0.7, chip_cy + chip_h/2 + 0.7],
        color=C["copper"], linewidth=2, zorder=4)

# USB connector (bottom of board)
usb_x = bx + board_w/2 - 0.4
usb_y = by - 0.5
usb = FancyBboxPatch((usb_x, usb_y), 0.8, 0.6,
                      boxstyle="square,pad=0.0",
                      linewidth=1.5, edgecolor="#888888",
                      facecolor="#444444", zorder=4)
ax.add_patch(usb)
ax.text(bx + board_w/2, usb_y + 0.3, "USB", color="white",
        fontsize=6, ha="center", va="center", zorder=5)

# ── helper: draw one pin row ───────────────────────────────────────────────────
PIN_R  = 0.14    # socket hole radius
PAD_R  = 0.22    # copper pad radius
PITCH  = 0.72    # vertical pitch

def draw_pins(pins, side):
    """side = 'left' or 'right'"""
    is_left = (side == "left")
    px = bx - 0.5 if is_left else bx + board_w + 0.5   # socket centre x
    text_x = px - 0.55 if is_left else px + 0.55
    align = "right" if is_left else "left"

    for i, (label, colour) in enumerate(pins):
        py = by + board_h - 0.9 - i * PITCH

        # copper pad on board edge
        pad = plt.Circle((px, py), PAD_R, color=C["copper"], zorder=3)
        ax.add_patch(pad)

        # socket body
        sock = plt.Circle((px, py), PAD_R + 0.06,
                            color=C["socket"], zorder=4, linewidth=1.2,
                            fill=False)
        ax.add_patch(sock)

        # coloured hole
        hole = plt.Circle((px, py), PIN_R, color=colour, zorder=5)
        ax.add_patch(hole)

        # pin number dot on board
        bd_x = (bx + 0.35) if is_left else (bx + board_w - 0.35)
        dot = plt.Circle((bd_x, py), 0.08, color=colour, alpha=0.85, zorder=6)
        ax.add_patch(dot)

        # trace line from dot to pin
        ax.plot([bd_x, px], [py, py],
                color=colour, linewidth=0.7, alpha=0.5, zorder=3)

        # label
        ax.text(text_x, py, label, color=colour,
                fontsize=6.8, ha=align, va="center",
                fontfamily="monospace", fontweight="bold", zorder=7)

draw_pins(LEFT_PINS,  "left")
draw_pins(RIGHT_PINS, "right")

# ── title & legend ────────────────────────────────────────────────────────────
ax.text(bx + board_w/2, by + board_h + 0.75,
        "ESP32 DevKit-C  ·  Socket Pinout",
        color=C["silk"], fontsize=13, ha="center", va="center",
        fontfamily="monospace", fontweight="bold", zorder=7)

ax.text(bx + board_w/2, by + board_h + 0.38,
        "38-pin  2.54 mm pitch",
        color="#888888", fontsize=8.5, ha="center", va="center",
        fontfamily="monospace", zorder=7)

legend_items = [
    ("Power (3V3/5V)", C["power"]),
    ("GND",            C["gnd"]),
    ("GPIO",           C["gpio"]),
    ("ADC",            C["adc"]),
    ("DAC",            C["dac"]),
    ("SPI",            C["spi"]),
    ("I2C",            C["i2c"]),
    ("UART",           C["uart"]),
]
handles = [mpatches.Patch(color=c, label=l) for l, c in legend_items]
legend = ax.legend(handles=handles, loc="lower center",
                   bbox_to_anchor=(0.5, -0.03),
                   ncol=4, fontsize=8,
                   facecolor="#1a1a1a", edgecolor=C["copper"],
                   labelcolor="white", framealpha=0.95)

# ── export ────────────────────────────────────────────────────────────────────
plt.tight_layout()

pdf_path = "esp32_board.pdf"
png_path = "esp32_board.png"

fig.savefig(pdf_path, format="pdf",  dpi=300, bbox_inches="tight",
            facecolor=C["bg"])
fig.savefig(png_path, format="png",  dpi=300, bbox_inches="tight",
            facecolor=C["bg"])

print(f"✅  Saved  →  {pdf_path}")
print(f"✅  Saved  →  {png_path}")
plt.close(fig)


