# Handoff — Conversión del Informe de Avance 3 a LaTeX

**Audiencia:** agente que toma `docs/Entrega3/entrega_3.md` (informe en Markdown) y produce `docs/Entrega3/main.tex` + `docs/Entrega3/main.pdf` siguiendo el estilo de los informes anteriores.

**Resultado esperado:** PDF compilable con `latexmk` o `pdflatex + bibtex + pdflatex × 2` en `docs/Entrega3/`, con encabezado institucional, índice, lista de figuras/tablas y bibliografía resueltos.

---

## 1. Insumo principal

Toma como **fuente de verdad de contenido** el archivo:

```
docs/Entrega3/entrega_3.md
```

(982 líneas, ~16 000 palabras; cubre los tres ejes de la rúbrica: diagrama de bloques de bajo nivel, planificación, implementación y resultados). No reescribas ni resumas el contenido: tu trabajo es **transcribirlo a LaTeX manteniendo redacción, métricas y estructura**. Si encuentras una ambigüedad, prefiere la formulación del MD por sobre la del informe anterior.

## 2. Plantilla y estilo de referencia

Replica exactamente el preámbulo, comandos custom, layout de encabezado y tipografía del avance 2:

```
docs/Entrega2/main.tex
```

Punto por punto:

- `\documentclass[letterpaper,11pt]{article}` y márgenes idénticos vía `geometry` (top=1.7cm, bottom=2cm, left=2cm, right=2cm).
- Mismos paquetes (lista completa en líneas 1–38 de `docs/Entrega2/main.tex`): `inputenc utf8`, `babel spanish`, `graphicx`, `fancyhdr`, `xcolor[table,xcdraw]`, `tikz` con las mismas librerías, `amsmath/amssymb/amsfonts`, `float`, `siunitx`, `listings`, `hyperref`, `booktabs`, `url`, `titlesec`, `adjustbox[export]`, `parskip`, `natbib[numbers]`, `ragged2e`, `array`, `longtable`, `multirow`, `pgfplots` (compat=1.17), `makecell`.
- Comandos custom `\rfigura`, `\rtabla`, configuración de `\lstset` para listings en C con literales UTF-8 (acentos y `°`).
- Bloque de encabezado en `\noindent\begin{minipage}` con logo a la izquierda, datos institucionales al centro y `Grupo \NumeroGrupo` a la derecha, tal como aparece en líneas 79–96 del E2.
- `\tableofcontents`, `\listoffigures` y `\listoftables` antes del cuerpo.

**Variables del nuevo informe:**

```latex
\newcommand{\Curso}{IEE2913 --- Diseño Eléctrico (Capstone)}
\newcommand{\TituloInforme}{Informe de Avance 3 (50\,\%)}
\newcommand{\NumeroGrupo}{10}
\newcommand{\NombreProyecto}{SupaClock: Wearable Biométrico Modular}
\newcommand{\Integrantes}{Tomás Avendaño, Benjamín Sepúlveda, Pablo Uribe}
\newcommand{\FechaEntrega}{1 de junio de 2026}
```

## 3. Mapeo MD → LaTeX

| Sección del MD | Equivalente LaTeX |
|---|---|
| `# Resumen ejecutivo` | `\section*{Resumen ejecutivo}` (no numerada) + `\addcontentsline{toc}{section}{Resumen ejecutivo}` |
| `# 1. Introducción` | `\section{Introducción}` |
| `## 1.1. Contexto…` | `\subsection{Contexto…}` |
| `# 2. Diagrama de bloques…` | `\section{Diagrama de bloques de bajo nivel actualizado}` |
| `## 2.1. … 2.4.` | `\subsection{…}` |
| `### 2.2.1.…` | `\subsubsection{…}` |
| `# 3. Planificación` | `\section{Planificación actualizada}` |
| `# 4. Implementación y resultados` | `\section{Implementación y resultados}\label{sec:impl}` |
| `## 4.1 – 4.8` | `\subsection{…}` |
| `### 4.x.y` | `\subsubsection{…}` |
| `# 5. Análisis cuantitativo` | `\section{Análisis cuantitativo de resultados}` |
| `# 6. Plan de validación…` | `\section{Plan de validación de la unidad cerrada y próximos pasos}` |
| `# 7. Referencias` | **No reproducir como sección.** Usar `\bibliographystyle{unsrtnat}` + `\bibliography{ref}` (alimentando `docs/Entrega3/ref.bib`, ver §6). |
| `# A. Anexos` | `\appendix\section{Anexos: extractos de código y tablas}` (las subsecciones del MD A.1–A.9 pasan a `\subsection{…}` bajo el apéndice). |

