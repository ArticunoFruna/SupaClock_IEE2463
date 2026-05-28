#!/usr/bin/env python3
"""
Generador de planos tecnicos para SupaClock V2.
Produce un PDF multi-pagina con vistas ortograficas dimensionadas.

Sale en: mechanical/supaclock_v2_blueprint.pdf
"""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyArrowPatch, Arc, FancyBboxPatch
from matplotlib.patches import PathPatch
from matplotlib.path import Path
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np

# ============================================================================
# PARAMETROS DEL CASE V2 (DEBEN coincidir con los .scad)
# ============================================================================
W = 98.0
L = 79.0
H = 25.0
r_vert = 12.0
r_chamfer = 1.5
taper = 2.0
grosor_pared = 2.0
pcb_off_x = 6.5
pcb_off_y = 6.0
pcb_thickness = 1.6
altura_base = 2.0
altura_total_bottom = altura_base + grosor_pared   # 4
altura_top = H - altura_total_bottom               # 21

standoff_od = 7.0
standoff_id = 1.8

cut_max30102 = (17.0, 22.0)
cut_max30205 = (14.0, 10.0)
elec_d = 6.0

mh = [(3.5+pcb_off_x,63.5+pcb_off_y),(81.5+pcb_off_x,63.5+pcb_off_y),
      (3.5+pcb_off_x,3.5+pcb_off_y),(81.5+pcb_off_x,3.5+pcb_off_y)]
pos_max30102 = (45.5+pcb_off_x, 33.195+pcb_off_y)
pos_max30205 = (45.0+pcb_off_x, 17.0+pcb_off_y)
electrodes = [(12.0+pcb_off_x,31.5+pcb_off_y),(65.5+pcb_off_x,32.0+pcb_off_y),
              (43.5+pcb_off_x, 4.0+pcb_off_y)]

display_center = (44.28+pcb_off_x, 38.5+pcb_off_y)
display_size = (28.0, 34.0)
btn_y_list = [15.375+pcb_off_y, 27.875+pcb_off_y]
btn_z = pcb_thickness + 1.9
btn_d = 4.0
usb_y = 48.0+pcb_off_y
usb_z = pcb_thickness + 13.0
usb_size = (10.0, 4.0)
jack_y = 16.586+pcb_off_y
jack_z = pcb_thickness + 12.6
jack_d = 6.5

lug_strap_w = 20.0
lug_thickness = 5.0
lug_protrude = 7.0
lug_z_bot = 5.0    # top-case-local
lug_z_top = 15.0
spring_bar_d = 1.8
lug_center_sep = lug_strap_w + lug_thickness   # 27

# ============================================================================
# DIBUJO - PRIMITIVAS DE INGENIERIA
# ============================================================================

LW_OUTLINE = 1.2
LW_HIDDEN  = 0.6
LW_CENTER  = 0.5
LW_DIM     = 0.5
LW_BORDER  = 1.5
FS_DIM     = 7
FS_LABEL   = 8
FS_TITLE   = 10

def setup_page(title, page_num, total_pages):
    """Inicializa una pagina A4 landscape con border y title block."""
    fig, ax = plt.subplots(figsize=(11.69, 8.27))   # A4 landscape (in)
    ax.set_xlim(0, 297)
    ax.set_ylim(0, 210)
    ax.set_aspect('equal')
    ax.set_axis_off()
    # Border
    ax.add_patch(Rectangle((5, 5), 287, 200, fill=False,
                           edgecolor='black', linewidth=LW_BORDER))
    # Title block
    tb_x, tb_y, tb_w, tb_h = 222, 5, 70, 32
    ax.add_patch(Rectangle((tb_x, tb_y), tb_w, tb_h, fill=False,
                           edgecolor='black', linewidth=1))
    # internal divisions
    ax.plot([tb_x, tb_x+tb_w],[tb_y+tb_h-8, tb_y+tb_h-8],
            'k', linewidth=0.5)
    ax.plot([tb_x, tb_x+tb_w],[tb_y+8, tb_y+8],
            'k', linewidth=0.5)
    ax.plot([tb_x+tb_w/2, tb_x+tb_w/2],[tb_y, tb_y+8],
            'k', linewidth=0.5)
    # text
    ax.text(tb_x+tb_w/2, tb_y+tb_h-4, title,
            ha='center', va='center', fontsize=FS_TITLE, fontweight='bold')
    ax.text(tb_x+2, tb_y+tb_h-12, "Proyecto: SupaClock V2",
            ha='left', va='top', fontsize=7)
    ax.text(tb_x+2, tb_y+tb_h-17, "Curso: IEE2913 Capstone PUC, Grupo 10",
            ha='left', va='top', fontsize=7)
    ax.text(tb_x+2, tb_y+tb_h-22, "Escala: 1:1 (en unidades reales);  Unidades: mm",
            ha='left', va='top', fontsize=7)
    ax.text(tb_x+2, tb_y+tb_h-27, "Tolerancia general: +/- 0.2mm (FDM)",
            ha='left', va='top', fontsize=7)
    ax.text(tb_x+tb_w/4, tb_y+4, "Material: PETG", ha='center', va='center', fontsize=7)
    ax.text(tb_x+3*tb_w/4, tb_y+4, f"Hoja {page_num}/{total_pages}",
            ha='center', va='center', fontsize=7)
    return fig, ax

def rounded_rect_outline(cx, cy, w, l, r, scale=1.0, lw=LW_OUTLINE,
                        linestyle='-', color='black'):
    """Devuelve los xs, ys de un rectangulo redondeado centrado en (cx,cy)."""
    xs, ys = [], []
    # Bottom edge (left -> right)
    xs.extend([cx-w/2+r, cx+w/2-r])
    ys.extend([cy-l/2, cy-l/2])
    # Right-bottom arc
    arc_pts = arc_points(cx+w/2-r, cy-l/2+r, r, -90, 0, n=15)
    xs.extend([p[0] for p in arc_pts]); ys.extend([p[1] for p in arc_pts])
    # Right edge
    xs.extend([cx+w/2, cx+w/2])
    ys.extend([cy-l/2+r, cy+l/2-r])
    # Right-top arc
    arc_pts = arc_points(cx+w/2-r, cy+l/2-r, r, 0, 90, n=15)
    xs.extend([p[0] for p in arc_pts]); ys.extend([p[1] for p in arc_pts])
    # Top edge
    xs.extend([cx+w/2-r, cx-w/2+r])
    ys.extend([cy+l/2, cy+l/2])
    # Left-top arc
    arc_pts = arc_points(cx-w/2+r, cy+l/2-r, r, 90, 180, n=15)
    xs.extend([p[0] for p in arc_pts]); ys.extend([p[1] for p in arc_pts])
    # Left edge
    xs.extend([cx-w/2, cx-w/2])
    ys.extend([cy+l/2-r, cy-l/2+r])
    # Left-bottom arc
    arc_pts = arc_points(cx-w/2+r, cy-l/2+r, r, 180, 270, n=15)
    xs.extend([p[0] for p in arc_pts]); ys.extend([p[1] for p in arc_pts])
    return xs, ys

