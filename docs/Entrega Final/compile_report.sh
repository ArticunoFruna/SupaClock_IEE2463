#!/bin/bash
# Script para compilar el informe final resolviendo referencias y bibliografía.

set -e

echo "=== Paso 1: pdflatex ==="
pdflatex -interaction=nonstopmode informe_final.tex

echo "=== Paso 2: bibtex ==="
bibtex informe_final

echo "=== Paso 3: pdflatex (1) ==="
pdflatex -interaction=nonstopmode informe_final.tex

echo "=== Paso 4: pdflatex (2) ==="
pdflatex -interaction=nonstopmode informe_final.tex

echo "=== Compilación completada con éxito ==="
