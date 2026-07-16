// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const CONTENT_TYPES_XML = `
    <Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
        <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
        <Default Extension="xml" ContentType="application/xml"/>
        <Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>
        <Override PartName="/xl/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>
        <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
        <Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>
        <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
    </Types>
`;
const CONTENT_TYPES_NS = 'http://schemas.openxmlformats.org/package/2006/content-types';

const RELS_XML = `
    <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
        <Relationship Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml" Id="rId1"/>
        <Relationship Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml" Id="rId2"/>
        <Relationship Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml" Id="rId3"/>
    </Relationships>
`;
const RELS_WORKBOOK_XML = `
    <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
        <Relationship Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml" Id="rId1"/>
        <Relationship Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme" Target="theme/theme1.xml" Id="rId2"/>
    </Relationships>
`;

const CORE_XML = `
    <cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties">
        <dc:creator xmlns:dc="http://purl.org/dc/elements/1.1/">openpyxl</dc:creator>
        <dcterms:created xmlns:dcterms="http://purl.org/dc/terms/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="dcterms:W3CDTF">2026-07-15T09:35:37Z</dcterms:created>
        <dcterms:modified xmlns:dcterms="http://purl.org/dc/terms/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="dcterms:W3CDTF">2026-07-15T09:35:37Z</dcterms:modified>
    </cp:coreProperties>
`;
const APP_XML = `
    <Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties">
        <Application>Microsoft Excel Compatible</Application>
        <AppVersion></AppVersion>
    </Properties>