def arc_points(cx, cy, r, ang1, ang2, n=20):
    angs = np.linspace(np.radians(ang1), np.radians(ang2), n)
    return [(cx + r*np.cos(a), cy + r*np.sin(a)) for a in angs]

def draw_outline(ax, xs, ys, scale, offset, lw=LW_OUTLINE,
                 linestyle='-', color='black'):
    sx = [scale*x + offset[0] for x in xs]
    sy = [scale*y + offset[1] for y in ys]
    ax.plot(sx, sy, color=color, linewidth=lw, linestyle=linestyle)

def draw_circle(ax, cx, cy, d, scale, offset, lw=LW_OUTLINE,
               linestyle='-', color='black', fill=False):
    c = Circle((scale*cx + offset[0], scale*cy + offset[1]),
               scale*d/2,
               edgecolor=color, facecolor='none' if not fill else color,
               linewidth=lw, linestyle=linestyle)
    ax.add_patch(c)

def draw_rect(ax, cx, cy, w, h, scale, offset, lw=LW_OUTLINE,
              linestyle='-', color='black'):
    r = Rectangle((scale*(cx-w/2) + offset[0], scale*(cy-h/2) + offset[1]),
                  scale*w, scale*h,
                  edgecolor=color, facecolor='none',
                  linewidth=lw, linestyle=linestyle)
    ax.add_patch(r)

def center_mark(ax, cx, cy, d, scale, offset):
    """Cruz centradora sobre un agujero."""
    x = scale*cx + offset[0]; y = scale*cy + offset[1]
    L_ = max(scale*d*0.7, 2.0)
    ax.plot([x-L_/2, x+L_/2], [y, y], color='black',
            linewidth=LW_CENTER, linestyle=(0, (4, 2, 1, 2)))
    ax.plot([x, x], [y-L_/2, y+L_/2], color='black',
            linewidth=LW_CENTER, linestyle=(0, (4, 2, 1, 2)))

def dim_h(ax, x1, x2, y, label, scale, offset, dy=8, side='above',
          ext=1.5):
    """Cota horizontal entre x1 y x2 a la altura y, label arriba o abajo."""
    sx1 = scale*x1 + offset[0]; sx2 = scale*x2 + offset[0]
    sy = scale*y + offset[1]
    sgn = 1 if side == 'above' else -1
    line_y = sy + sgn*dy
    # extension lines
    ax.plot([sx1, sx1], [sy + sgn*ext, line_y + sgn*ext], 'k', linewidth=LW_DIM)
    ax.plot([sx2, sx2], [sy + sgn*ext, line_y + sgn*ext], 'k', linewidth=LW_DIM)
    # dimension line con flechas
    ax.annotate('', xy=(sx1, line_y), xytext=(sx2, line_y),
                arrowprops=dict(arrowstyle='<->', color='black',
                                shrinkA=0, shrinkB=0, lw=LW_DIM))
    # text label (above for 'above', below for 'below')
    txt_y = line_y + sgn*1.5
    va = 'bottom' if sgn > 0 else 'top'
    ax.text((sx1+sx2)/2, txt_y, label, ha='center', va=va, fontsize=FS_DIM)

def dim_v(ax, y1, y2, x, label, scale, offset, dx=8, side='right',
          ext=1.5):
    """Cota vertical entre y1 y y2 a la posicion x."""
    sy1 = scale*y1 + offset[1]; sy2 = scale*y2 + offset[1]
    sx = scale*x + offset[0]
    sgn = 1 if side == 'right' else -1
    line_x = sx + sgn*dx
    ax.plot([sx + sgn*ext, line_x + sgn*ext], [sy1, sy1], 'k', linewidth=LW_DIM)
    ax.plot([sx + sgn*ext, line_x + sgn*ext], [sy2, sy2], 'k', linewidth=LW_DIM)
    ax.annotate('', xy=(line_x, sy1), xytext=(line_x, sy2),
                arrowprops=dict(arrowstyle='<->', color='black',
                                shrinkA=0, shrinkB=0, lw=LW_DIM))
    txt_x = line_x + sgn*1.5
    ha = 'left' if sgn > 0 else 'right'
    ax.text(txt_x, (sy1+sy2)/2, label, ha=ha, va='center',
            fontsize=FS_DIM, rotation=90 if sgn > 0 else -90)

def dim_diameter(ax, cx, cy, d, scale, offset, label=None, leader_offset=(8, 8)):
    """Cota de diametro con leader linea."""
    sx = scale*cx + offset[0]; sy = scale*cy + offset[1]
    sd = scale*d
    end_x = sx + leader_offset[0]
    end_y = sy + leader_offset[1]
    ax.annotate('', xy=(sx + sd/2*0.7, sy + sd/2*0.7),
                xytext=(end_x, end_y),
                arrowprops=dict(arrowstyle='->', color='black',
                                shrinkA=0, shrinkB=0, lw=LW_DIM))
    if label is None:
        label = f"Ø{d:.2g}"
    ax.text(end_x, end_y + 1.0, label, ha='center', va='bottom',
            fontsize=FS_DIM)

def label_point(ax, cx, cy, scale, offset, text, leader_offset=(10, 5)):
    """Etiqueta de texto con leader."""
    sx = scale*cx + offset[0]; sy = scale*cy + offset[1]
    end_x = sx + leader_offset[0]; end_y = sy + leader_offset[1]
    ax.annotate('', xy=(sx, sy), xytext=(end_x, end_y),
                arrowprops=dict(arrowstyle='->', color='black',
                                shrinkA=0, shrinkB=0, lw=LW_DIM))
    ax.text(end_x, end_y + 1.0, text, ha='center', va='bottom',
            fontsize=FS_LABEL, fontweight='bold')

def view_title(ax, x, y, text):
    ax.text(x, y, text, ha='center', va='center',
            fontsize=FS_TITLE, fontweight='bold',
            bbox=dict(boxstyle='round', facecolor='white',
                      edgecolor='black', linewidth=0.5))

# ============================================================================
# PAGINA 1: BOTTOM CASE - PLAN VIEW
# ============================================================================

