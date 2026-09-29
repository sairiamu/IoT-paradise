#!/usr/bin/env python3
"""
ESP32 Power Management + Sensor Interface Board
Full schematic: LDO regulator, decoupling caps, pull-up resistors,
flyback diode, status LED, I2C/SPI headers, reset circuit, bypass network.
Exports: board_schematic.pdf  and  board_schematic.png
Install: pip install matplotlib numpy
"""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
from matplotlib.lines import Line2D
import numpy as np

# ══════════════════════════════════════════════════════════════════════════════
# COLOUR SYSTEM
# ══════════════════════════════════════════════════════════════════════════════
BG      = "#0a0e14"
GRID    = "#111820"
SILK    = "#e8e4d0"
NET_PWR = "#e05252"   # VCC / power nets
NET_GND = "#4a90d9"   # GND nets
NET_SIG = "#50c878"   # signal nets
NET_CLK = "#f0c040"   # clock / SPI nets
NET_I2C = "#c87df0"   # I2C nets
COMP    = "#c8a86a"   # component body
COMP_HL = "#f0d090"   # component highlight
TEXT_H  = "#e8e4d0"   # heading text
TEXT_S  = "#8a9aaa"   # sub text
WIRE    = "#3a5a7a"   # pcb wire trace
BORDER  = "#1a2a3a"

fig = plt.figure(figsize=(22, 28), facecolor=BG)
ax  = fig.add_axes([0, 0, 1, 1], facecolor=BG)
ax.set_xlim(0, 22)
ax.set_ylim(0, 28)
ax.set_aspect("equal")
ax.axis("off")

# ── grid dots ─────────────────────────────────────────────────────────────────
for gx in np.arange(0.5, 22, 0.5):
    for gy in np.arange(0.5, 28, 0.5):
        ax.plot(gx, gy, ".", color=GRID, markersize=1, zorder=0)

# ══════════════════════════════════════════════════════════════════════════════
# HELPERS
# ══════════════════════════════════════════════════════════════════════════════

def wire(x1, y1, x2, y2, color=NET_SIG, lw=1.4, z=3):
    ax.plot([x1, x2], [y1, y2], color=color, linewidth=lw, solid_capstyle="round", zorder=z)

def wire_path(pts, color=NET_SIG, lw=1.4):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color=color, linewidth=lw, solid_capstyle="round", zorder=3)

def junction(x, y, color=NET_SIG, r=0.08):
    ax.add_patch(plt.Circle((x, y), r, color=color, zorder=5))

def label(x, y, txt, color=TEXT_H, fs=7.5, ha="left", va="center", bold=False):
    fw = "bold" if bold else "normal"
    ax.text(x, y, txt, color=color, fontsize=fs, ha=ha, va=va,
            fontfamily="monospace", fontweight=fw, zorder=8)

def ref_label(x, y, ref, val, color=COMP_HL):
    ax.text(x, y+0.18, ref, color=color,  fontsize=6.5, ha="center", va="bottom",
            fontfamily="monospace", fontweight="bold", zorder=8)
    ax.text(x, y-0.18, val, color=TEXT_S, fontsize=6.0, ha="center", va="top",
            fontfamily="monospace", zorder=8)