`;

const THEME_XML = `
    <a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="Office Theme">
        <a:themeElements>
            <a:clrScheme name="Office">
                <a:dk1>
                    <a:sysClr val="windowText" lastClr="000000"/>
                </a:dk1>
                <a:lt1>
                    <a:sysClr val="window" lastClr="FFFFFF"/>
                </a:lt1>
                <a:dk2>
                    <a:srgbClr val="1F497D"/>
                </a:dk2>
                <a:lt2>
                    <a:srgbClr val="EEECE1"/>
                </a:lt2>
                <a:accent1>
                    <a:srgbClr val="4F81BD"/>
                </a:accent1>
                <a:accent2>
                    <a:srgbClr val="C0504D"/>
                </a:accent2>
                <a:accent3>
                    <a:srgbClr val="9BBB59"/>
                </a:accent3>
                <a:accent4>
                    <a:srgbClr val="8064A2"/>
                </a:accent4>
                <a:accent5>
                    <a:srgbClr val="4BACC6"/>
                </a:accent5>
                <a:accent6>
                    <a:srgbClr val="F79646"/>
                </a:accent6>
                <a:hlink>
                    <a:srgbClr val="0000FF"/>
                </a:hlink>
                <a:folHlink>
                    <a:srgbClr val="800080"/>
                </a:folHlink>
            </a:clrScheme>
            <a:fontScheme name="Office">
                <a:majorFont>
                    <a:latin typeface="Cambria"/>
                    <a:ea typeface=""/>
                    <a:cs typeface=""/>
                    <a:font script="Jpan" typeface="ＭＳ Ｐゴシック"/>
                    <a:font script="Hang" typeface="맑은 고딕"/>
                    <a:font script="Hans" typeface="宋体"/>
                    <a:font script="Hant" typeface="新細明體"/>
                    <a:font script="Arab" typeface="Times New Roman"/>
                    <a:font script="Hebr" typeface="Times New Roman"/>
                    <a:font script="Thai" typeface="Tahoma"/>
                    <a:font script="Ethi" typeface="Nyala"/>
                    <a:font script="Beng" typeface="Vrinda"/>
                    <a:font script="Gujr" typeface="Shruti"/>
                    <a:font script="Khmr" typeface="MoolBoran"/>
                    <a:font script="Knda" typeface="Tunga"/>
                    <a:font script="Guru" typeface="Raavi"/>
                    <a:font script="Cans" typeface="Euphemia"/>
                    <a:font script="Cher" typeface="Plantagenet Cherokee"/>
                    <a:font script="Yiii" typeface="Microsoft Yi Baiti"/>
                    <a:font script="Tibt" typeface="Microsoft Himalaya"/>
                    <a:font script="Thaa" typeface="MV Boli"/>
                    <a:font script="Deva" typeface="Mangal"/>
                    <a:font script="Telu" typeface="Gautami"/>
                    <a:font script="Taml" typeface="Latha"/>
                    <a:font script="Syrc" typeface="Estrangelo Edessa"/>
                    <a:font script="Orya" typeface="Kalinga"/>
                    <a:font script="Mlym" typeface="Kartika"/>
                    <a:font script="Laoo" typeface="DokChampa"/>
                    <a:font script="Sinh" typeface="Iskoola Pota"/>
                    <a:font script="Mong" typeface="Mongolian Baiti"/>
                    <a:font script="Viet" typeface="Times New Roman"/>
                    <a:font script="Uigh" typeface="Microsoft Uighur"/>
                </a:majorFont>
                <a:minorFont>
                    <a:latin typeface="Calibri"/>
                    <a:ea typeface=""/>
                    <a:cs typeface=""/>
                    <a:font script="Jpan" typeface="ＭＳ Ｐゴシック"/>
                    <a:font script="Hang" typeface="맑은 고딕"/>
                    <a:font script="Hans" typeface="宋体"/>
                    <a:font script="Hant" typeface="新細明體"/>
                    <a:font script="Arab" typeface="Arial"/>
                    <a:font script="Hebr" typeface="Arial"/>
                    <a:font script="Thai" typeface="Tahoma"/>
                    <a:font script="Ethi" typeface="Nyala"/>
                    <a:font script="Beng" typeface="Vrinda"/>
                    <a:font script="Gujr" typeface="Shruti"/>
                    <a:font script="Khmr" typeface="DaunPenh"/>
                    <a:font script="Knda" typeface="Tunga"/>
                    <a:font script="Guru" typeface="Raavi"/>
                    <a:font script="Cans" typeface="Euphemia"/>
                    <a:font script="Cher" typeface="Plantagenet Cherokee"/>
                    <a:font script="Yiii" typeface="Microsoft Yi Baiti"/>
                    <a:font script="Tibt" typeface="Microsoft Himalaya"/>
                    <a:font script="Thaa" typeface="MV Boli"/>
                    <a:font script="Deva" typeface="Mangal"/>
                    <a:font script="Telu" typeface="Gautami"/>
                    <a:font script="Taml" typeface="Latha"/>
                    <a:font script="Syrc" typeface="Estrangelo Edessa"/>
                    <a:font script="Orya" typeface="Kalinga"/>
                    <a:font script="Mlym" typeface="Kartika"/>
                    <a:font script="Laoo" typeface="DokChampa"/>
                    <a:font script="Sinh" typeface="Iskoola Pota"/>
                    <a:font script="Mong" typeface="Mongolian Baiti"/>
                    <a:font script="Viet" typeface="Arial"/>
                    <a:font script="Uigh" typeface="Microsoft Uighur"/>
                </a:minorFont>
            </a:fontScheme>
            <a:fmtScheme name="Office">
                <a:fillStyleLst>
                    <a:solidFill>
                        <a:schemeClr val="phClr"/>
                    </a:solidFill>
                    <a:gradFill rotWithShape="1">
                        <a:gsLst>
                            <a:gs pos="0">
                                <a:schemeClr val="phClr">
                                    <a:tint val="50000"/>
                                    <a:satMod val="300000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="35000">
                                <a:schemeClr val="phClr">
                                    <a:tint val="37000"/>
                                    <a:satMod val="300000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="100000">
                                <a:schemeClr val="phClr">
                                    <a:tint val="15000"/>
                                    <a:satMod val="350000"/>
                                </a:schemeClr>
                            </a:gs>
                        </a:gsLst>
                        <a:lin ang="16200000" scaled="1"/>
                    </a:gradFill>
                    <a:gradFill rotWithShape="1">
                        <a:gsLst>
                            <a:gs pos="0">
                                <a:schemeClr val="phClr">
                                    <a:shade val="51000"/>
                                    <a:satMod val="130000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="80000">
                                <a:schemeClr val="phClr">
                                    <a:shade val="93000"/>
                                    <a:satMod val="130000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="100000">
                                <a:schemeClr val="phClr">
                                    <a:shade val="94000"/>
                                    <a:satMod val="135000"/>
                                </a:schemeClr>
                            </a:gs>
                        </a:gsLst>
                        <a:lin ang="16200000" scaled="0"/>
                    </a:gradFill>
                </a:fillStyleLst>
                <a:lnStyleLst>
                    <a:ln w="9525" cap="flat" cmpd="sng" algn="ctr">
                        <a:solidFill>
                            <a:schemeClr val="phClr">
                                <a:shade val="95000"/>
                                <a:satMod val="105000"/>
                            </a:schemeClr>
                        </a:solidFill>
                        <a:prstDash val="solid"/>
                    </a:ln>
                    <a:ln w="25400" cap="flat" cmpd="sng" algn="ctr">
                        <a:solidFill>
                            <a:schemeClr val="phClr"/>
                        </a:solidFill>
                        <a:prstDash val="solid"/>
                    </a:ln>
                    <a:ln w="38100" cap="flat" cmpd="sng" algn="ctr">
                        <a:solidFill>
                            <a:schemeClr val="phClr"/>
                        </a:solidFill>
                        <a:prstDash val="solid"/>
                    </a:ln>
                </a:lnStyleLst>
                <a:effectStyleLst>
                    <a:effectStyle>
                        <a:effectLst>
                            <a:outerShdw blurRad="40000" dist="20000" dir="5400000" rotWithShape="0">
                                <a:srgbClr val="000000">
                                    <a:alpha val="38000"/>
                                </a:srgbClr>
                            </a:outerShdw>
                        </a:effectLst>
                    </a:effectStyle>
                    <a:effectStyle>
                        <a:effectLst>
                            <a:outerShdw blurRad="40000" dist="23000" dir="5400000" rotWithShape="0">
                                <a:srgbClr val="000000">
                                    <a:alpha val="35000"/>
                                </a:srgbClr>
                            </a:outerShdw>
                        </a:effectLst>
                    </a:effectStyle>
                    <a:effectStyle>
                        <a:effectLst>
                            <a:outerShdw blurRad="40000" dist="23000" dir="5400000" rotWithShape="0">
                                <a:srgbClr val="000000">
                                    <a:alpha val="35000"/>
                                </a:srgbClr>
                            </a:outerShdw>
                        </a:effectLst>
                        <a:scene3d>
                            <a:camera prst="orthographicFront">
                                <a:rot lat="0" lon="0" rev="0"/>
                            </a:camera>
                            <a:lightRig rig="threePt" dir="t">
                                <a:rot lat="0" lon="0" rev="1200000"/>
                            </a:lightRig>
                        </a:scene3d>
                        <a:sp3d>
                            <a:bevelT w="63500" h="25400"/>
                        </a:sp3d>
                    </a:effectStyle>
                </a:effectStyleLst>
                <a:bgFillStyleLst>
                    <a:solidFill>
                        <a:schemeClr val="phClr"/>
                    </a:solidFill>
                    <a:gradFill rotWithShape="1">
                        <a:gsLst>
                            <a:gs pos="0">
                                <a:schemeClr val="phClr">
                                    <a:tint val="40000"/>
                                    <a:satMod val="350000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="40000">
                                <a:schemeClr val="phClr">
                                    <a:tint val="45000"/>
                                    <a:shade val="99000"/>
                                    <a:satMod val="350000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="100000">
                                <a:schemeClr val="phClr">
                                    <a:shade val="20000"/>
                                    <a:satMod val="255000"/>
                                </a:schemeClr>
                            </a:gs>
                        </a:gsLst>
                        <a:path path="circle">
                            <a:fillToRect l="50000" t="-80000" r="50000" b="180000"/>
                        </a:path>
                    </a:gradFill>
                    <a:gradFill rotWithShape="1">
                        <a:gsLst>
                            <a:gs pos="0">
                                <a:schemeClr val="phClr">
                                    <a:tint val="80000"/>
                                    <a:satMod val="300000"/>
                                </a:schemeClr>
                            </a:gs>
                            <a:gs pos="100000">
                                <a:schemeClr val="phClr">
                                    <a:shade val="30000"/>
                                    <a:satMod val="200000"/>
                                </a:schemeClr>
                            </a:gs>
                        </a:gsLst>
                        <a:path path="circle">
                            <a:fillToRect l="50000" t="50000" r="50000" b="50000"/>
                        </a:path>
                    </a:gradFill>
                </a:bgFillStyleLst>
            </a:fmtScheme>
        </a:themeElements>
        <a:objectDefaults/>
        <a:extraClrSchemeLst/>
    </a:theme>