def page_bottom_plan(pdf):
    fig, ax = setup_page("BOTTOM CASE - VISTA EN PLANTA", 1, 6)
    # Layout: vista centrada arriba, leyenda abajo
    scale = 1.5   # 1.5px:1mm  -> 98mm * 1.5 = 147mm wide
    offset = (45, 60)  # bottom-left of the view

    # Outline rounded rect
    xs, ys = rounded_rect_outline(W/2, L/2, W, L, r_vert)
    draw_outline(ax, xs, ys, scale, offset)

    # Standoffs (4 circles with center holes)
    for p in mh:
        draw_circle(ax, p[0], p[1], standoff_od, scale, offset, lw=LW_OUTLINE)
        draw_circle(ax, p[0], p[1], standoff_id, scale, offset, lw=LW_HIDDEN,
                    linestyle='--')
        center_mark(ax, p[0], p[1], standoff_od, scale, offset)

    # Sensor cutouts (rectangles)
    draw_rect(ax, pos_max30102[0], pos_max30102[1],
              cut_max30102[0], cut_max30102[1], scale, offset)
    draw_rect(ax, pos_max30205[0], pos_max30205[1],
              cut_max30205[0], cut_max30205[1], scale, offset)

    # Electrode holes
    for e in electrodes:
        draw_circle(ax, e[0], e[1], elec_d, scale, offset)
        center_mark(ax, e[0], e[1], elec_d, scale, offset)

    # ----- COTAS -----
    # Outer dimensions
    dim_h(ax, 0, W, -3, f"{W:.0f}", scale, offset, dy=4, side='below')
    dim_v(ax, 0, L, -3, f"{L:.0f}", scale, offset, dx=4, side='left')

    # Corner radius
    ax.annotate('', xy=(scale*1 + offset[0], scale*1 + offset[1]),
                xytext=(scale*(-8) + offset[0], scale*(-8) + offset[1]),
                arrowprops=dict(arrowstyle='->', color='black', lw=LW_DIM))
    ax.text(scale*(-8) + offset[0], scale*(-9) + offset[1],
            f"R{r_vert:.0f}", ha='center', va='top', fontsize=FS_DIM)

    # Standoff positions (top edge X distance and Y distance)
    dim_h(ax, 0, mh[2][0], L+5, f"{mh[2][0]:.1f}", scale, offset, dy=4)
    dim_h(ax, 0, mh[3][0], L+12, f"{mh[3][0]:.1f}", scale, offset, dy=4)
    dim_v(ax, 0, mh[2][1], W+5, f"{mh[2][1]:.1f}", scale, offset, dx=4)
    dim_v(ax, 0, mh[0][1], W+12, f"{mh[0][1]:.1f}", scale, offset, dx=4)

    # Standoff diameter (leader from one standoff)
    p = mh[0]
    ax.annotate('', xy=(scale*p[0] + scale*standoff_od/2*0.7 + offset[0],
                        scale*p[1] + scale*standoff_od/2*0.7 + offset[1]),
                xytext=(scale*p[0] + 14 + offset[0],
                        scale*p[1] + 14 + offset[1]),
                arrowprops=dict(arrowstyle='->', color='black', lw=LW_DIM))
    ax.text(scale*p[0] + 14 + offset[0], scale*p[1] + 15 + offset[1],
            f"4x Ø{standoff_od:.1f} ext, Ø{standoff_id:.1f} int",
            ha='center', va='bottom', fontsize=FS_DIM)

    # MAX30102 dimensions
    p = pos_max30102
    dim_h(ax, p[0] - cut_max30102[0]/2, p[0] + cut_max30102[0]/2,
          p[1] - cut_max30102[1]/2, f"{cut_max30102[0]:.0f}", scale, offset,
          dy=3, side='below')
    dim_v(ax, p[1] - cut_max30102[1]/2, p[1] + cut_max30102[1]/2,
          p[0] + cut_max30102[0]/2, f"{cut_max30102[1]:.0f}", scale, offset,
          dx=3, side='right')
    label_point(ax, p[0], p[1] + cut_max30102[1]/2 + 1, scale, offset,
                "MAX30102\nventana", leader_offset=(15, 18))

    # MAX30205
    p = pos_max30205
    dim_h(ax, p[0] - cut_max30205[0]/2, p[0] + cut_max30205[0]/2,
          p[1] - cut_max30205[1]/2, f"{cut_max30205[0]:.0f}", scale, offset,
          dy=3, side='below')
    label_point(ax, p[0] - cut_max30205[0]/2 - 1, p[1], scale, offset,
                "MAX30205\nventana", leader_offset=(-18, -10))

    # Electrodes - just label one and indicate the diameter
    p = electrodes[2]   # bottom-center, the "RA touch" electrode
    label_point(ax, p[0], p[1], scale, offset,
                f"3x Ø{elec_d:.0f}\nelectrodos\nM3", leader_offset=(0, -15))

    # View title
    view_title(ax, 110, 195, "VISTA EN PLANTA - BOTTOM CASE (mirando hacia -Z)")

    # Section line A-A
    sec_y = L/2
    ax.plot([scale*(-5) + offset[0], scale*(W+5) + offset[0]],
            [scale*sec_y + offset[1], scale*sec_y + offset[1]],
            color='black', linewidth=LW_CENTER,
            linestyle=(0, (8, 3, 1, 3)))
    ax.text(scale*(-7) + offset[0], scale*sec_y + offset[1], "A",
            ha='right', va='center', fontsize=FS_TITLE, fontweight='bold')
    ax.text(scale*(W+7) + offset[0], scale*sec_y + offset[1], "A",
            ha='left', va='center', fontsize=FS_TITLE, fontweight='bold')

    # Notes
    ax.text(13, 50, "NOTAS:\n"
                    "1. Origen (0,0) = esquina inf-izq exterior\n"
                    "2. Esquinas verticales R12, paredes 2mm uniformes\n"
                    "3. Standoffs Z=2 a Z=4. Cilindros huecos para M2 self-tap\n"
                    "4. Ventanas y agujeros atraviesan TODO el piso (0 < Z < 2)\n"
                    "5. Seccion A-A en la pagina 2",
            ha='left', va='top', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow',
                      edgecolor='gray', linewidth=0.5))

    pdf.savefig(fig)
    plt.close(fig)

# ============================================================================
# PAGINA 2: BOTTOM CASE - SECCION A-A
# ============================================================================

