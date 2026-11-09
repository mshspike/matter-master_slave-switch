"""Parametric case for the ESP32 Matter switch (trimesh + manifold3d, plain Python).

Two printed parts, no screws and no inserts:
  back.stl  : shell with 18650 cradle, ESP32 board pocket, button-board ledges, USB-C slot
  front.stl : lid with display window, 4 button holes, rocker cutout; its lip snaps into the back

Print: back with the open face up; front with the outside face down. PETG, 0.2 mm layers, 3 walls.
All sizes are nominal datasheet values. MEASURE YOUR PARTS and edit the PARAMETERS before printing.
Axes: X width, Y height, Z depth (z=0 is the back face, +Z points at the user).
"""
import os
import numpy as np
import trimesh


class S:
    """Tiny solid wrapper: union and cut on watertight meshes."""
    def __init__(self, mesh):
        self.m = mesh

    def union(self, o):
        return S(trimesh.boolean.union([self.m, o.m], engine="manifold"))

    def cut(self, o):
        return S(trimesh.boolean.difference([self.m, o.m], engine="manifold"))


def cyl(r, h, center, axis="z"):
    m = trimesh.creation.cylinder(radius=r, height=h, sections=48)
    if axis == "x":
        m.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [0, 1, 0]))
    m.apply_translation(center)
    return S(m)

# ---------------- PARAMETERS (mm) ----------------
W, H, D = 88.0, 132.0, 32.0      # outer size
WALL = 2.0
FP = 3.0                          # front plate thickness
TOL = 0.25                        # clearance for press fits

# 18650 protected cell (BAK N18650CK is about 70 mm long)
CELL_D, CELL_L = 18.6, 70.5
CELL_Y0, CELL_Y1 = 2.0, 25.0      # y band of the cell cradle

# ESP32 board (FireBeetle 2 ESP32-C6, nominal; USB-C on the +X end)
ESP_L, ESP_W = 58.0, 25.4
ESP_Y0 = 29.0
ESP_POST = 1.5                    # board standoff height
USB_SLOT_W, USB_SLOT_H = 10.5, 4.6

# Waveshare 2.13in e-paper HAT: board 65 x 30.2, glass window about 48.6 x 23.7
DISP_L, DISP_W = 65.0, 30.2
DISP_Y0 = 96.0
WIN_L, WIN_W = 50.0, 25.0
DISP_FRAME_H = 5.0

# Controls
BTN_Y = 66.0
BTN_X = [23.0, 37.0, 51.0, 65.0]  # centres of the four buttons
BTN_HOLE = 10.5
ROCKER_Y = 84.0
ROCKER_CUT = (20.0, 14.0)         # KCD11 mini rocker panel cutout, check your part

FL = WALL - 0.3  # features start slightly inside the floor so unions fuse
INNER_W, INNER_H = W - 2 * WALL, H - 2 * WALL
BACK_H = D - FP                   # height of the back shell
cx = W / 2.0