`;
const STYLES_XML = `
    <styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
        <numFmts count="0"/>
        <fonts count="1">
            <font>
                <name val="Calibri"/>
                <family val="2"/>
                <color theme="1"/>
                <sz val="11"/>
                <scheme val="minor"/>
            </font>
        </fonts>
        <fills count="2">
            <fill>
                <patternFill/>
            </fill>
            <fill>
                <patternFill patternType="gray125"/>
            </fill>
        </fills>
        <borders count="1">
            <border>
                <left/>
                <right/>
                <top/>
                <bottom/>
                <diagonal/>
            </border>
        </borders>
        <cellStyleXfs count="1">
            <xf numFmtId="0" fontId="0" fillId="0" borderId="0"/>
        </cellStyleXfs>
        <cellXfs count="1">
            <xf numFmtId="0" fontId="0" fillId="0" borderId="0" pivotButton="0" quotePrefix="0" xfId="0"/>
        </cellXfs>
        <cellStyles count="1">
            <cellStyle name="Normal" xfId="0" builtinId="0" hidden="0"/>
        </cellStyles>
        <tableStyles count="0" defaultTableStyle="TableStyleMedium9" defaultPivotStyle="PivotStyleLight16"/>
        <colors>
            <indexedColors>
                <rgbColor rgb="00000000"/>
                <rgbColor rgb="00FFFFFF"/>
                <rgbColor rgb="00FF0000"/>
                <rgbColor rgb="0000FF00"/>
                <rgbColor rgb="000000FF"/>
                <rgbColor rgb="00FFFF00"/>
                <rgbColor rgb="00FF00FF"/>
                <rgbColor rgb="0000FFFF"/>
                <rgbColor rgb="00000000"/>
                <rgbColor rgb="00FFFFFF"/>
                <rgbColor rgb="00FF0000"/>
                <rgbColor rgb="0000FF00"/>
                <rgbColor rgb="000000FF"/>
                <rgbColor rgb="00FFFF00"/>
                <rgbColor rgb="00FF00FF"/>
                <rgbColor rgb="0000FFFF"/>
                <rgbColor rgb="00800000"/>
                <rgbColor rgb="00008000"/>
                <rgbColor rgb="00000080"/>
                <rgbColor rgb="00808000"/>
                <rgbColor rgb="00800080"/>
                <rgbColor rgb="00008080"/>
                <rgbColor rgb="00C0C0C0"/>
                <rgbColor rgb="00808080"/>
                <rgbColor rgb="009999FF"/>
                <rgbColor rgb="00993366"/>
                <rgbColor rgb="00FFFFCC"/>
                <rgbColor rgb="00CCFFFF"/>
                <rgbColor rgb="00660066"/>
                <rgbColor rgb="00FF8080"/>
                <rgbColor rgb="000066CC"/>
                <rgbColor rgb="00CCCCFF"/>
                <rgbColor rgb="00000080"/>
                <rgbColor rgb="00FF00FF"/>
                <rgbColor rgb="00FFFF00"/>
                <rgbColor rgb="0000FFFF"/>
                <rgbColor rgb="00800080"/>
                <rgbColor rgb="00800000"/>
                <rgbColor rgb="00008080"/>
                <rgbColor rgb="000000FF"/>
                <rgbColor rgb="0000CCFF"/>
                <rgbColor rgb="00CCFFFF"/>
                <rgbColor rgb="00CCFFCC"/>
                <rgbColor rgb="00FFFF99"/>
                <rgbColor rgb="0099CCFF"/>
                <rgbColor rgb="00FF99CC"/>
                <rgbColor rgb="00CC99FF"/>
                <rgbColor rgb="00FFCC99"/>
                <rgbColor rgb="003366FF"/>
                <rgbColor rgb="0033CCCC"/>
                <rgbColor rgb="0099CC00"/>
                <rgbColor rgb="00FFCC00"/>
                <rgbColor rgb="00FF9900"/>
                <rgbColor rgb="00FF6600"/>
                <rgbColor rgb="00666699"/>
                <rgbColor rgb="00969696"/>
                <rgbColor rgb="00003366"/>
                <rgbColor rgb="00339966"/>
                <rgbColor rgb="00003300"/>
                <rgbColor rgb="00333300"/>
                <rgbColor rgb="00993300"/>
                <rgbColor rgb="00993366"/>
                <rgbColor rgb="00333399"/>
                <rgbColor rgb="00333333"/>
            </indexedColors>
        </colors>
    </styleSheet>