def page_bottom_section(pdf):
    fig, ax = setup_page("BOTTOM CASE - SECCION A-A", 2, 6)
    scale = 2.5   # bigger for section (case es solo 4mm de alto)
    offset = (50, 80)

    # Outline of the section (full box outline 98 x 4)
    ax.add_patch(Rectangle((scale*0 + offset[0], scale*0 + offset[1]),
                           scale*W, scale*altura_total_bottom,
                           edgecolor='black', facecolor='none',
                           linewidth=LW_OUTLINE))
    # Cavity (Z=2 a Z=4, X=2 a X=W-2)
    ax.add_patch(Rectangle((scale*grosor_pared + offset[0],
                            scale*grosor_pared + offset[1]),
                           scale*(W - 2*grosor_pared), scale*altura_base,
                           edgecolor='black', facecolor='none',
                           linewidth=LW_OUTLINE))
    # Standoffs at Z = grosor_pared to altura_total
    # Project from MH positions
    for p in mh:
        x = p[0]
        ax.add_patch(Rectangle((scale*(x-standoff_od/2) + offset[0],
                               scale*grosor_pared + offset[1]),
                              scale*standoff_od, scale*altura_base,
                              edgecolor='black', facecolor='none',
                              linewidth=LW_OUTLINE))
        # Inner hole as hidden
        ax.add_patch(Rectangle((scale*(x-standoff_id/2) + offset[0],
                               scale*grosor_pared + offset[1]),
                              scale*standoff_id, scale*altura_base,
                              edgecolor='black', facecolor='none',
                              linewidth=LW_HIDDEN, linestyle='--'))

    # Sensor cutouts (extend full floor Z=0..2)
    # MAX30102
    x1, x2 = pos_max30102[0] - cut_max30102[0]/2, pos_max30102[0] + cut_max30102[0]/2
    ax.add_patch(Rectangle((scale*x1 + offset[0], scale*0 + offset[1]),
                          scale*(x2-x1), scale*grosor_pared,
                          edgecolor='red', facecolor='none',
                          linewidth=LW_OUTLINE))

    # Hatching for the floor (between cavity and outer)
    # Just hatch the wall slabs left/right and the floor (not cavity)
    # Floor (excluding cutout area at MAX30102)
    hatch_floor_segments = [(0, x1), (x2, W)]
    for xa, xb in hatch_floor_segments:
        rect = Rectangle((scale*xa + offset[0], offset[1]),
                         scale*(xb-xa), scale*grosor_pared,
                         hatch='//', facecolor='lightgray',
                         edgecolor='black', linewidth=0.3)
        ax.add_patch(rect)

    # Dimensions
    dim_h(ax, 0, W, 0, f"{W:.0f}", scale, offset, dy=5, side='below')
    dim_v(ax, 0, altura_total_bottom, 0, f"{altura_total_bottom:.0f}",
          scale, offset, dx=5, side='left')
    dim_v(ax, 0, grosor_pared, W, f"{grosor_pared:.0f}",
          scale, offset, dx=5, side='right')
    dim_v(ax, grosor_pared, altura_total_bottom, W+5, f"{altura_base:.0f}",
          scale, offset, dx=5, side='right')

    # Labels
    ax.text(scale*W/2 + offset[0], scale*1 + offset[1], "PISO 2mm",
            ha='center', va='center', fontsize=FS_DIM, color='blue')
    ax.text(scale*(W*0.2) + offset[0], scale*3 + offset[1], "CAVIDAD 2mm",
            ha='center', va='center', fontsize=FS_DIM, color='blue')

    # Section indicator
    view_title(ax, 148, 195, "SECCION A-A (corte horizontal Y=39.5 = L/2)")

    # Notes
    ax.text(15, 50, "VISTA SECCION A-A:\n"
                    "- Vista de un corte vertical en Y = L/2 = 39.5 mm\n"
                    "- Muestra los 4 standoffs proyectados (2 visibles + 2 detras)\n"
                    "- La ventana del MAX30102 (rojo) atraviesa el piso completo\n"
                    "- Hatching = material del case;  Blanco = aire/cavidad\n"
                    "- Los standoffs miden 2mm de alto (Z=2 a Z=4)\n"
                    "- PCB apoya sobre los standoffs (Z=4) y se eleva 1.6mm mas",
            ha='left', va='top', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow',
                      edgecolor='gray', linewidth=0.5))

    pdf.savefig(fig)
    plt.close(fig)

# ============================================================================
# PAGINA 3: TOP CASE - PLAN VIEW
# ============================================================================