def box(x0, y0, z0, x1, y1, z1):
    m = trimesh.creation.box(extents=(x1 - x0, y1 - y0, z1 - z0))
    m.apply_translation(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    return S(m)


# ---------------- BACK SHELL ----------------
back = box(0, 0, 0, W, H, BACK_H).cut(box(WALL, WALL, WALL, W - WALL, H - WALL, BACK_H + 1))

# Cell cradle: two saddles, plus end blocks that carry the spring and flat contacts.
cell_cy, cell_cz = (CELL_Y0 + CELL_Y1) / 2.0, WALL + CELL_D / 2.0 + 0.4
x_cell0 = cx - CELL_L / 2.0
for xs in (x_cell0 + 12.0, x_cell0 + CELL_L - 12.0 - 4.0):
    saddle = box(xs, CELL_Y0, FL, xs + 4.0, CELL_Y1, cell_cz)
    bore = cyl(CELL_D / 2.0 + TOL, 6.0, (xs + 2.0, cell_cy, cell_cz), axis="x")
    back = back.union(saddle.cut(bore))

END_T = 3.0
for side in (0, 1):
    x0 = WALL if side == 0 else W - WALL - END_T
    blk = box(x0 - 0.3, CELL_Y0 - 0.3, FL, x0 + END_T + 0.3, CELL_Y1, cell_cz + 6.0)
    # pocket for a flat contact plate (14 wide, 1.2 deep) on the face toward the cell
    px = x0 + END_T - 1.2 if side == 0 else x0
    blk = blk.cut(box(px, cell_cy - 7, cell_cz - 7, px + 1.2, cell_cy + 7, cell_cz + 7))
    # wire hole up to the electronics
    blk = blk.cut(cyl(1.6, 12.0, (x0 + END_T / 2.0, cell_cy, cell_cz + 2.0)))
    back = back.union(blk)

# ESP32 board pocket: four corner brackets on standoffs, board is held down by the front plate.
ex1 = W - WALL - 0.3          # USB-C end sits against the +X wall
ex0 = ex1 - ESP_L
ey0, ey1 = ESP_Y0, ESP_Y0 + ESP_W
back = back.union(box(ex0 - TOL, ey0 - TOL, FL, ex1 + TOL, ey1 + TOL, WALL + ESP_POST)
                  .cut(box(ex0 + 3, ey0 + 3, WALL - 1, ex1 - 3, ey1 - 3, WALL + ESP_POST + 1)))
for (bx, by, sx, sy) in ((ex0 - TOL, ey0 - TOL, -1, -1), (ex1 + TOL, ey0 - TOL, 1, -1),
                         (ex0 - TOL, ey1 + TOL, -1, 1), (ex1 + TOL, ey1 + TOL, 1, 1)):
    bw = 1.6
    def rng(c, sgn, ln):  # extends outward from c, with 0.4 overlap back into the ring
        lo, hi = sorted((c, c + sgn * ln))
        return (lo, hi + 0.4) if sgn < 0 else (lo - 0.4, hi)
    for (xa, xb_), (ya, yb) in ((rng(bx, sx, bw), rng(by, -sy, 6)), (rng(bx, -sx, 6), rng(by, sy, bw))):
        back = back.union(box(xa, ya, FL, xb_, yb, WALL + ESP_POST + 6))

# USB-C slot in the +X wall, centred on the board
usb_y = (ey0 + ey1) / 2.0
usb_z = WALL + ESP_POST + 1.6 + 1.6
back = back.cut(box(W - WALL - 0.5, usb_y - USB_SLOT_W / 2, usb_z - USB_SLOT_H / 2,
                    W + 0.5, usb_y + USB_SLOT_W / 2, usb_z + USB_SLOT_H / 2))

# Button board ledges: a 60 x 14 mm strip of perfboard rests on two blocks, tact switches face the lid.
LEDGE_Z = D - FP - 7.3 - 1.6 - 0.6   # board underside so tact tops sit just under the lid
for xs in (cx - 34.0, cx + 30.0):
    back = back.union(box(xs, BTN_Y - 7 - 1, FL, xs + 4.0, BTN_Y + 7 + 1, LEDGE_Z))
    # lip that keeps the strip from sliding sideways
    back = back.union(box(xs + (3.6 if xs < cx else -1.2), BTN_Y - 7, LEDGE_Z - 0.3,
                          xs + (5.2 if xs < cx else 0.4), BTN_Y + 7, LEDGE_Z + 1.6))

# Snap windows for the lid bumps (left and right walls)
SNAP_Z = BACK_H - 4.0
for xw in (0, W - WALL):
    for yw in (H * 0.30, H * 0.70):
        back = back.cut(box(xw - 0.1, yw - 4, SNAP_Z - 1.5, xw + WALL + 0.1, yw + 4, SNAP_Z + 1.5))

# ---------------- FRONT LID ----------------
front = box(0, 0, 0, W, H, FP)
# Lip that enters the back shell (z from FP upward in print orientation: built in +Z, flipped later)
LIP_H, LIP_T = 5.0, 1.2
lip_out = box(WALL + TOL, WALL + TOL, FP - 0.3, W - WALL - TOL, H - WALL - TOL, FP + LIP_H)
lip_in = box(WALL + TOL + LIP_T, WALL + TOL + LIP_T, FP - 1, W - WALL - TOL - LIP_T, H - WALL - TOL - LIP_T, FP + LIP_H + 1)
front = front.union(lip_out.cut(lip_in))
# Snap bumps on the lip, outside faces, aligned with the back's windows
for xb, d in ((WALL + TOL - 0.5, 1), (W - WALL - TOL - 0.3, -1)):
    for yw in (H * 0.30, H * 0.70):
        front = front.union(box(xb, yw - 3.5, FP + LIP_H - 2.0, xb + 0.8, yw + 3.5, FP + LIP_H - 0.6))

# Window, button holes, rocker cutout (through the plate)
disp_cy = DISP_Y0 + DISP_W / 2.0
front = front.cut(box(cx - WIN_L / 2, disp_cy - WIN_W / 2, -1, cx + WIN_L / 2, disp_cy + WIN_W / 2, FP + 1))
for bx in BTN_X:
    front = front.cut(cyl(BTN_HOLE / 2, FP + 2, (bx + (cx - 44.0), BTN_Y, FP / 2)))
front = front.cut(box(cx - ROCKER_CUT[0] / 2, ROCKER_Y - ROCKER_CUT[1] / 2, -1,
                      cx + ROCKER_CUT[0] / 2, ROCKER_Y + ROCKER_CUT[1] / 2, FP + 1))

# Display frame on the inside face, with two flexing hooks on the long edges
fx0, fx1 = cx - DISP_L / 2 - TOL, cx + DISP_L / 2 + TOL
fy0, fy1 = DISP_Y0 - TOL, DISP_Y0 + DISP_W + TOL
frame = (box(fx0 - 1.2, fy0 - 1.2, FP - 0.3, fx1 + 1.2, fy1 + 1.2, FP + DISP_FRAME_H)
         .cut(box(fx0, fy0, FP - 1, fx1, fy1, FP + DISP_FRAME_H + 1)))
# leave the short edges partly open so the cable and header have room
frame = frame.cut(box(fx0 - 2, fy0 + 6, FP, fx0 + 1, fy1 - 6, FP + DISP_FRAME_H + 1))
front = front.union(frame)
for (ya, yb) in ((fy0 - 0.3, fy0 + 1.0), (fy1 - 1.0, fy1 + 0.3)):
    front = front.union(box(cx - 4, ya, FP + DISP_FRAME_H - 0.6, cx + 4, yb, FP + DISP_FRAME_H + 0.8))

# Hold-down ribs: grow from the inside face of the plate and press on the cell, the ESP32 board
# and the button board. Rib length = distance from the lid underside down to the held part, minus 0.8.
lid_under = BACK_H
cell_top = cell_cz + CELL_D / 2.0
esp_top = WALL + ESP_POST + 1.6
btn_top = LEDGE_Z + 1.6
Z0 = FP - 0.3
rib_cell = lid_under - cell_top - 0.8
front = front.union(box(x_cell0 + 12.0, CELL_Y0 + 1, Z0, x_cell0 + 16.0, CELL_Y1 - 1, FP + rib_cell))
front = front.union(box(x_cell0 + CELL_L - 16.0, CELL_Y0 + 1, Z0, x_cell0 + CELL_L - 12.0, CELL_Y1 - 1, FP + rib_cell))
rib_esp = lid_under - esp_top - 0.8
for (rx, ry) in ((ex0 + 4, ey0 + 1), (ex1 - 8, ey0 + 1), (ex0 + 4, ey1 - 4), (ex1 - 8, ey1 - 4)):
    front = front.union(box(rx, ry, Z0, rx + 4, ry + 3, FP + rib_esp))
rib_btn = lid_under - btn_top - 0.8
front = front.union(box(cx - 34.0 + 4.2, BTN_Y - 7, Z0, cx - 34.0 + 6.0, BTN_Y + 7, FP + rib_btn))

# Print orientation: front plate outside face down (already z=0 is the outside face).
if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stl")
    os.makedirs(out, exist_ok=True)
    for name, part in (("back", back), ("front", front)):
        part.m.export(os.path.join(out, name + ".stl"))
        bb = part.m.bounds
        print(name, "bbox %.1f x %.1f x %.1f" % tuple(bb[1] - bb[0]),
              "volume %.1f cm3" % (part.m.volume / 1000.0), "watertight", part.m.is_watertight,
              "bodies", len(part.m.split(only_watertight=False)))