### 3.1. Tablas

Todas las tablas del MD deben renderizarse con `booktabs` (`\toprule`, `\midrule`, `\bottomrule`), envueltas en `\begin{table}[H] … \end{table}` con `\caption{…}\label{tab:…}` antes del `\begin{tabular}`. Las tablas largas (Tabla 2.1 *Especificaciones por bloque*, Tabla 3.1 *Contraste planificado vs. logrado*) requieren `longtable`. Para columnas de texto envolvente usa la macro definida en el E2: `\newcolumntype{L}[1]{>{\RaggedRight\arraybackslash}p{#1}}`.

### 3.2. Listados de código

El MD usa bloques ```\`\`\`c``` y ```\`\`\`scad```. Conviértelos a `\begin{lstlisting}[language=C, caption={…}, label={lst:…}]` (o sin `language` para SCAD/Dart). El estilo global ya está configurado en el preámbulo del E2; sólo añade `caption` y `label` por listing.

### 3.3. Énfasis y código en línea

- `**negrita**` → `\textbf{…}`.
- `*itálica*` → `\textit{…}` (o `\emph{…}` si está dentro de una itálica).
- ` `\`código\`` ` → `\texttt{…}`.

### 3.4. Citas

Donde el MD cite normas o documentos (Espressif, IEEE 11073, etc.), reemplaza por `\cite{key}` con la clave correspondiente del `ref.bib` (sección 6 detalla las claves nuevas).

## 4. Figuras a generar / referenciar

El informe MD menciona las siguientes figuras. Cada una debe materializarse como `\begin{figure}[H]…\includegraphics…\caption…\label…\end{figure}`. Las rutas son relativas a `docs/Entrega3/`.

| Fig. | Caption (texto exacto del MD) | Fuente |
|---|---|---|
| 2.1 | *Diagrama de bloques de bajo nivel del prototipo SupaClock al cierre del avance 3…* | `\resizebox{\textwidth}{!}{\input{fig_bloques_lownivel_e3.tex}}` (ya existe). |
| 3.1 | *Carta Gantt actualizada al 28/05/2026…* | `\includegraphics[width=\linewidth]{gantt.jpeg}` (ya existe en `docs/Entrega3/gantt.jpeg`). |
| 4.x renders de carcasa | Top/bottom/lugs assembly | Usa `\graphicspath{{./}{../../mechanical/}}` y referencia `render_v2_assembly_hero.png`, `render_v2_assembly_top.png`, `render_v2_assembly_bottom.png`, `render_v2_lug_closeup.png`. |
| 4.x PCB carrier | Cara TOP/BOT del PCB carrier | `pcb_front-1.png` y `pcb_back-1.png` (ya en `docs/Entrega3/`). |
| 4.x diagrama HAR pipeline | Optional: convertir el bloque mermaid de `funcionamiento_ml.md` a TikZ; si no, omitir la figura y mantener la tabla de capas que ya está en el MD. |

Configura al inicio del documento:

```latex
\graphicspath{{./}{../Entrega2/}{../../mechanical/}{../../hardware/SupaClock_Carrier/}}
```

(igual que `docs/Entrega3/presentacion3.tex`, ya verificado).

## 5. Estructura cronológica del documento final

Sigue este orden (idéntico al MD):

1. Resumen ejecutivo
2. Introducción (1.1 Contexto y delta)
3. Diagrama de bloques de bajo nivel actualizado (2.1 a 2.4)
4. Planificación actualizada (3.1 a 3.3)
5. Implementación y resultados (4.1 a 4.8)
6. Análisis cuantitativo de resultados (5.1 a 5.5)
7. Plan de validación de la unidad cerrada y próximos pasos (6.1 a 6.4)
8. Referencias bibliográficas (`\bibliography`)
9. Apéndices A.1 a A.9

## 6. Bibliografía (`ref.bib`)

Crea `docs/Entrega3/ref.bib` reutilizando la base de `docs/Entrega2/ref.bib` y **añadiendo** las nuevas claves que aparecen en la sección 7 del MD:

```bibtex
@misc{esp32s3_ds_2024,
  title  = {ESP32-S3 Series Datasheet},
  author = {{Espressif Systems}},
  year   = {2024},
  url    = {https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf}
}
@misc{esp_dsp,
  title  = {ESP-DSP Library},
  author = {{Espressif Systems}},
  year   = {2024},
  url    = {https://github.com/espressif/esp-dsp}
}
@misc{esp_nn,
  title  = {ESP-NN: Optimized Neural Network functions for ESP32-S3},
  author = {{Espressif Systems}},
  year   = {2024},
  url    = {https://github.com/espressif/esp-nn}
}
@misc{tflite_micro_2024,
  title  = {TensorFlow Lite for Microcontrollers},
  author = {{TensorFlow Authors}},
  year   = {2024},
  url    = {https://www.tensorflow.org/lite/microcontrollers}
}
@manual{xiao_esp32s3_pinout,
  title  = {Seeed XIAO ESP32-S3 Schematic \& Pinout Reference v1.4},
  author = {{Seeed Studio}},
  year   = {2026}
}
@manual{lpkf_protomat,
  title  = {ProtoMat S64 User Manual and Design Rules},
  author = {{LPKF Laser \& Electronics AG}},
  year   = {2023}
}
@misc{inter_font,
  title  = {Inter Font Family v3.19},
  author = {Andersson, Rasmus and others},
  year   = {2023},
  url    = {https://github.com/rsms/inter}
}
@misc{lv_font_conv,
  title  = {lv\_font\_conv: Tool for converting fonts to LVGL bitmap format},
  author = {{LVGL Project}},
  year   = {2024},
  url    = {https://github.com/lvgl/lv_font_conv}
}
@manual{espidf_sleep,
  title  = {ESP-IDF Programming Guide v5.x — Sleep Modes},
  author = {{Espressif Systems}},
  year   = {2024},
  url    = {https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html}
}
@misc{hive_db,
  title  = {Hive: Lightweight and blazing fast key-value database for Flutter},
  author = {{Hive Authors}},
  year   = {2024},
  url    = {https://pub.dev/packages/hive}
}
```

Mantén las claves ya existentes del E2 (`ble_core_spec`, `ieee11073`, `usbpd`, `iec60601`, `rohs`, `en62133`, `ecma287`, `encuesta_af`, `obesidad_chile`, `minsalwearables`, `Shin2019`, `Kushlev2016`, `Pielot2014`, `samsung_bioactive`, `kazemi_cnn`, `motion_artifact_svd`, `fitbit_afib`, `esp32c3_ds`) porque varias se siguen referenciando.

Compila con `\bibliographystyle{unsrtnat}` (consistente con `natbib[numbers]` del E2).

## 7. Decisiones de estilo que NO debes cambiar

- **No reproducir** el análisis de impacto socioeconómico/ambiental/ético ni la sección de estándares (ECMA-287, IEEE 11073, USB-C, IEC 60601-1, RoHS, IEC 62133-2). El MD del avance 3 explícitamente **referencia** esos análisis del avance 2 y los considera vigentes. No los traigas de vuelta.
- **No agregues** secciones nuevas (set-up final de pruebas, modelos de autonomía detallados, comparativa con industria) que no estén en el MD del avance 3. El plan original del E2 ya cubrió esos puntos y este informe solo entrega los *deltas*.
- **Mantén la voz**: el MD habla en primera persona del plural y en términos técnicos sin marketing. No agregues adjetivos ni interpretaciones.
- **No inventes datos numéricos** que no estén en el MD. Si un número parece faltar (e.g. corriente de un escenario no listado), déjalo como `TBD` o pide aclaración antes de inventar.

## 8. Compilación esperada

```bash
cd docs/Entrega3/
pdflatex -interaction=nonstopmode main.tex
bibtex main
pdflatex -interaction=nonstopmode main.tex
pdflatex -interaction=nonstopmode main.tex
```

o equivalentemente `latexmk -pdf main.tex`. Los archivos auxiliares `.aux`, `.bbl`, `.log`, `.toc`, `.lof`, `.lot`, `.out` se generarán y deben quedar en `docs/Entrega3/`. El PDF objetivo es `docs/Entrega3/main.pdf`.

**Salidas que validan el éxito:**

- 30 a 38 páginas (el MD tiene ~16 000 palabras, equivalente a ese rango en *article* 11pt con márgenes E2).
- Índice generado completo, con todas las secciones 1–6, anexos A.1–A.9 y referencias.
- 4 a 8 figuras incluidas (mínimo: diagrama de bloques 2.1, Gantt 3.1, render carcasa 4.6, PCB front+back 4.5).
- 8 a 12 tablas (mínimo: 2.1 specs, 3.1 planificación, 3.2 distribución de trabajo, 4.4.3 capas CNN, 5.1 consumo, 5.2 accuracy HAR, 5.4 DRC, A.4 cotas mecánicas).
- 4 a 6 listings (extracto pinmap A.1, FFT A.2, HAR API A.3, fragmentos en línea opcionales).
- Bibliografía con `[1]` … `[N]` resuelta, sin warnings de citas indefinidas.

## 9. Recursos a verificar antes de empezar

```bash
ls docs/Entrega3/
# Debe existir: entrega_3.md (fuente), fig_bloques_lownivel_e3.tex, gantt.jpeg,
#               pcb_front-1.png, pcb_back-1.png, bosquejo.jpeg, presentacion3.tex,
#               funcionamiento_ml.md (referencia técnica del modelo CNN).
ls docs/Entrega2/main.tex docs/Entrega2/ref.bib docs/Entrega2/logo.pdf
# Plantilla y bibliografía base.
ls mechanical/render_v2_*.png hardware/SupaClock_Carrier/SupaClock_Carrier_v1_*.pdf
# Renders y placement PDFs del PCB, accesibles vía graphicspath.
```

Si falta `logo.pdf` o algún `.png`, copia el del directorio Entrega2 a Entrega3 manteniendo el nombre.

## 10. Checklist final antes de cerrar el handoff

- [ ] `main.tex` creado en `docs/Entrega3/`.
- [ ] `ref.bib` creado en `docs/Entrega3/` con claves del E2 + nuevas (sección 6).
- [ ] `logo.pdf` disponible en `docs/Entrega3/`.
- [ ] El PDF compila sin errores y sin `Undefined control sequence` / `Citation undefined`.
- [ ] Índice, lista de figuras y lista de tablas se generan completos.
- [ ] Hipervínculos `\ref`/`\cite` funcionan (los `??` en el PDF significan que faltaron `\label` o `\cite` keys).
- [ ] No se duplica contenido del avance 2 (impacto, estándares, modelos de autonomía detallados).
- [ ] Las métricas numéricas del MD aparecen idénticas en el LaTeX.
- [ ] El total de páginas está entre 30 y 38.

Cuando termines, deja un commit con mensaje:

```
docs(Entrega3): convert markdown report to LaTeX (main.tex + ref.bib)
```

y reporta al usuario:

1. Número de páginas final del PDF.
2. Cantidad de figuras, tablas y listings incluidos.
3. Warnings residuales de la compilación (e.g. *overfull hbox*) que valga la pena revisar a mano.