def page_top_plan(pdf):
    fig, ax = setup_page("TOP CASE - VISTA EN PLANTA (desde arriba, mirando -Z)",
                         3, 6)
    scale = 1.4
    offset = (50, 60)

    # Outline (top of top case has taper: smaller)
    xs, ys = rounded_rect_outline(W/2, L/2, W - 2*taper, L - 2*taper,
                                   r_vert - taper)
    draw_outline(ax, xs, ys, scale, offset)

    # Hidden outline (base of top case = full envelope at seam)
    xs_b, ys_b = rounded_rect_outline(W/2, L/2, W, L, r_vert)
    draw_outline(ax, xs_b, ys_b, scale, offset, lw=LW_HIDDEN, linestyle=':')

    # Display window
    draw_rect(ax, display_center[0], display_center[1],
              display_size[0], display_size[1], scale, offset)

    # Pilares (hidden, dashed)
    for p in mh:
        draw_circle(ax, p[0], p[1], standoff_od, scale, offset,
                    lw=LW_HIDDEN, linestyle='--')
        draw_circle(ax, p[0], p[1], standoff_id, scale, offset,
                    lw=LW_HIDDEN, linestyle=':')

    # Lugs (4)
    for xoff in [-lug_center_sep/2, lug_center_sep/2]:
        xc = W/2 + xoff
        for y_base, dir_ in [(0, -1), (L, +1)]:
            y_outer_tip = y_base + dir_ * lug_protrude
            y_outer_ctr = y_outer_tip - dir_ * lug_thickness/2
            y_inner = y_base - dir_ * 1.0
            # Stadium shape: 2 sides + half-circle at outer end + half-circle attach
            # Just draw outline
            # Outer half-circle
            arc1 = arc_points(xc, y_outer_ctr, lug_thickness/2,
                              -90 if dir_ > 0 else 90,
                              90 if dir_ > 0 else 270, n=20)
            ax.plot([scale*p[0] + offset[0] for p in arc1],
                    [scale*p[1] + offset[1] for p in arc1],
                    'k', linewidth=LW_OUTLINE)
            # 2 straight sides
            ax.plot([scale*(xc-lug_thickness/2)+offset[0],
                     scale*(xc-lug_thickness/2)+offset[0]],
                    [scale*y_outer_ctr+offset[1], scale*y_inner+offset[1]],
                    'k', linewidth=LW_OUTLINE)
            ax.plot([scale*(xc+lug_thickness/2)+offset[0],
                     scale*(xc+lug_thickness/2)+offset[0]],
                    [scale*y_outer_ctr+offset[1], scale*y_inner+offset[1]],
                    'k', linewidth=LW_OUTLINE)
            # Spring bar hole (visible as dashed circle from top, since axis is X)
            ax.plot([scale*(xc-lug_thickness/2)+offset[0],
                     scale*(xc+lug_thickness/2)+offset[0]],
                    [scale*y_outer_ctr+offset[1], scale*y_outer_ctr+offset[1]],
                    'k', linewidth=LW_HIDDEN, linestyle='--')

    # Cotas
    # Outer
    dim_h(ax, 0, W, -3, f"{W:.0f}", scale, offset, dy=4, side='below')
    dim_v(ax, 0, L, -3, f"{L:.0f}", scale, offset, dx=4, side='left')

    # Display window
    p = display_center
    dim_h(ax, p[0]-display_size[0]/2, p[0]+display_size[0]/2,
          p[1]+display_size[1]/2, f"{display_size[0]:.0f}",
          scale, offset, dy=4)
    dim_v(ax, p[1]-display_size[1]/2, p[1]+display_size[1]/2,
          p[0]+display_size[0]/2, f"{display_size[1]:.0f}",
          scale, offset, dx=4)
    # Position of display center
    dim_h(ax, 0, p[0], -8, f"X={p[0]:.2f}", scale, offset, dy=4, side='below')
    dim_v(ax, 0, p[1], -8, f"Y={p[1]:.0f}", scale, offset, dx=4, side='left')
    label_point(ax, p[0], p[1], scale, offset, "Ventana display\n28x34 mm",
                leader_offset=(0, 16))

    # Lugs
    # Center-to-center separation
    xc_l = W/2 - lug_center_sep/2
    xc_r = W/2 + lug_center_sep/2
    dim_h(ax, xc_l, xc_r, L + lug_protrude + 1, f"{lug_center_sep:.0f} c-c",
          scale, offset, dy=4)
    # Inner gap
    inner_l = xc_l + lug_thickness/2
    inner_r = xc_r - lug_thickness/2
    dim_h(ax, inner_l, inner_r, L + lug_protrude + 7, f"{lug_strap_w:.0f} (correa)",
          scale, offset, dy=3)
    # Lug protrude
    dim_v(ax, L, L + lug_protrude, xc_l - 4, f"{lug_protrude:.0f}",
          scale, offset, dx=3, side='left')
    label_point(ax, xc_l, L + lug_protrude, scale, offset,
                f"Spring bar Ø{spring_bar_d:.1f}",
                leader_offset=(-15, 5))

    # Pillar one - label
    p = mh[0]
    label_point(ax, p[0], p[1], scale, offset,
                "4x Pilar interno\nØ4 ext, Ø1.8 int (hidden)",
                leader_offset=(-10, 12))

    view_title(ax, 148, 195, "TOP CASE - PLANTA")

    # Notes
    ax.text(13, 35, "NOTAS:\n"
                    "1. Linea punteada externa = base del top case (Z=0 local)\n"
                    "2. Linea solida = top del case (Z=18 local). Inset 2mm por taper\n"
                    "3. Ventana display atraviesa el techo (Z=15 a Z=18 local)\n"
                    "4. Pilares internos a Ø4 OD, Ø1.8 ID, Z=1.6 a Z=16 local\n"
                    "5. Lugs en Z=5 a Z=15 local (Z=9 a Z=19 absoluto)",
            ha='left', va='top', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow',
                      edgecolor='gray', linewidth=0.5))

    pdf.savefig(fig)
    plt.close(fig)

# ============================================================================
# PAGINA 4: TOP CASE - ALZADO LATERAL DERECHO (X+ wall)
# ============================================================================

