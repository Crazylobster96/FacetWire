# SPDX-License-Identifier: MPL-2.0
"""Generate an original two-page PDF regression fixture (requires reportlab)."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor

out = Path(__file__).resolve().parents[1] / 'examples/placeholder_demo/assets/document-import/sample.pdf'
out.parent.mkdir(parents=True, exist_ok=True)
c = canvas.Canvas(str(out), pagesize=(595, 842), invariant=1)
c.setTitle('FacetWire fixed-page import fixture')
for page in (1, 2):
    c.setFillColor(HexColor('#303844'))
    c.setFont('Helvetica-Bold', 24)
    c.drawString(48, 774, 'FacetWire PDF Preview')
    c.setFont('Helvetica', 12)
    c.drawString(48, 744, f'Page {page} / 2 - fixed geometry, text and vector graphics')
    c.drawString(48, 714, 'No background rectangle: uncovered pixels remain transparent.')
    c.setFillColor(HexColor('#537DA2'))
    for index, height in enumerate((80, 140, 115, 200)):
        c.roundRect(60 + index * 120, 420, 65, height + (page - 1) * 20, 6, fill=1, stroke=0)
        c.setFillColor(HexColor('#303844'))
        c.drawString(74 + index * 120, 398, f'Q{index+1}')
        c.setFillColor(HexColor('#537DA2'))
    c.setFillColor(HexColor('#D49B62'))
    c.circle(100, 280, 32 + page * 4, fill=1, stroke=0)
    c.setFillColor(HexColor('#303844'))
    c.drawString(160, 280, 'Original test content - no external resources.')
    c.drawString(48, 80, f'FacetWire / source-aware document profile / page {page}')
    c.showPage()
c.save()
print(out)