def section_box(x, y, w, h, title, color="#1a2a1a"):
    ax.add_patch(FancyBboxPatch((x, y), w, h,
        boxstyle="round,pad=0.1", linewidth=1.2,
        edgecolor=BORDER, facecolor=color, zorder=1))
    ax.text(x+0.18, y+h-0.18, title, color=TEXT_S,
            fontsize=8, fontfamily="monospace", va="top", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# COMPONENT DRAW FUNCTIONS
# ══════════════════════════════════════════════════════════════════════════════

def draw_resistor(cx, cy, angle=0, ref="R?", val="10k"):
    """Standard rectangular resistor body on a wire"""
    import matplotlib.transforms as transforms
    W, H = 0.55, 0.22
    t = ax.transData
    rot = transforms.Affine2D().rotate_deg_around(cx, cy, angle) + t

    if angle == 0:
        # horizontal leads
        ax.plot([cx-0.5, cx-W/2], [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx+W/2, cx+0.5], [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
        body = plt.Rectangle((cx-W/2, cy-H/2), W, H,
                              linewidth=1.2, edgecolor=COMP_HL, facecolor=COMP, zorder=4)
        ax.add_patch(body)
        # coloured bands
        for i, bc in enumerate(["#f0c040","#50c878","#c87df0","#888"]):
            bx = cx - W/2 + 0.08 + i*0.12
            ax.plot([bx, bx],[cy-H/2+0.03, cy+H/2-0.03], color=bc, lw=1.2, zorder=5)
    else:
        # vertical
        ax.plot([cx, cx], [cy-0.5, cy-H/2], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx, cx], [cy+H/2, cy+0.5], color=NET_SIG, lw=1.4, zorder=3)
        body = plt.Rectangle((cx-H/2, cy-W/2), H, W,
                              linewidth=1.2, edgecolor=COMP_HL, facecolor=COMP, zorder=4)
        ax.add_patch(body)
        for i, bc in enumerate(["#f0c040","#50c878","#c87df0","#888"]):
            by = cy - W/2 + 0.08 + i*0.12
            ax.plot([cx-H/2+0.03, cx+H/2-0.03],[by, by], color=bc, lw=1.2, zorder=5)

    ref_label(cx, cy + (0.42 if angle==0 else 0.5), ref, val)

def draw_capacitor(cx, cy, angle=0, ref="C?", val="100n", polar=False):
    W = 0.22
    if angle == 0:
        ax.plot([cx-0.5, cx-W/2], [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx+W/2, cx+0.5], [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
        # plates
        ax.plot([cx-W/2, cx-W/2], [cy-0.28, cy+0.28], color=COMP_HL, lw=2.5, zorder=5)
        plate2_x = cx + W/2
        if polar:
            ax.add_patch(FancyBboxPatch((plate2_x-0.02, cy-0.28), 0.2, 0.56,
                boxstyle="round,pad=0.02", facecolor=COMP, edgecolor=COMP_HL, lw=1.5, zorder=4))
            ax.text(cx-0.35, cy+0.34, "+", color=NET_PWR, fontsize=9, fontweight="bold", zorder=8)
        else:
            ax.plot([plate2_x, plate2_x], [cy-0.28, cy+0.28], color=COMP_HL, lw=2.5, zorder=5)
    else:
        ax.plot([cx, cx], [cy-0.5, cy-W/2], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx, cx], [cy+W/2, cy+0.5], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx-0.28, cx+0.28], [cy-W/2, cy-W/2], color=COMP_HL, lw=2.5, zorder=5)
        if polar:
            ax.add_patch(FancyBboxPatch((cx-0.28, cy+W/2-0.02), 0.56, 0.2,
                boxstyle="round,pad=0.02", facecolor=COMP, edgecolor=COMP_HL, lw=1.5, zorder=4))
            ax.text(cx+0.30, cy-0.42, "+", color=NET_PWR, fontsize=9, fontweight="bold", zorder=8)
        else:
            ax.plot([cx-0.28, cx+0.28], [cy+W/2, cy+W/2], color=COMP_HL, lw=2.5, zorder=5)

    ref_label(cx, cy + (0.52 if angle==0 else 0.60), ref, val)

def draw_inductor(cx, cy, ref="L?", val="10uH"):
    n_humps = 4
    t  = np.linspace(0, np.pi*n_humps, 200)
    xs = cx - 0.5 + t / (np.pi * n_humps)
    ys = cy + 0.18 * np.sin(t)
    ax.plot(xs, ys, color=COMP_HL, lw=2, zorder=4)
    ax.plot([cx-0.5, xs[0]],  [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
    ax.plot([xs[-1], cx+0.5], [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
    ref_label(cx, cy+0.45, ref, val)

def draw_diode(cx, cy, angle=0, ref="D?", val="1N4148", color=COMP_HL):
    tri = np.array([[-0.22, 0], [0.22, 0.22], [0.22, -0.22]])
    if angle == 90:
        R = np.array([[0,-1],[1,0]])
        tri = tri @ R.T
    tri += [cx, cy]
    poly = plt.Polygon(tri, closed=True, facecolor=color, edgecolor=COMP_HL, lw=1.5, zorder=4)
    ax.add_patch(poly)
    # cathode bar
    if angle == 0:
        ax.plot([cx+0.22, cx+0.22], [cy-0.22, cy+0.22], color=COMP_HL, lw=2.5, zorder=5)
        ax.plot([cx-0.5,  cx-0.22], [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx+0.22, cx+0.5],  [cy, cy], color=NET_SIG, lw=1.4, zorder=3)
    else:
        ax.plot([cx-0.22, cx+0.22], [cy+0.22, cy+0.22], color=COMP_HL, lw=2.5, zorder=5)
        ax.plot([cx, cx], [cy-0.5, cy-0.22], color=NET_SIG, lw=1.4, zorder=3)
        ax.plot([cx, cx], [cy+0.22, cy+0.5], color=NET_SIG, lw=1.4, zorder=3)
    ref_label(cx + (0 if angle==90 else 0), cy + (0.50 if angle==0 else 0.55), ref, val)

def draw_led(cx, cy, led_color="#ff4444", ref="LED1", val="RED"):
    draw_diode(cx, cy, ref=ref, val=val, color=led_color)
    # light rays
    for ang, dx, dy in [(45,0.3,0.3),(30,0.15,0.38),(-30,0.38,0.15)]:
        ax.annotate("", xy=(cx+dx, cy+dy), xytext=(cx+dx-0.12, cy+dy-0.12),
                    arrowprops=dict(arrowstyle="-|>", color=led_color,
                                   lw=1, mutation_scale=6), zorder=6)

def draw_transistor_npn(cx, cy, ref="Q?", val="2N3904"):
    # Collector, Base, Emitter
    r = 0.38
    circle = plt.Circle((cx, cy), r, linewidth=1.5, edgecolor=COMP_HL,
                         facecolor=COMP, zorder=4)
    ax.add_patch(circle)
    # base line
    ax.plot([cx-0.5, cx-r*0.6], [cy, cy], color=NET_SIG, lw=1.4, zorder=5)
    ax.plot([cx-r*0.6, cx-r*0.6], [cy-0.25, cy+0.25], color=COMP_HL, lw=2.5, zorder=5)
    # collector
    ax.plot([cx-r*0.6, cx+0.3], [cy+0.25, cy+0.5], color=NET_SIG, lw=1.4, zorder=5)
    ax.annotate("", xy=(cx+0.3, cy+0.5),
                xytext=(cx+0.05, cy+0.32),
                arrowprops=dict(arrowstyle="-|>", color=COMP_HL, lw=1.2, mutation_scale=8), zorder=5)
    # emitter + arrow
    ax.plot([cx-r*0.6, cx+0.3], [cy-0.25, cy-0.5], color=NET_SIG, lw=1.4, zorder=5)
    ax.annotate("", xy=(cx+0.28, cy-0.48),
                xytext=(cx+0.05, cy-0.30),
                arrowprops=dict(arrowstyle="-|>", color=NET_SIG, lw=1.2, mutation_scale=8), zorder=5)
    # labels
    ax.text(cx+0.35, cy+0.55, "C", color=TEXT_S, fontsize=7, fontfamily="monospace", zorder=8)
    ax.text(cx-0.65, cy,      "B", color=TEXT_S, fontsize=7, fontfamily="monospace", zorder=8)
    ax.text(cx+0.35, cy-0.58, "E", color=TEXT_S, fontsize=7, fontfamily="monospace", zorder=8)
    ref_label(cx, cy+0.62, ref, val)

def draw_ldo(cx, cy, ref="U1", val="AMS1117-3.3"):
    W, H = 1.6, 0.9
    body = FancyBboxPatch((cx-W/2, cy-H/2), W, H,
        boxstyle="round,pad=0.05", linewidth=2,
        edgecolor=COMP_HL, facecolor="#1a3a2a", zorder=4)
    ax.add_patch(body)
    ax.text(cx, cy+0.12, ref,  color=COMP_HL, fontsize=8, fontweight="bold",
            ha="center", fontfamily="monospace", zorder=8)
    ax.text(cx, cy-0.12, val, color=TEXT_S,  fontsize=6.5,
            ha="center", fontfamily="monospace", zorder=8)
    # pins: IN, OUT, ADJ/GND
    pin_labels = [("IN", cx-W/2, cy+0.2), ("OUT", cx+W/2, cy+0.2),
                  ("GND", cx-W/2+0.3, cy-H/2)]
    for pl, px, py in pin_labels:
        ax.text(px + (0.08 if "IN" in pl else (-0.08 if "OUT" in pl else 0)),
                py + (0 if "GND" not in pl else -0.15),
                pl, color=NET_GND if "GND" in pl else NET_PWR,
                fontsize=6, fontfamily="monospace", ha="center", va="center", zorder=8)
    # leads
    ax.plot([cx-W/2-0.5, cx-W/2], [cy, cy],    color=NET_PWR, lw=1.8, zorder=3)
    ax.plot([cx+W/2, cx+W/2+0.5], [cy, cy],    color=NET_PWR, lw=1.8, zorder=3)
    ax.plot([cx, cx],             [cy-H/2, cy-H/2-0.4], color=NET_GND, lw=1.8, zorder=3)

def draw_crystal(cx, cy, ref="Y1", val="8MHz"):
    W, H = 0.3, 0.55
    outer = plt.Rectangle((cx-W/2-0.1, cy-H/2-0.05), W+0.2, H+0.1,
                           linewidth=1, edgecolor=TEXT_S, facecolor="none", zorder=4)
    inner = plt.Rectangle((cx-W/2, cy-H/2), W, H,
                           linewidth=1.5, edgecolor=COMP_HL, facecolor=COMP, zorder=5)
    ax.add_patch(outer); ax.add_patch(inner)
    ax.plot([cx-0.5, cx-W/2-0.1], [cy, cy], color=NET_CLK, lw=1.4, zorder=3)
    ax.plot([cx+W/2+0.1, cx+0.5], [cy, cy], color=NET_CLK, lw=1.4, zorder=3)
    ref_label(cx, cy+0.5, ref, val)

def draw_ic_chip(cx, cy, w, h, ref, val, left_pins, right_pins):
    """Generic IC box with named pins on each side"""
    body = FancyBboxPatch((cx-w/2, cy-h/2), w, h,
        boxstyle="round,pad=0.08", linewidth=2,
        edgecolor=COMP_HL, facecolor="#1a1a2e", zorder=4)
    ax.add_patch(body)
    # pin 1 marker
    ax.add_patch(plt.Circle((cx-w/2+0.12, cy+h/2-0.12), 0.06, color=COMP_HL, zorder=6))
    ax.text(cx, cy + h/2 - 0.22, ref, color=COMP_HL, fontsize=9,
            ha="center", fontweight="bold", fontfamily="monospace", zorder=8)
    ax.text(cx, cy + h/2 - 0.48, val, color=TEXT_S, fontsize=7,
            ha="center", fontfamily="monospace", zorder=8)
    pitch = (h - 0.8) / max(len(left_pins)-1, 1)
    for i, (name, net_c) in enumerate(left_pins):
        py = cy + h/2 - 0.55 - i*pitch
        ax.plot([cx-w/2-0.5, cx-w/2], [py, py], color=net_c, lw=1.4, zorder=3)
        ax.text(cx-w/2+0.12, py, name, color=net_c, fontsize=6.5,
                fontfamily="monospace", va="center", zorder=8)
        ax.add_patch(plt.Rectangle((cx-w/2-0.12, py-0.07), 0.12, 0.14,
                                    color=COMP, zorder=5))
    for i, (name, net_c) in enumerate(right_pins):
        py = cy + h/2 - 0.55 - i*pitch
        ax.plot([cx+w/2, cx+w/2+0.5], [py, py], color=net_c, lw=1.4, zorder=3)
        ax.text(cx+w/2-0.12, py, name, color=net_c, fontsize=6.5,
                fontfamily="monospace", va="center", ha="right", zorder=8)
        ax.add_patch(plt.Rectangle((cx+w/2, py-0.07), 0.12, 0.14,
                                    color=COMP, zorder=5))

def draw_connector(cx, cy, n_pins, ref="J?", val="2.54mm", horizontal=True):
    PH = 0.44
    W  = 0.5
    total_h = n_pins * PH
    body = FancyBboxPatch((cx-W/2, cy-total_h/2), W, total_h,
        boxstyle="round,pad=0.05", linewidth=1.5,
        edgecolor=COMP_HL, facecolor="#2a1a1a", zorder=4)
    ax.add_patch(body)
    ax.text(cx, cy+total_h/2+0.22, ref, color=COMP_HL, fontsize=7,
            ha="center", fontfamily="monospace", fontweight="bold", zorder=8)
    ax.text(cx, cy+total_h/2+0.04, val, color=TEXT_S, fontsize=6,
            ha="center", fontfamily="monospace", zorder=8)
    pins = []
    for i in range(n_pins):
        py = cy + total_h/2 - PH/2 - i*PH
        square = plt.Rectangle((cx-0.1, py-0.1), 0.2, 0.2,
                                 linewidth=1, edgecolor=COMP_HL, facecolor="#888", zorder=5)
        ax.add_patch(square)
        ax.text(cx-W/2+0.06, py, f"{i+1}", color=TEXT_S, fontsize=5.5,
                fontfamily="monospace", va="center", zorder=8)
        ax.plot([cx+W/2, cx+W/2+0.4], [py, py], color=NET_SIG, lw=1.2, zorder=3)
        pins.append((cx+W/2+0.4, py))
    return pins

def draw_gnd_symbol(x, y, color=NET_GND):
    ax.plot([x, x], [y, y-0.2], color=color, lw=1.8, zorder=6)
    for i, w in enumerate([0.28, 0.18, 0.08]):
        yy = y - 0.20 - i*0.12
        ax.plot([x-w, x+w], [yy, yy], color=color, lw=1.8, zorder=6)

def draw_vcc_symbol(x, y, label_txt="VCC", color=NET_PWR):
    ax.plot([x, x], [y, y+0.3], color=color, lw=1.8, zorder=6)
    ax.annotate("", xy=(x, y+0.5), xytext=(x, y+0.28),
                arrowprops=dict(arrowstyle="-|>", color=color, lw=1.5, mutation_scale=10), zorder=6)
    ax.text(x, y+0.58, label_txt, color=color, fontsize=7,
            ha="center", fontfamily="monospace", fontweight="bold", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# TITLE BLOCK
# ══════════════════════════════════════════════════════════════════════════════
ax.add_patch(FancyBboxPatch((0.3, 26.5), 21.4, 1.2,
    boxstyle="round,pad=0.08", linewidth=2,
    edgecolor=COMP_HL, facecolor="#0f1a0f", zorder=2))
ax.text(11, 27.2, "ESP32  POWER + SENSOR INTERFACE BOARD  —  SCHEMATIC",
        color=COMP_HL, fontsize=13, ha="center", fontfamily="monospace",
        fontweight="bold", zorder=8)
ax.text(11, 26.75,
        "REV 1.0   |   VCC = 5 V (USB) → 3V3 (LDO)   |   I2C / SPI / UART   |   Pull-up Network   |   EMC Bypass",
        color=TEXT_S, fontsize=8.5, ha="center", fontfamily="monospace", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 1 — POWER INPUT + LDO (top-left)
# ══════════════════════════════════════════════════════════════════════════════
section_box(0.4, 20.5, 6.2, 5.7, "POWER SUPPLY", "#0f1a0f")

# USB / 5V input connector
draw_connector(1.2, 25.0, 2, ref="J1", val="USB-B")

# Fuse
label(2.5, 25.3, "F1 / 500mA", color=TEXT_S, fs=6.5)
ax.add_patch(FancyBboxPatch((2.4, 25.14), 0.7, 0.18,
    boxstyle="round,pad=0.03", lw=1.2, edgecolor=COMP_HL, facecolor=COMP, zorder=4))
wire(1.6, 25.08, 2.4, 25.08, NET_PWR)
wire(3.1, 25.08, 3.5, 25.08, NET_PWR)
wire(1.6, 24.64, 2.0, 24.64, NET_GND)
draw_gnd_symbol(2.0, 24.64, NET_GND)

# Bulk cap input
draw_capacitor(3.9, 25.08, angle=90, ref="C1", val="100µF", polar=True)
wire(3.5, 25.08, 3.62, 25.08, NET_PWR)
wire(4.18, 25.08, 4.5, 25.08, NET_PWR)
draw_gnd_symbol(3.9, 24.56, NET_GND)
wire(3.9, 24.56, 3.9, 24.82, NET_GND)

# Schottky diode (reverse-polarity protection)
draw_diode(4.5, 25.08, ref="D1", val="SS14", color="#884488")
junction(4.5, 25.08, NET_PWR)
wire(4.72, 25.08, 5.2, 25.08, NET_PWR)

# LDO
draw_ldo(5.8, 24.6, ref="U2", val="AMS1117-3.3")
wire(5.2, 25.08, 4.98, 25.08, NET_PWR)
wire(4.98, 25.08, 4.98, 24.6)
wire(4.98, 24.6, 5.0, 24.6, NET_PWR)

# Output caps
draw_capacitor(5.5, 23.5, angle=90, ref="C2", val="10µF", polar=True)
draw_capacitor(6.2, 23.5, angle=90, ref="C3", val="100nF")
draw_capacitor(6.9, 23.5, angle=90, ref="C4", val="10nF")
for cx_cap in [5.5, 6.2, 6.9]:
    wire(cx_cap, 23.72, cx_cap, 24.15, NET_PWR)
    draw_gnd_symbol(cx_cap, 23.28, NET_GND)
    wire(cx_cap, 23.28, cx_cap, 23.5-0.22, NET_GND)

# 3V3 bus wire
wire_path([(5.5, 24.15), (5.5, 24.3), (6.9, 24.3), (6.9, 24.15)], NET_PWR)
wire_path([(5.5, 24.3), (5.5, 24.6), (5.0, 24.6)], NET_PWR)
wire(6.9, 24.3, 7.2, 24.3, NET_PWR)
junction(5.5, 24.3, NET_PWR)

draw_vcc_symbol(5.8+0.8, 25.15, "3V3", NET_PWR)
ax.text(4.2, 24.85, "5V BUS", color=NET_PWR, fontsize=6.5,
        fontfamily="monospace", fontweight="bold", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 2 — ESP32 MODULE (centre)
# ══════════════════════════════════════════════════════════════════════════════
section_box(7.0, 18.8, 8.0, 7.5, "ESP32-WROOM-32  MODULE", "#0a1a0a")

esp_left = [
    ("GND",    NET_GND), ("3V3",    NET_PWR), ("EN",     NET_SIG),
    ("IO34",   NET_SIG), ("IO35",   NET_SIG), ("IO32",   NET_SIG),
    ("IO33",   NET_SIG), ("IO25",   NET_SIG), ("IO26",   NET_SIG),
    ("IO27",   NET_SIG), ("IO14",   NET_CLK), ("IO12",   NET_CLK),
    ("IO13",   NET_CLK), ("SD2",    NET_SIG), ("SD3",    NET_SIG),
]
esp_right = [
    ("GND",    NET_GND), ("IO23",   NET_I2C), ("IO22",   NET_I2C),
    ("TX0",    NET_SIG), ("RX0",    NET_SIG), ("IO21",   NET_I2C),
    ("IO19",   NET_CLK), ("IO18",   NET_CLK), ("IO5",    NET_CLK),
    ("IO17",   NET_SIG), ("IO16",   NET_SIG), ("IO4",    NET_SIG),
    ("IO0",    NET_SIG), ("IO2",    NET_SIG), ("IO15",   NET_SIG),
]
draw_ic_chip(11.0, 22.5, 3.2, 6.8, "ESP32", "WROOM-32",
             esp_left, esp_right)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 3 — RESET CIRCUIT (top-right of ESP32)
# ══════════════════════════════════════════════════════════════════════════════
section_box(15.2, 23.0, 3.8, 3.2, "RESET CIRCUIT", "#1a0f0f")
draw_resistor(16.0, 25.6, angle=90, ref="R1", val="10kΩ")
ax.text(15.6, 26.2, "EN", color=NET_SIG, fontsize=7, fontfamily="monospace", zorder=8)
draw_capacitor(17.2, 25.1, angle=90, ref="C5", val="100nF")
draw_vcc_symbol(16.0, 26.2, "3V3", NET_PWR)
wire(16.0, 25.1, 17.2, 25.1, NET_SIG)
draw_gnd_symbol(17.2, 24.58, NET_GND)
wire(17.2, 24.58, 17.2, 24.88, NET_GND)
# Push button
ax.add_patch(FancyBboxPatch((15.5, 25.0), 0.5, 0.22,
    boxstyle="round,pad=0.04", lw=1.5, edgecolor=COMP_HL, facecolor=COMP, zorder=4))
ax.text(15.75, 24.82, "SW1\nRESET", color=TEXT_S, fontsize=6,
        ha="center", fontfamily="monospace", zorder=8)
wire(15.5, 25.11, 16.0, 25.11, NET_SIG)
draw_gnd_symbol(15.1, 25.11, NET_GND)
wire(15.1, 25.11, 15.5, 25.11, NET_GND)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 4 — I2C PULL-UP + SENSOR HEADER
# ══════════════════════════════════════════════════════════════════════════════
section_box(15.2, 19.5, 5.8, 3.3, "I²C BUS + SENSOR HEADER", "#0a0a1a")
draw_resistor(16.0, 22.2, ref="R2", val="4.7kΩ")
draw_resistor(16.0, 21.5, ref="R3", val="4.7kΩ")
draw_vcc_symbol(16.0, 22.7, "3V3", NET_PWR)
wire(16.0, 22.7-0.3, 16.0, 22.2+0.5, NET_PWR)
wire(16.0, 21.5+0.5, 16.0, 21.75, NET_PWR)
wire_path([(16.0, 21.5+0.5), (16.0, 21.75)], NET_PWR)
ax.text(16.6, 22.2, "SDA", color=NET_I2C, fontsize=7, fontfamily="monospace", zorder=8)
ax.text(16.6, 21.5, "SCL", color=NET_I2C, fontsize=7, fontfamily="monospace", zorder=8)
j_pins = draw_connector(19.5, 21.5, 4, ref="J2", val="I2C HDR")
i2c_labels = ["VCC","GND","SDA","SCL"]
i2c_colors = [NET_PWR, NET_GND, NET_I2C, NET_I2C]
for (px, py), lbl, col in zip(j_pins, i2c_labels, i2c_colors):
    ax.text(px+0.1, py, lbl, color=col, fontsize=7,
            fontfamily="monospace", va="center", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 5 — SPI HEADER
# ══════════════════════════════════════════════════════════════════════════════
section_box(0.4, 16.0, 5.0, 4.2, "SPI BUS + HEADER", "#0a0a12")
j3_pins = draw_connector(2.0, 18.5, 6, ref="J3", val="SPI HDR")
spi_labels = ["VCC","GND","MOSI","MISO","SCK","CS0"]
spi_colors = [NET_PWR, NET_GND, NET_CLK, NET_CLK, NET_CLK, NET_SIG]
for (px, py), lbl, col in zip(j3_pins, spi_labels, spi_colors):
    ax.text(px+0.1, py, lbl, color=col, fontsize=7,
            fontfamily="monospace", va="center", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 6 — STATUS LED + DRIVER TRANSISTOR
# ══════════════════════════════════════════════════════════════════════════════
section_box(0.4, 12.5, 5.0, 3.2, "STATUS LED DRIVER", "#1a0a0a")
draw_resistor(1.8, 15.3, ref="R4", val="330Ω")
draw_led(3.2, 15.3, led_color="#22ee44", ref="LED1", val="GREEN")
draw_vcc_symbol(4.4, 15.5, "3V3", NET_PWR)
wire(3.42, 15.3, 4.4, 15.3, NET_PWR)
draw_resistor(1.5, 14.2, ref="R5", val="1kΩ")
draw_transistor_npn(2.55, 13.8, ref="Q1", val="2N3904")
wire(1.05, 14.2, 1.3, 14.2, NET_SIG)
ax.text(0.9, 14.2, "IO2", color=NET_SIG, fontsize=7, fontfamily="monospace",
        va="center", zorder=8)
wire(2.0, 14.2, 2.05, 14.2, NET_SIG)
wire(2.55, 14.3, 2.55, 15.08, NET_SIG)
wire(2.55, 15.08, 2.7, 15.08, NET_SIG)
draw_gnd_symbol(2.55, 13.28, NET_GND)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 7 — MOTOR / RELAY FLYBACK DIODE
# ══════════════════════════════════════════════════════════════════════════════
section_box(5.8, 12.5, 5.5, 3.2, "RELAY DRIVER + FLYBACK", "#1a0a1a")
draw_transistor_npn(7.2, 14.5, ref="Q2", val="TIP120")
draw_inductor(8.2, 15.3, ref="RL1", val="RELAY")
draw_diode(8.2, 15.8, angle=90, ref="D2", val="1N4007", color="#884444")
draw_vcc_symbol(8.2, 16.4, "12V", NET_PWR)
wire(8.2, 16.1, 8.2, 16.18, NET_PWR)
draw_gnd_symbol(7.2, 13.98, NET_GND)
draw_resistor(6.1, 14.5, ref="R6", val="2.2kΩ")
ax.text(5.6, 14.5, "IO4", color=NET_SIG, fontsize=7, fontfamily="monospace",
        va="center", zorder=8)
wire(5.5, 14.5, 5.6, 14.5, NET_SIG)
wire(6.62, 14.5, 6.7, 14.5, NET_SIG)
wire(7.2, 15.0, 7.2, 15.08, NET_SIG)
wire(7.2, 15.08, 7.7, 15.08, NET_SIG)
wire(7.7, 15.08, 7.7, 15.3)
wire(7.7, 15.3, 7.7, 15.58, NET_SIG)
junction(8.2, 15.58, NET_SIG)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 8 — CRYSTAL / OSCILLATOR
# ══════════════════════════════════════════════════════════════════════════════
section_box(11.8, 12.5, 4.5, 3.2, "CLOCK / CRYSTAL", "#0a1212")
draw_crystal(13.5, 14.5, ref="Y1", val="40MHz")
draw_capacitor(12.5, 13.6, angle=90, ref="C6", val="22pF")
draw_capacitor(14.5, 13.6, angle=90, ref="C7", val="22pF")
draw_gnd_symbol(12.5, 13.08, NET_GND)
draw_gnd_symbol(14.5, 13.08, NET_GND)
wire(12.5, 13.32, 12.5, 14.5-0.5, NET_CLK)
wire(12.5, 14.5-0.5, 13.0, 14.5-0.5, NET_CLK)
wire(13.0, 14.5-0.5, 13.0, 14.5, NET_CLK)
wire(14.5, 13.32, 14.5, 14.5-0.5, NET_CLK)
wire(14.5, 14.5-0.5, 14.0, 14.5-0.5, NET_CLK)
wire(14.0, 14.5-0.5, 14.0, 14.5, NET_CLK)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 9 — EMC / FILTER NETWORK
# ══════════════════════════════════════════════════════════════════════════════
section_box(16.4, 12.5, 5.0, 3.2, "EMC FILTER NETWORK", "#12120a")
draw_inductor(17.5, 15.2, ref="L1", val="10µH")
draw_capacitor(18.8, 14.5, angle=90, ref="C8", val="470pF")
draw_capacitor(19.8, 14.5, angle=90, ref="C9", val="100nF")
draw_capacitor(20.8, 14.5, angle=90, ref="C10", val="10nF")
for xc in [18.8, 19.8, 20.8]:
    draw_gnd_symbol(xc, 13.98, NET_GND)
    wire(xc, 13.98, xc, 14.28, NET_GND)
wire(18.0, 15.2, 18.8, 15.2, NET_SIG)
wire_path([(18.8, 15.2),(18.8, 14.72),(19.8, 14.72),(20.8, 14.72)], NET_SIG)
junction(18.8, 15.2, NET_SIG)
junction(19.8, 14.72, NET_SIG)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 10 — UART / PROGRAMMING HEADER
# ══════════════════════════════════════════════════════════════════════════════
section_box(5.8, 8.5, 5.5, 3.8, "UART / PROG HEADER", "#12080a")
j4_pins = draw_connector(7.5, 10.5, 6, ref="J4", val="UART/PROG")
uart_labels = ["VCC","GND","TX","RX","IO0","RESET"]
uart_colors = [NET_PWR, NET_GND, NET_SIG, NET_SIG, NET_SIG, NET_SIG]
for (px, py), lbl, col in zip(j4_pins, uart_labels, uart_colors):
    ax.text(px+0.1, py, lbl, color=col, fontsize=7,
            fontfamily="monospace", va="center", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# SECTION 11 — ADC INPUT PROTECTION
# ══════════════════════════════════════════════════════════════════════════════
section_box(11.8, 8.5, 5.0, 3.8, "ADC INPUT PROTECTION", "#0a080f")
for i, (rio, rpin) in enumerate([("R7","IO34"),("R8","IO35"),("R9","IO32")]):
    ry = 11.8 - i * 1.0
    draw_resistor(13.2, ry, ref=rio, val="10kΩ")
    draw_diode(14.6, ry, ref=f"D{3+i}", val="BAT54", color="#446688")
    ax.text(12.5, ry, rpin, color=NET_SIG, fontsize=7,
            fontfamily="monospace", va="center", zorder=8)
    draw_gnd_symbol(15.2, ry, NET_GND)
    wire(15.08, ry, 15.2, ry, NET_GND)

# ══════════════════════════════════════════════════════════════════════════════
# LEGEND BOX
# ══════════════════════════════════════════════════════════════════════════════
legend_x, legend_y = 0.4, 7.8
ax.add_patch(FancyBboxPatch((legend_x, legend_y - 3.5), 21.2, 3.5,
    boxstyle="round,pad=0.1", lw=1.5, edgecolor=COMP_HL, facecolor="#0c0c14", zorder=2))
ax.text(11, legend_y - 0.25, "NET LEGEND", color=COMP_HL, fontsize=9,
        ha="center", fontfamily="monospace", fontweight="bold", zorder=8)

legend_items = [
    (NET_PWR, "Power Net (VCC / 3V3 / 5V / 12V)"),
    (NET_GND, "Ground Net"),
    (NET_SIG, "Signal / GPIO Net"),
    (NET_CLK, "Clock / SPI Net"),
    (NET_I2C, "I²C Bus Net (SDA / SCL)"),
    (COMP_HL, "Component Body / Reference"),
    (TEXT_S,  "Pin Label / Value"),
]
for i, (col, lbl) in enumerate(legend_items):
    col_x = legend_x + 0.5 + (i % 4) * 5.3
    col_y = legend_y - 1.0 - (i // 4) * 0.8
    ax.plot([col_x, col_x + 0.6], [col_y, col_y], color=col, lw=2.5, zorder=8)
    ax.text(col_x + 0.75, col_y, lbl, color=col, fontsize=7.5,
            fontfamily="monospace", va="center", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# BILL OF MATERIALS TABLE
# ══════════════════════════════════════════════════════════════════════════════
bom_x, bom_y = 0.4, 5.8
ax.add_patch(FancyBboxPatch((bom_x, bom_y - 5.5), 21.2, 5.5,
    boxstyle="round,pad=0.1", lw=1.5, edgecolor=COMP_HL, facecolor="#080c08", zorder=2))
ax.text(11, bom_y - 0.25, "BILL OF MATERIALS (BOM)", color=COMP_HL, fontsize=9,
        ha="center", fontfamily="monospace", fontweight="bold", zorder=8)

bom_headers = ["Ref", "Value", "Part", "Package", "Qty", "Notes"]
bom_data = [
    ["U1",    "ESP32-WROOM-32",  "ESP32",       "SMD-38",       "1",  "Main MCU, WiFi+BT"],
    ["U2",    "AMS1117-3.3",     "LDO Reg",     "SOT-223",      "1",  "800mA 3.3V output"],
    ["C1",    "100µF/16V",       "Electrolytic","ø8×12mm",      "1",  "Bulk input filter"],
    ["C2",    "10µF/10V",        "Tantalum",    "Case-A",       "1",  "LDO output filter"],
    ["C3–C4", "100nF / 10nF",    "MLCC",        "0402",         "2",  "Bypass capacitors"],
    ["C5",    "100nF",           "MLCC",        "0402",         "1",  "EN pin debounce"],
    ["C6–C7", "22pF",            "MLCC NP0",    "0402",         "2",  "Crystal load caps"],
    ["C8–C10","470pF/100nF/10nF","MLCC",        "0402",         "3",  "EMC π-filter"],
    ["R1",    "10kΩ",            "Resistor",    "0402",         "1",  "EN pull-up"],
    ["R2–R3", "4.7kΩ",           "Resistor",    "0402",         "2",  "I²C pull-ups"],
    ["R4",    "330Ω",            "Resistor",    "0402",         "1",  "LED current limit"],
    ["R5",    "1kΩ",             "Resistor",    "0402",         "1",  "Q1 base resistor"],
    ["R6",    "2.2kΩ",           "Resistor",    "0402",         "1",  "Q2 base resistor"],
    ["R7–R9", "10kΩ",            "Resistor",    "0402",         "3",  "ADC series protect"],
    ["D1",    "SS14",            "Schottky",    "SMA",          "1",  "Reverse polarity"],
    ["D2",    "1N4007",          "Rectifier",   "DO-41",        "1",  "Relay flyback"],
    ["D3–D5", "BAT54",           "Schottky",    "SOD-323",      "3",  "ADC clamp diodes"],
    ["LED1",  "Green 5mm",       "LED",         "THT",          "1",  "Status indicator"],
    ["Q1",    "2N3904",          "NPN BJT",     "TO-92",        "1",  "LED driver"],
    ["Q2",    "TIP120",          "NPN Darlingt","TO-220",        "1",  "Relay driver"],
    ["L1",    "10µH / 300mA",    "Inductor",    "1210",         "1",  "EMC choke"],
    ["Y1",    "40MHz",           "Crystal",     "SMD-2P-5×3.2", "1",  "ESP32 clock"],
    ["J1",    "USB-B",           "Connector",   "THT",          "1",  "Power input"],
    ["J2",    "4-pin 2.54mm",    "Header",      "THT",          "1",  "I²C sensor"],
    ["J3",    "6-pin 2.54mm",    "Header",      "THT",          "1",  "SPI device"],
    ["J4",    "6-pin 2.54mm",    "Header",      "THT",          "1",  "UART / prog"],
    ["F1",    "500mA PTC",       "Resettable",  "1812",         "1",  "USB fuse"],
    ["RL1",   "5V Relay",        "SPDT Relay",  "PCB mount",    "1",  "Load switching"],
    ["SW1",   "SPST Tact",       "Push button", "THT 6×6mm",    "1",  "Reset button"],
]

col_xs = [0.6, 2.3, 4.3, 6.2, 7.8, 9.0]
col_ws = [1.6, 1.9, 1.8, 1.5, 1.1, 12.0]
col_cols = [COMP_HL, NET_SIG, NET_CLK, TEXT_S, NET_PWR, TEXT_S]

# Header row
for hdr, cx_col, col in zip(bom_headers, col_xs, col_cols):
    ax.text(cx_col, bom_y - 0.65, hdr, color=COMP_HL,
            fontsize=7.5, fontfamily="monospace", fontweight="bold", zorder=8)

ax.plot([bom_x+0.1, bom_x+21.0], [bom_y-0.78, bom_y-0.78],
        color=COMP_HL, lw=0.8, zorder=6)

for row_i, row in enumerate(bom_data):
    row_y = bom_y - 0.98 - row_i * 0.145
    if row_i % 2 == 0:
        ax.add_patch(plt.Rectangle((bom_x+0.1, row_y-0.04), 21.0, 0.15,
                                    color="#0f1a0f", zorder=2))
    for val, cx_col, col in zip(row, col_xs, col_cols):
        ax.text(cx_col, row_y+0.05, val, color=col,
                fontsize=6.4, fontfamily="monospace", zorder=8)

# row count
ax.text(21.0, bom_y - 5.3, f"{len(bom_data)} line items",
        color=TEXT_S, fontsize=7, fontfamily="monospace", ha="right", zorder=8)

# ══════════════════════════════════════════════════════════════════════════════
# BORDER + CORNER MARKS
# ══════════════════════════════════════════════════════════════════════════════
for cx_, cy_ in [(0.5, 0.5),(21.5, 0.5),(0.5, 27.5),(21.5, 27.5)]:
    ax.plot(cx_, cy_, "+", color=COMP_HL, markersize=14, markeredgewidth=1.2, zorder=9)
ax.add_patch(plt.Rectangle((0.3, 0.3), 21.4, 27.4,
             linewidth=2, edgecolor=COMP_HL, facecolor="none", zorder=10))

# ══════════════════════════════════════════════════════════════════════════════
# EXPORT
# ══════════════════════════════════════════════════════════════════════════════
plt.tight_layout(pad=0)
fig.savefig("board_schematic.pdf", format="pdf", dpi=300,
            bbox_inches="tight", facecolor=BG)
fig.savefig("board_schematic.png", format="png", dpi=300,
            bbox_inches="tight", facecolor=BG)
print("✅  board_schematic.pdf")
print("✅  board_schematic.png")
plt.close(fig)