def page_top_side(pdf):
    fig, ax = setup_page("TOP CASE - ALZADO LATERAL (pared +X)", 4, 6)
    scale = 2.0
    offset = (50, 70)

    # Wall is the projection looking from +X. Y vs Z view.
    # Outline: width L, height altura_top
    # With taper, the top is inset
    Wt = L - 2*taper * (altura_top / H)
    Lt = L

    # Trapezoid outline (taper)
    # Bottom (Z=0): Y from 0 to L
    # Top (Z=18): Y from 2*taper*18/22 to L - 2*taper*18/22
    bot_l, bot_r = 0, L
    top_l = taper * altura_top / H
    top_r = L - taper * altura_top / H
    # Approx with rounded top corners (chamfer)
    ax.plot([scale*bot_l + offset[0], scale*bot_r + offset[0]],
            [offset[1], offset[1]], 'k', linewidth=LW_OUTLINE)
    ax.plot([scale*bot_l + offset[0], scale*top_l + offset[0]],
            [offset[1], scale*altura_top + offset[1]],
            'k', linewidth=LW_OUTLINE)
    ax.plot([scale*bot_r + offset[0], scale*top_r + offset[0]],
            [offset[1], scale*altura_top + offset[1]],
            'k', linewidth=LW_OUTLINE)
    ax.plot([scale*top_l + offset[0], scale*top_r + offset[0]],
            [scale*altura_top + offset[1], scale*altura_top + offset[1]],
            'k', linewidth=LW_OUTLINE)

    # Buttons (2 circles)
    for by in btn_y_list:
        draw_circle(ax, by, btn_z, btn_d, scale, offset)
        center_mark(ax, by, btn_z, btn_d, scale, offset)
    # USB-C cutout (rectangle Y x Z)
    draw_rect(ax, usb_y, usb_z, usb_size[0], usb_size[1], scale, offset)

    # Jack hole on the opposite wall (-X wall, projected as hidden)
    draw_circle(ax, jack_y, jack_z, jack_d, scale, offset, lw=LW_HIDDEN, linestyle='--')
    center_mark(ax, jack_y, jack_z, jack_d, scale, offset)

    # Lugs (4) - project onto YZ plane
    # In this elevation, we see lugs from the side. Both -Y and +Y lugs are visible
    # The -Y lug extends from Y=0 outward to Y=-7
    # The +Y lug extends from Y=L outward to Y=L+7
    # Z range Z=5 to Z=15
    # But there are 2 lugs per side at different X — they all project onto same YZ
    # Draw lug outline as rectangle (no spring bar hole visible in this view since hole axis is X)
    # Actually we'd see the spring bar holes as circles (X axis perpendicular to YZ plane)
    # Lug on -Y side:
    for y_base, dir_ in [(0, -1), (L, +1)]:
        y_outer = y_base + dir_ * lug_protrude
        y_outer_ctr = y_outer - dir_ * lug_thickness/2
        # rectangle for lug body
        y1 = min(y_outer_ctr, y_base + dir_ * (-lug_thickness/2))  # roughly
        # easier: draw a rounded rect Y_base to Y_outer x Z_bot to Z_top
        x_a, x_b = min(y_base, y_outer), max(y_base, y_outer)
        ax.add_patch(Rectangle((scale*x_a + offset[0],
                                scale*lug_z_bot + offset[1]),
                               scale*(x_b - x_a),
                               scale*(lug_z_top - lug_z_bot),
                               edgecolor='black', facecolor='none',
                               linewidth=LW_OUTLINE))
        # Spring bar hole (visible circle, end-on)
        draw_circle(ax, y_outer_ctr, (lug_z_bot+lug_z_top)/2,
                    spring_bar_d, scale, offset, lw=LW_HIDDEN, linestyle='--')
        center_mark(ax, y_outer_ctr, (lug_z_bot+lug_z_top)/2,
                    spring_bar_d, scale, offset)

    # Dimensions
    # Total height
    dim_v(ax, 0, altura_top, -3, f"{altura_top:.0f}", scale, offset,
          dx=4, side='left')
    # Total length (Y)
    dim_h(ax, 0, L, 0, f"{L:.0f}", scale, offset, dy=4, side='below')

    # Button positions
    dim_h(ax, 0, btn_y_list[0], altura_top + 2, f"{btn_y_list[0]:.2f}",
          scale, offset, dy=4)
    dim_h(ax, 0, btn_y_list[1], altura_top + 8, f"{btn_y_list[1]:.2f}",
          scale, offset, dy=4)
    # Button Z
    dim_v(ax, 0, btn_z, btn_y_list[1] + 2, f"Z={btn_z:.1f}",
          scale, offset, dx=3, side='right')
    label_point(ax, btn_y_list[0], btn_z, scale, offset,
                f"2x boton Ø{btn_d:.0f}", leader_offset=(-10, 15))

    # USB-C
    dim_h(ax, 0, usb_y, altura_top + 14, f"{usb_y:.0f}",
          scale, offset, dy=4)
    dim_h(ax, usb_y - usb_size[0]/2, usb_y + usb_size[0]/2,
          usb_z - usb_size[1]/2 - 2,
          f"{usb_size[0]:.0f}", scale, offset, dy=3, side='below')
    dim_v(ax, usb_z - usb_size[1]/2, usb_z + usb_size[1]/2,
          usb_y + usb_size[0]/2 + 2, f"{usb_size[1]:.0f}",
          scale, offset, dx=3, side='right')
    label_point(ax, usb_y, usb_z, scale, offset,
                "USB-C 10x4", leader_offset=(15, -8))

    label_point(ax, jack_y, jack_z, scale, offset,
                f"Jack 3.5mm (hidden, pared izq)\nØ{jack_d:.1f}, Y={jack_y:.2f}, Z={jack_z:.1f}",
                leader_offset=(-15, -12))

    # Lug spring bar position
    label_point(ax, -lug_protrude + lug_thickness/2,
                (lug_z_bot+lug_z_top)/2, scale, offset,
                f"Spring bar Ø{spring_bar_d:.1f}\nZ={(lug_z_bot+lug_z_top)/2:.0f}",
                leader_offset=(-12, 8))
    dim_v(ax, lug_z_bot, lug_z_top, -lug_protrude + 1,
          f"{lug_z_top - lug_z_bot:.0f}", scale, offset, dx=3, side='left')
    dim_v(ax, 0, lug_z_bot, -lug_protrude + 1, f"{lug_z_bot:.0f}",
          scale, offset, dx=3, side='left')

    view_title(ax, 148, 195, "TOP CASE - ALZADO LATERAL (pared +X, mira hacia -X)")

    ax.text(13, 35, "NOTAS:\n"
                    "1. Z=0 en esta vista = seam (top-case-local). Suma 4mm para abs.\n"
                    "2. Botones: Ø4 atraviesa pared +X (lado derecho), eje +X\n"
                    "3. USB-C: cutout 10x4 atraviesa pared +X, eje +X\n"
                    "4. Lugs visibles a izq y der (en -Y y +Y respectivamente)\n"
                    "5. Spring bar holes representados como circulos punteados",
            ha='left', va='top', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow',
                      edgecolor='gray', linewidth=0.5))

    pdf.savefig(fig)
    plt.close(fig)

# ============================================================================
# PAGINA 5: TOP CASE - ALZADO FRONTAL (-Y wall, jack)
# ============================================================================

def page_top_front(pdf):
    fig, ax = setup_page("TOP CASE - ALZADO FRONTAL (pared -Y)", 5, 6)
    scale = 2.0
    offset = (50, 70)

    # Looking in +Y direction at the -Y wall. X vs Z plane.
    # outer X = 0 to W
    ax.plot([scale*0 + offset[0], scale*W + offset[0]],
            [offset[1], offset[1]], 'k', linewidth=LW_OUTLINE)
    # tapered sides
    top_l = taper * altura_top / H
    top_r = W - taper * altura_top / H
    ax.plot([scale*0 + offset[0], scale*top_l + offset[0]],
            [offset[1], scale*altura_top + offset[1]],
            'k', linewidth=LW_OUTLINE)
    ax.plot([scale*W + offset[0], scale*top_r + offset[0]],
            [offset[1], scale*altura_top + offset[1]],
            'k', linewidth=LW_OUTLINE)
    ax.plot([scale*top_l + offset[0], scale*top_r + offset[0]],
            [scale*altura_top + offset[1], scale*altura_top + offset[1]],
            'k', linewidth=LW_OUTLINE)

    # Jack hole projection (hidden rectangle on the left wall)
    draw_rect(ax, grosor_pared/2, jack_z, grosor_pared, jack_d, scale, offset, lw=LW_HIDDEN, linestyle='--')
    # Center line for the jack hole
    ax.plot([scale*(-3) + offset[0], scale*(grosor_pared+3) + offset[0]],
            [scale*jack_z + offset[1], scale*jack_z + offset[1]],
            color='black', linewidth=LW_CENTER, linestyle=(0, (4, 2, 1, 2)))

    # Lugs visible (4 lugs project onto this view)
    # Two on -Y (visible front-on) and two on +Y (hidden behind)
    # The lugs at -Y side: their outline shows as rectangles X x Z
    for xoff in [-lug_center_sep/2, lug_center_sep/2]:
        xc = W/2 + xoff
        # -Y lug visible (in front of case)
        ax.add_patch(Rectangle((scale*(xc - lug_thickness/2) + offset[0],
                                scale*lug_z_bot + offset[1]),
                               scale*lug_thickness,
                               scale*(lug_z_top - lug_z_bot),
                               edgecolor='black', facecolor='none',
                               linewidth=LW_OUTLINE))
        # Spring bar hole shown as horizontal line crossing the lug
        ax.plot([scale*(xc - lug_thickness/2) + offset[0],
                 scale*(xc + lug_thickness/2) + offset[0]],
                [scale*((lug_z_bot+lug_z_top)/2) + offset[1],
                 scale*((lug_z_bot+lug_z_top)/2) + offset[1]],
                'k', linewidth=LW_HIDDEN, linestyle='--')

    # Cotas
    dim_h(ax, 0, W, 0, f"{W:.0f}", scale, offset, dy=4, side='below')
    dim_v(ax, 0, altura_top, -3, f"{altura_top:.0f}",
          scale, offset, dx=4, side='left')

    # Jack (hidden, on left wall)
    label_point(ax, grosor_pared/2, jack_z, scale, offset,
                f"Cutout Jack 3.5mm (pared izq)\nØ{jack_d:.1f}, Z={jack_z:.1f}", leader_offset=(15, 12))

    # Lug positions
    xc_l = W/2 - lug_center_sep/2
    xc_r = W/2 + lug_center_sep/2
    dim_h(ax, xc_l, xc_r, lug_z_top + 2, f"{lug_center_sep:.0f}",
          scale, offset, dy=3)
    dim_h(ax, 0, xc_l, lug_z_bot - 4, f"{xc_l:.1f}",
          scale, offset, dy=3, side='below')
    label_point(ax, xc_l, lug_z_top, scale, offset,
                f"4x lug\n5 wide x 10 tall", leader_offset=(-15, 8))

    view_title(ax, 148, 195, "TOP CASE - ALZADO FRONTAL (mira hacia +Y)")

    ax.text(13, 35, "NOTAS:\n"
                    "1. Z=0 en esta vista = seam (top-case-local)\n"
                    "2. Jack 3.5mm Ø6.5 atraviesa pared -X (izquierda, oculto)\n"
                    "3. Los 4 lugs son visibles en este alzado (2 al frente, 2 atras\n"
                    "   se proyectan sobre el mismo X)\n"
                    "4. Las paredes laterales muestran el taper (2mm inset arriba)",
            ha='left', va='top', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow',
                      edgecolor='gray', linewidth=0.5))

    pdf.savefig(fig)
    plt.close(fig)

