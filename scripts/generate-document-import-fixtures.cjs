// SPDX-License-Identifier: MPL-2.0
// Requires docx (development tool only; never shipped in the application).
const fs = require('node:fs');
const path = require('node:path');
const {Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  ImageRun, HeadingLevel, WidthType, ShadingType, BorderStyle} = require('docx');
const root = path.resolve(__dirname, '..');
const out = path.join(root, 'examples/placeholder_demo/assets/document-import');
fs.mkdirSync(out, {recursive: true});
const image = fs.readFileSync(path.join(root, 'examples/placeholder_demo/assets/documents/rich-media-showcase.agscene/resources/showcase-image.png'));
const border = {style: BorderStyle.SINGLE, size: 4, color: 'A7B3C2'};
const children = [
  new Paragraph({heading: HeadingLevel.HEADING_1, children: [new TextRun('FacetWire Document Import')]}),
  new Paragraph({children: [new TextRun('中文段落：这是可追溯的 DOCX 结构化导入。'), new TextRun({text: ' Bold / 加粗', bold: true}), new TextRun({text: ' Italic', italics: true})]}),
  new Paragraph({children: [new ImageRun({type: 'png', data: image, transformation: {width: 300, height: 180},
    altText: {title: 'Embedded illustration', description: 'FacetWire local image fixture', name: 'fixture-image'}})]}),
  new Table({width: {size: 9360, type: WidthType.DXA}, columnWidths: [4680,4680], rows:
    [['Feature','Status'],['Paragraph / 段落','Structured'],['Image / 图片','Embedded'],['Pagination / 分页','Native Flow']].map((row,index) => new TableRow({children: row.map(text => new TableCell({width: {size:4680,type:WidthType.DXA}, borders: {top:border,left:border,bottom:border,right:border},
      shading: {fill:index===0?'EAF0F6':'FFFFFF',type:ShadingType.CLEAR}, margins:{top:80,bottom:80,left:120,right:120}, children:[new Paragraph(text)]}))}))}),
  new Paragraph({pageBreakBefore:true, heading:HeadingLevel.HEADING_1, children:[new TextRun('Second page / 第二页')]}),
  new Paragraph('This paragraph begins after an explicit page break. The original source document is never overwritten.'),
  ...Array.from({length:12},(_,i)=>new Paragraph(`Paragraph ${i+1}: Flow should preserve source order and show all text. Stable source IDs identify the package part and structural location.`)),
];
const document = new Document({styles:{default:{document:{run:{font:'Arial',size:24}}}, paragraphStyles:[
  {id:'Heading1', name:'Heading 1', basedOn:'Normal', next:'Normal', quickFormat:true,
    run:{font:'Arial',size:36,bold:true}, paragraph:{outlineLevel:0,spacing:{before:120,after:120}}}]},
  sections:[{properties:{page:{size:{width:12240,height:15840},margin:{top:1440,right:1440,bottom:1440,left:1440}}},children}]});
Packer.toBuffer(document).then(bytes=>{fs.writeFileSync(path.join(out,'sample.docx'),bytes); console.log('DOCX fixture generated');});