`;

const WORKBOOK_XML = `
    <workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
        <workbookPr/>
        <workbookProtection/>
        <bookViews>
            <workbookView visibility="visible" minimized="0" showHorizontalScroll="1" showVerticalScroll="1" showSheetTabs="1" tabRatio="600" firstSheet="0" activeTab="0" autoFilterDateGrouping="1"/>
        </bookViews>
        <sheets>
            <sheet xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" name="Sheet1" sheetId="1" state="visible" r:id="rId1"/>
        </sheets>
        <definedNames/>
        <calcPr calcId="124519" fullCalcOnLoad="1"/>
    </workbook>
`;
const WORKSHEET_XML = `
    <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
        <sheetPr>
            <outlinePr summaryBelow="1" summaryRight="1"/>
            <pageSetUpPr/>
        </sheetPr>
        <dimension ref="A1:A1"/>
        <sheetViews>
            <sheetView workbookViewId="0">
                <selection activeCell="A1" sqref="A1"/>
            </sheetView>
        </sheetViews>
        <sheetFormatPr baseColWidth="8" defaultRowHeight="15"/>
        <sheetData/>
        <pageMargins left="0.75" right="0.75" top="1" bottom="1" header="0.5" footer="0.5"/>
    </worksheet>
`;

const RELS_NS = 'http://schemas.openxmlformats.org/package/2006/relationships';
const REL_NS = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships';
const REL_WORKSHEET_NS = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet';

const WORKSHEET_NS = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main';
const WORKSHEET_TYPE = 'application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml';

export {
    CONTENT_TYPES_XML,
    CONTENT_TYPES_NS,

    RELS_XML,
    RELS_WORKBOOK_XML,

    CORE_XML,
    APP_XML,

    THEME_XML,
    STYLES_XML,
    WORKBOOK_XML,
    WORKSHEET_XML,

    RELS_NS,
    REL_NS,
    REL_WORKSHEET_NS,

    WORKSHEET_NS,
    WORKSHEET_TYPE
}
