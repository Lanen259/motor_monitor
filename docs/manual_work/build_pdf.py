# -*- coding: utf-8 -*-
"""把 manual_content.md 构建为《电机自动化平台详细使用说明书》PDF。
依赖: reportlab, Pillow。用法: python build_pdf.py
"""
import os
import re
import sys

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
                                Spacer, Image, Table, TableStyle, PageBreak,
                                NextPageTemplate)
from reportlab.platypus.tableofcontents import TableOfContents
from PIL import Image as PILImage

BASE = os.path.dirname(os.path.abspath(__file__))
MD_PATH = os.path.join(BASE, "manual_content.md")
OUT_PATH = os.path.join(os.path.dirname(BASE), "电机自动化平台详细使用说明书.pdf")

FONT = "msyh"
FONT_B = "msyhbd"
WIN_FONTS = r"C:\Windows\Fonts"
pdfmetrics.registerFont(TTFont(FONT, os.path.join(WIN_FONTS, "msyh.ttc"), subfontIndex=0))
pdfmetrics.registerFont(TTFont(FONT_B, os.path.join(WIN_FONTS, "msyhbd.ttc"), subfontIndex=0))

PAGE_W, PAGE_H = A4
MARGIN = 18 * mm
USABLE = PAGE_W - 2 * MARGIN

S_TITLE = ParagraphStyle("title", fontName=FONT_B, fontSize=26, leading=34,
                         alignment=TA_CENTER, textColor=colors.HexColor("#1565C0"))
S_SUB = ParagraphStyle("sub", fontName=FONT, fontSize=12, leading=18,
                       alignment=TA_CENTER, textColor=colors.HexColor("#607D8B"))
S_H1 = ParagraphStyle("h1", fontName=FONT_B, fontSize=18, leading=26, spaceBefore=10,
                      spaceAfter=10, textColor=colors.HexColor("#1565C0"))
S_H2 = ParagraphStyle("h2", fontName=FONT_B, fontSize=14, leading=20, spaceBefore=10,
                      spaceAfter=6, textColor=colors.HexColor("#1976D2"))
S_H3 = ParagraphStyle("h3", fontName=FONT_B, fontSize=12, leading=17, spaceBefore=8,
                      spaceAfter=4, textColor=colors.HexColor("#37474F"))
S_BODY = ParagraphStyle("body", fontName=FONT, fontSize=10, leading=16, spaceAfter=4)
S_BULLET = ParagraphStyle("bullet", fontName=FONT, fontSize=10, leading=16,
                          leftIndent=14, spaceAfter=2, bulletIndent=4)
S_CODE = ParagraphStyle("code", fontName=FONT, fontSize=8.5, leading=12.5,
                        backColor=colors.HexColor("#F5F7FA"), borderColor=colors.HexColor("#E0E0E0"),
                        borderWidth=0.5, borderPadding=4, leftIndent=6, rightIndent=6,
                        spaceAfter=6, textColor=colors.HexColor("#263238"))
S_CELL = ParagraphStyle("cell", fontName=FONT, fontSize=8.5, leading=12)
S_CELLH = ParagraphStyle("cellh", fontName=FONT_B, fontSize=8.5, leading=12,
                         textColor=colors.HexColor("#1565C0"))
S_CAP = ParagraphStyle("cap", fontName=FONT, fontSize=8.5, leading=12,
                       alignment=TA_CENTER, textColor=colors.HexColor("#78909C"),
                       spaceAfter=8)
S_TOC1 = ParagraphStyle("toc1", fontName=FONT_B, fontSize=11, leading=18)
S_TOC2 = ParagraphStyle("toc2", fontName=FONT, fontSize=10, leading=15, leftIndent=16)


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def inline(t):
    t = esc(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"`([^`]+)`", r'<font color="#C62828">\1</font>', t)
    return t


class ManualDoc(BaseDocTemplate):
    def __init__(self, filename, **kw):
        BaseDocTemplate.__init__(self, filename, **kw)
        frame = Frame(MARGIN, MARGIN, USABLE, PAGE_H - 2 * MARGIN, id="main")
        self.addPageTemplates([PageTemplate(id="all", frames=[frame], onPage=self._footer)])
        self.toc = TableOfContents()
        self.toc.levelStyles = [S_TOC1, S_TOC2]

    def _footer(self, canvas, doc):
        canvas.saveState()
        canvas.setFont(FONT, 8)
        canvas.setFillColor(colors.HexColor("#90A4AE"))
        canvas.drawString(MARGIN, 10 * mm, "电机自动化平台 · 详细使用说明书")
        canvas.drawRightString(PAGE_W - MARGIN, 10 * mm, "第 %d 页" % doc.page)
        canvas.setStrokeColor(colors.HexColor("#E0E0E0"))
        canvas.line(MARGIN, 13 * mm, PAGE_W - MARGIN, 13 * mm)
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph):
            name = flowable.style.name
            txt = re.sub(r"<[^>]+>", "", flowable.getPlainText())
            if name == "h1":
                self.notify("TOCEntry", (0, txt, self.page))
            elif name == "h2":
                self.notify("TOCEntry", (1, txt, self.page))


