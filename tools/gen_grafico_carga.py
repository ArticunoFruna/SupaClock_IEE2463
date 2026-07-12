"""Genera curva de carga para el slide de batería.

LiPo 300 mAh, cargador integrado del XIAO ESP32-S3 (~100 mA).
Modelo simple: CC hasta ~90% (lineal), luego CV con decay exponencial.

Salida: docs/Entrega Final/grafico_carga_ajustado.png
"""
import numpy as np
import matplotlib.pyplot as plt
import os

CAP_MAH = 300.0
I_CC = 100.0       # mA — corriente en fase CC
SOC_CC_END = 90.0  # % — cambio a CV

t_cc_end = (SOC_CC_END/100.0) * CAP_MAH / I_CC  # h -> 300*0.9/100 = 2.7 h
tau_cv = 0.45  # h — decay CV

# Fase CC: 0..t_cc_end -> 0..90 %
t_cc = np.linspace(0.0, t_cc_end, 200)
soc_cc = (t_cc / t_cc_end) * SOC_CC_END

# Fase CV: aproximación exponencial hacia 100 %
t_cv = np.linspace(t_cc_end, t_cc_end + 1.6, 250)
soc_cv = 100.0 - (100.0 - SOC_CC_END) * np.exp(-(t_cv - t_cc_end) / tau_cv)

t = np.concatenate([t_cc, t_cv])
soc = np.concatenate([soc_cc, soc_cv])

# Punto de 90 % (fin CC)
t_90 = t_cc_end
# Tiempo hasta 100 %: definimos "carga completa" cuando soc > 99.5
idx_full = np.argmax(soc >= 99.5)
t_full = t[idx_full] if idx_full > 0 else t[-1]

fig, ax = plt.subplots(figsize=(9, 5.5))
ax.plot(t, soc, color="#1f77b4", linewidth=2.6, label=f"Carga USB-C ({t_full:.1f} h a 100 %)")
ax.axhline(90.0, color="#7fbcd6", linestyle="--", linewidth=1.4, label="90 % (fin de fase CC)")

# Marker en 90 %
ax.plot([t_90], [90.0], marker="o", color="#2ca02c", markersize=9,
        label=f"Muestra: {t_90:.1f} h → 90 %")
ax.annotate(f"{t_90:.1f} h, 90 %", xy=(t_90, 90.0),
            xytext=(t_90 + 0.35, 68),
            arrowprops=dict(arrowstyle="->", color="black", lw=1.2),
            fontsize=13)

# Cara sección: CC / CV
ax.axvspan(0, t_cc_end, alpha=0.05, color="#1f77b4")
ax.text(t_cc_end/2, 3, "Corriente constante", ha="center", fontsize=11,
        color="#333333", style="italic")
ax.text(t_cc_end + 0.7, 3, "Voltaje constante", ha="center", fontsize=11,
        color="#333333", style="italic")

ax.set_xlim(0, t_cc_end + 1.7)
ax.set_ylim(0, 105)
ax.set_xlabel("Tiempo [horas]", fontsize=13)
ax.set_ylabel("Batería [%]", fontsize=13)
ax.set_title("Carga", fontsize=15, pad=10)
ax.grid(True, alpha=0.35)
ax.legend(loc="lower right", fontsize=11)

plt.tight_layout()
out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "docs", "Entrega Final")
out_path = os.path.join(out_dir, "grafico_carga_ajustado.png")
plt.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"Guardado: {out_path}")