# ============================================================================
# PAGINA 6: DETALLES - LUG + BUTTON CAP
# ============================================================================

def page_details(pdf):
    fig, ax = setup_page("DETALLES - LUG y BUTTON CAP", 6, 6)
    # Two detail views side by side
    # LEFT: lug top view + side view
    # RIGHT: button cap section

    # ----- LUG TOP VIEW (left side) -----
    scale_lug = 7.0
    offset_lug = (30, 110)

    # Outline of one lug viewed from top (stadium shape)
    xc = 0  # local origin
    y_base = 0
    dir_ = -1  # extend in -Y
    y_outer_ctr = -lug_protrude + lug_thickness/2  # = -4.5
    y_inner = 1.0  # 1mm into case
    # Draw stadium
    # Arc 1 (outer end half-circle)
    arc1 = arc_points(xc, y_outer_ctr, lug_thickness/2, 90, 270, n=30)
    ax.plot([scale_lug*p[0] + offset_lug[0] for p in arc1],
            [scale_lug*p[1] + offset_lug[1] for p in arc1],
            'k', linewidth=LW_OUTLINE)
    # Sides
    ax.plot([scale_lug*(-lug_thickness/2) + offset_lug[0],
             scale_lug*(-lug_thickness/2) + offset_lug[0]],
            [scale_lug*y_outer_ctr + offset_lug[1],
             scale_lug*y_inner + offset_lug[1]],
            'k', linewidth=LW_OUTLINE)
    ax.plot([scale_lug*(+lug_thickness/2) + offset_lug[0],
             scale_lug*(+lug_thickness/2) + offset_lug[0]],
            [scale_lug*y_outer_ctr + offset_lug[1],
             scale_lug*y_inner + offset_lug[1]],
            'k', linewidth=LW_OUTLINE)
    # Top closing (case wall edge as dashed)
    ax.plot([scale_lug*(-lug_thickness/2) + offset_lug[0],
             scale_lug*(+lug_thickness/2) + offset_lug[0]],
            [scale_lug*y_inner + offset_lug[1],
             scale_lug*y_inner + offset_lug[1]],
            'k', linewidth=LW_HIDDEN, linestyle='--')
    # Case wall edge at Y=0 (dashed = boundary with case)
    ax.plot([scale_lug*(-lug_thickness*0.8) + offset_lug[0],
             scale_lug*(+lug_thickness*0.8) + offset_lug[0]],
            [scale_lug*y_base + offset_lug[1],
             scale_lug*y_base + offset_lug[1]],
            'k', linewidth=LW_HIDDEN, linestyle=':')
    # Spring bar hole (visible in top view as a thin rectangle since hole axis is X)
    ax.add_patch(Rectangle((scale_lug*(-lug_thickness/2)+offset_lug[0],
                            scale_lug*(y_outer_ctr - spring_bar_d/2)+offset_lug[1]),
                          scale_lug*lug_thickness, scale_lug*spring_bar_d,
                          edgecolor='black', facecolor='none',
                          linewidth=LW_HIDDEN, linestyle='--'))
    center_mark(ax, xc, y_outer_ctr, lug_thickness, scale_lug, offset_lug)

    # Dimensions for lug top view
    dim_h(ax, -lug_thickness/2, +lug_thickness/2, -lug_protrude - 0.5,
          f"{lug_thickness:.0f}", scale_lug, offset_lug, dy=4, side='below')
    dim_v(ax, -lug_protrude, 0, lug_thickness/2 + 0.5,
          f"{lug_protrude:.0f}", scale_lug, offset_lug, dx=4, side='right')
    dim_v(ax, y_outer_ctr, 0, -lug_thickness/2 - 0.5,
          f"{abs(y_outer_ctr):.1f}", scale_lug, offset_lug,
          dx=4, side='left')
    # Diameter of half-circle
    label_point(ax, xc, y_outer_ctr, scale_lug, offset_lug,
                f"R{lug_thickness/2:.1f}", leader_offset=(15, 10))
    # Spring bar hole diameter
    label_point(ax, 0, y_outer_ctr - spring_bar_d/2, scale_lug, offset_lug,
                f"Ø{spring_bar_d:.1f} spring bar\n(pasante a lo largo de X)",
                leader_offset=(20, -10))

    view_title(ax, 50, 165, "DETALLE LUG (vista en planta, escala 7x)")

    # ----- LUG SIDE VIEW -----
    offset_lug_side = (110, 110)
    scale_lug_side = 7.0
    # Side view: Y x Z
    # Lug rectangle Y_outer to Y_base, Z_bot to Z_top
    ax.add_patch(Rectangle((scale_lug_side*(-lug_protrude) + offset_lug_side[0],
                           scale_lug_side*lug_z_bot + offset_lug_side[1]),
                          scale_lug_side*lug_protrude,
                          scale_lug_side*(lug_z_top - lug_z_bot),
                          edgecolor='black', facecolor='none',
                          linewidth=LW_OUTLINE))
    # Spring bar hole (Ø1.8 circle)
    draw_circle(ax, y_outer_ctr, (lug_z_bot+lug_z_top)/2,
                spring_bar_d, scale_lug_side, offset_lug_side)
    center_mark(ax, y_outer_ctr, (lug_z_bot+lug_z_top)/2,
                spring_bar_d, scale_lug_side, offset_lug_side)
    # Dimensions
    dim_v(ax, lug_z_bot, lug_z_top, -lug_protrude - 0.5,
          f"{lug_z_top - lug_z_bot:.0f}", scale_lug_side, offset_lug_side,
          dx=4, side='left')
    dim_h(ax, -lug_protrude, 0, lug_z_bot - 0.5,
          f"{lug_protrude:.0f}", scale_lug_side, offset_lug_side,
          dy=4, side='below')
    label_point(ax, y_outer_ctr, (lug_z_bot+lug_z_top)/2,
                scale_lug_side, offset_lug_side,
                f"Ø{spring_bar_d:.1f}", leader_offset=(15, 5))

    view_title(ax, 130, 165, "DETALLE LUG (alzado lateral, escala 7x)")

    # ----- BUTTON CAP DETAIL (right side) -----
    flange_d = 6.0
    flange_h = 1.5
    stem_d = 3.5
    stem_h = 7.9
    lip_h = 0.8
    lip_d = 4.3

    offset_cap = (210, 80)
    scale_cap = 7.0

    # Section view: half-cross-section of cap. R axis vertical, axial axis horizontal.
    # Total length along axial = flange_h + stem_h
    # Use horizontal layout: flange on left, stem to the right
    # X axis = axial direction (mm)
    # Y axis = radial direction (mm)

    # Flange (rectangle flange_h x flange_d/2 on top half)
    ax.add_patch(Rectangle((offset_cap[0], offset_cap[1]),
                          scale_cap*flange_h, scale_cap*flange_d/2,
                          edgecolor='black', facecolor='lightgray',
                          linewidth=LW_OUTLINE, hatch='///'))
    # Stem (rectangle stem_h x stem_d/2)
    ax.add_patch(Rectangle((scale_cap*flange_h + offset_cap[0], offset_cap[1]),
                          scale_cap*(stem_h - lip_h), scale_cap*stem_d/2,
                          edgecolor='black', facecolor='lightgray',
                          linewidth=LW_OUTLINE, hatch='///'))
    # Lip (trapezoidal cross-section)
    lip_x_start = scale_cap*(flange_h + stem_h - lip_h) + offset_cap[0]
    lip_x_end = scale_cap*(flange_h + stem_h) + offset_cap[0]
    path = Path([(lip_x_start, offset_cap[1]),
                 (lip_x_end, offset_cap[1]),
                 (lip_x_end, offset_cap[1] + scale_cap*lip_d/2),
                 (lip_x_start, offset_cap[1] + scale_cap*stem_d/2),
                 (lip_x_start, offset_cap[1])])
    ax.add_patch(PathPatch(path, edgecolor='black', facecolor='lightgray',
                          linewidth=LW_OUTLINE, hatch='///'))
    # Center line (axial)
    ax.plot([offset_cap[0] - 2, scale_cap*(flange_h+stem_h) + offset_cap[0] + 5],
            [offset_cap[1], offset_cap[1]],
            'k', linewidth=LW_CENTER, linestyle=(0, (8, 3, 1, 3)))

    # Dimensions
    dim_h(ax, 0, flange_h, -flange_d/2*0.5, f"{flange_h:.1f}",
          scale_cap, offset_cap, dy=4, side='below')
    dim_h(ax, flange_h, flange_h + stem_h - lip_h,
          -flange_d/2*0.5,
          f"{stem_h - lip_h:.1f}", scale_cap, offset_cap, dy=10, side='below')
    dim_h(ax, flange_h + stem_h - lip_h, flange_h + stem_h,
          -flange_d/2*0.5,
          f"{lip_h:.1f}", scale_cap, offset_cap, dy=16, side='below')
    dim_v(ax, 0, flange_d/2, -1, f"Ø{flange_d:.0f}",
          scale_cap, offset_cap, dx=4, side='left')
    dim_v(ax, 0, stem_d/2, flange_h + stem_h*0.4, f"Ø{stem_d:.1f}",
          scale_cap, offset_cap, dx=4, side='right')
    dim_v(ax, 0, lip_d/2, flange_h + stem_h + 0.5, f"Ø{lip_d:.1f}",
          scale_cap, offset_cap, dx=4, side='right')

    label_point(ax, flange_h/2, flange_d/2 - 0.3, scale_cap, offset_cap,
                "Flange\nexterior", leader_offset=(0, 12))
    label_point(ax, flange_h + (stem_h - lip_h)/2, stem_d/2 - 0.2,
                scale_cap, offset_cap,
                "Stem", leader_offset=(0, 8))
    label_point(ax, flange_h + stem_h - lip_h/2, lip_d/2 - 0.1,
                scale_cap, offset_cap,
                "Retention lip\nconico", leader_offset=(15, 10))

    view_title(ax, 235, 165,
              "DETALLE BUTTON CAP\n(seccion axial, 7x)")

    # General notes
    ax.text(13, 50,
            "NOTAS GENERALES (todas las piezas):\n"
            "1. Hatching = material solido en vistas de seccion\n"
            "2. Lineas punteadas = aristas ocultas o bordes mas alla del corte\n"
            "3. Cota maxima de R/Ø incluye 'Ø' para diametro, 'R' para radio\n"
            "4. Origen de cada vista esta indicado por el centro de marca o por\n"
            "   la cota referida en (0,0) del feature\n"
            "5. Tolerancia de impresion FDM tipica: +0.0 / -0.3 para huecos\n"
            "   roscados, +/-0.2 para dimensiones planas",
            ha='left', va='top', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow',
                      edgecolor='gray', linewidth=0.5))

    pdf.savefig(fig)
    plt.close(fig)

# ============================================================================
# MAIN
# ============================================================================

def main():
    out = "supaclock_v2_blueprint.pdf"
    with PdfPages(out) as pdf:
        page_bottom_plan(pdf)
        page_bottom_section(pdf)
        page_top_plan(pdf)
        page_top_side(pdf)
        page_top_front(pdf)
        page_details(pdf)
    print(f"Generado: {out}")

if __name__ == "__main__":
    main()