def image_flowable(relpath):
    path = os.path.join(BASE, relpath)
    if not os.path.exists(path):
        return Paragraph("[缺失图片: %s]" % esc(relpath), S_CAP)
    with PILImage.open(path) as im:
        w, h = im.size
    dialog = any(k in relpath for k in ("06_", "07_", "08_", "09_"))
    max_w = (110 * mm) if dialog else (USABLE - 4 * mm)
    max_h = 150 * mm
    scale = min(max_w / w, max_h / h)
    return Image(path, width=w * scale, height=h * scale)


def build_table(rows):
    data = []
    for r in rows:
        cells = [c.strip() for c in r.strip().strip("|").split("|")]
        data.append(cells)
    if len(data) >= 2 and all(re.match(r"^:?-+:?$", c) for c in data[1] if c):
        header, body = data[0], data[2:]
    else:
        header, body = data[0], data[1:]
    ncols = len(header)
    table = [[Paragraph(inline(c), S_CELLH) for c in header]]
    for r in body:
        r = (r + [""] * ncols)[:ncols]
        table.append([Paragraph(inline(c), S_CELL) for c in r])
    colw = USABLE / ncols
    t = Table(table, colWidths=[colw] * ncols, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#E3F2FD")),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#CFD8DC")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1),
         [colors.white, colors.HexColor("#FAFBFC")]),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return t


def parse_md(text):
    story = []
    # 封面
    story.append(Spacer(1, 60 * mm))
    story.append(Paragraph("电机自动化平台", S_TITLE))
    story.append(Paragraph("详细使用说明书", S_TITLE))
    story.append(Spacer(1, 10 * mm))
    story.append(Paragraph("Motor Automation Studio — 功能 · 协议 · 操作 完整指南", S_SUB))
    story.append(Spacer(1, 6 * mm))
    story.append(Paragraph("版本 v1.0 · 2026-08-07", S_SUB))
    story.append(PageBreak())
    story.append(Paragraph("目录", S_H1))
    story.append(doc_toc_placeholder)
    story.append(PageBreak())

    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if not line.strip():
            i += 1
            continue
        if line.startswith("```"):
            buf = []
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1
            code = esc("\n".join(buf)).replace("\n", "<br/>").replace(" ", "&nbsp;")
            story.append(Paragraph(code, S_CODE))
            continue
        m = re.match(r"^!\[(.*?)\]\((.*?)\)", line.strip())
        if m:
            story.append(Spacer(1, 2))
            story.append(image_flowable(m.group(2).strip()))
            if m.group(1).strip():
                story.append(Paragraph("图: %s" % esc(m.group(1).strip()), S_CAP))
            i += 1
            continue
        if line.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append(lines[i])
                i += 1
            story.append(build_table(rows))
            story.append(Spacer(1, 4))
            continue
        if line.startswith("# "):
            story.append(PageBreak())
            story.append(Paragraph(inline(line[2:]), S_H1))
            i += 1
            continue
        if line.startswith("## "):
            story.append(Paragraph(inline(line[3:]), S_H2))
            i += 1
            continue
        if line.startswith("### "):
            story.append(Paragraph(inline(line[4:]), S_H3))
            i += 1
            continue
        if line.startswith("- ") or line.startswith("* "):
            story.append(Paragraph(inline(line[2:]), S_BULLET, bulletText="•"))
            i += 1
            continue
        if re.match(r"^---+\s*$", line):
            i += 1
            continue
        # 普通段落(合并续行)
        buf = [line]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(
                r"^(#|```|\||!|- |\*)", lines[i].strip()):
            buf.append(lines[i].strip())
            i += 1
        story.append(Paragraph(inline(" ".join(buf)), S_BODY))
    return story


class TocPlaceholder(object):
    pass


doc_toc_placeholder = None  # replaced in main


def main():
    global doc_toc_placeholder
    with open(MD_PATH, "r", encoding="utf-8") as f:
        text = f.read()

    doc = ManualDoc(OUT_PATH, pagesize=A4, title="电机自动化平台详细使用说明书",
                    author="Motor Automation Studio")
    doc_toc_placeholder = doc.toc
    story = parse_md(text)
    doc.multiBuild(story)
    print("PDF written:", OUT_PATH)


if __name__ == "__main__":
    main()
