import re

with open("main.tex", "r") as f:
    content = f.read()

# Extract preamble (everything before \begin{document})
preamble = content.split("\\begin{document}")[0]

# Replace document class to standalone
preamble = re.sub(r"\\documentclass\[.*?\]{.*?}", "\\\\documentclass[tikz,border=1cm]{standalone}\n\\\\usepackage{xcolor}\n\\\\usepackage{amsmath,amssymb}", preamble)

# We want to extract tikzpictures.
tikz_blocks = re.findall(r"\\begin{tikzpicture}.*?\\end{tikzpicture}", content, re.DOTALL)

for i, block in enumerate(tikz_blocks):
    with open(f"fig_{i}.tex", "w") as f:
        f.write(preamble + "\n\\begin{document}\n" + block + "\n\\end{document}\n")

print(f"Extracted {len(tikz_blocks)} diagrams.")
