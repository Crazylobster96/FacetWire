# SPDX-License-Identifier: MPL-2.0
[CmdletBinding()]
param(
    [string]$SourcePath = '',
    [string]$OutputPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($SourcePath)) {
    $SourcePath = Join-Path $scriptDirectory '..\docs\verification\ui-spike\test-cases.psv'
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $scriptDirectory '..\docs\verification\ui-spike\FacetWire-UI-Spike-Test-Matrix.xlsx'
}
function Convert-ToMatrix {
    param([object[][]]$Rows, [int]$ColumnCount)
    $matrix = New-Object 'object[,]' $Rows.Count, $ColumnCount
    for ($row = 0; $row -lt $Rows.Count; $row++) {
        for ($column = 0; $column -lt $ColumnCount; $column++) {
            $matrix[$row, $column] = $Rows[$row][$column]
        }
    }
    return ,$matrix
}

$source = [System.IO.Path]::GetFullPath($SourcePath)
$output = [System.IO.Path]::GetFullPath($OutputPath)
$records = @(Import-Csv -LiteralPath $source -Delimiter '|' -Encoding UTF8)
if ($records.Count -eq 0) { throw '测试用例源文件为空。' }
$headers = @($records[0].PSObject.Properties.Name)
if ($headers.Count -ne 17) { throw "预期 17 列，实际为 $($headers.Count) 列。" }
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($output)) | Out-Null

$headerRows = [object[][]]@([object[]]$headers)
$dataRows = [System.Collections.Generic.List[object[]]]::new()
foreach ($record in $records) {
    $row = [object[]]::new($headers.Count)
    for ($column = 0; $column -lt $headers.Count; $column++) {
        $row[$column] = [string]$record.($headers[$column])
    }
    $dataRows.Add($row)
}

$excel = $null
$workbook = $null
try {
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $workbook = $excel.Workbooks.Add()
    while ($workbook.Worksheets.Count -lt 4) { $null = $workbook.Worksheets.Add() }

    $caseSheet = $workbook.Worksheets.Item(1)
    $summarySheet = $workbook.Worksheets.Item(2)
    $environmentSheet = $workbook.Worksheets.Item(3)
    $guideSheet = $workbook.Worksheets.Item(4)
    $caseSheet.Name = '测试用例'
    $summarySheet.Name = '汇总'
    $environmentSheet.Name = '环境记录'
    $guideSheet.Name = '填写说明'

    for ($column = 0; $column -lt $headers.Count; $column++) {
        $caseSheet.Cells.Item(1, $column + 1).Value2 = $headers[$column]
    }
    $lastRow = $records.Count + 1
    for ($row = 0; $row -lt $records.Count; $row++) {
        for ($column = 0; $column -lt $headers.Count; $column++) {
            $caseSheet.Cells.Item($row + 2, $column + 1).Value2 =
                [string]$records[$row].($headers[$column])
        }
    }
    $table = $caseSheet.ListObjects.Add(1, $caseSheet.Range('A1', "Q$lastRow"), $null, 1)
    $table.Name = 'UiSpikeTestCases'
    $table.TableStyle = 'TableStyleMedium2'

    $statusRange = $caseSheet.Range('M2', 'M1000')
    $statusRange.Validation.Delete()
    $listSeparator = [string]$excel.International(5)
    $statusValues = @('未执行', '通过', '失败', '阻塞', '不适用') -join $listSeparator
    $statusRange.Validation.Add(3, 1, 1, $statusValues)
    $statusRange.Validation.IgnoreBlank = $true
    $statusRange.Validation.InCellDropdown = $true
    $statusRange.Validation.ErrorTitle = '状态无效'
    $statusRange.Validation.ErrorMessage = '请从下拉列表选择状态。'
    $statusRange.Validation.ShowError = $true

    $caseSheet.Cells.Font.Name = 'Arial'
    $caseSheet.Cells.Font.Size = 10
    $caseSheet.Rows.Item(1).Font.Bold = $true
    $caseSheet.Rows.Item(1).RowHeight = 30
    $caseSheet.Range('A1', "Q$lastRow").VerticalAlignment = -4160
    $caseSheet.Range('F2', "Q$lastRow").WrapText = $true
    $widths = @(12, 9, 15, 16, 12, 24, 36, 36, 9, 16, 12, 13, 11, 28, 30, 12, 30)
    for ($column = 1; $column -le $widths.Count; $column++) {
        $caseSheet.Columns.Item($column).ColumnWidth = $widths[$column - 1]
    }
    $caseSheet.Activate()
    $caseSheet.Range('A2').Select()
    $excel.ActiveWindow.FreezePanes = $true

    $summarySheet.Range('A1').Value2 = '状态'
    $summarySheet.Range('B1').Value2 = '数量'
    $summarySheet.Range('C1').Value2 = '占全部用例'
    $statuses = @('未执行', '通过', '失败', '阻塞', '不适用')
    for ($index = 0; $index -lt $statuses.Count; $index++) {
        $row = $index + 2
        $summarySheet.Cells.Item($row, 1).Value2 = $statuses[$index]
        $summarySheet.Cells.Item($row, 2).Formula = "=COUNTIF(测试用例!`$M:`$M,A$row)"
        $summarySheet.Cells.Item($row, 3).Formula = "=IFERROR(B$row/`$B`$8,0)"
    }
    $summarySheet.Range('A8').Value2 = '用例总数'
    $summarySheet.Range('B8').Formula = '=COUNTA(测试用例!A:A)-1'
    $summarySheet.Range('A9').Value2 = '通过率（排除不适用）'
    $summarySheet.Range('B9').Formula = '=IFERROR(B3/(B8-B6),0)'
    $summarySheet.Range('A11').Value2 = '结论规则'
    $summarySheet.Range('B11').Value2 = '任一 P0 失败则 Gate 失败；阻塞不视为通过。'
    $summarySheet.Range('A1', 'C1').Font.Bold = $true
    $summarySheet.Range('A1', 'C1').Interior.Color = 0xD9EAF7
    $summarySheet.Range('C2', 'C6').NumberFormat = '0.0%'
    $summarySheet.Range('B9').NumberFormat = '0.0%'
    $summarySheet.Columns.Item(1).ColumnWidth = 26
    $summarySheet.Columns.Item(2).ColumnWidth = 20
    $summarySheet.Columns.Item(3).ColumnWidth = 20
    $summarySheet.Cells.Font.Name = 'Arial'

    $environmentHeaders = @('执行批次', '平台', 'OS/版本', '架构', 'Flutter', 'Dart', '编译器/IDE', '渲染后端', '设备/分辨率', '辅助技术', '执行人', '日期', '备注')
    $environmentValues = @('WIN-NATIVE-20260821', 'Windows', '', 'x64', '未安装', '未安装', 'MSVC 19.41.34120 / CMake 3.29.5', 'Native only', '', '', '', '2026-08-21', 'Spike CTest 1/1 通过')
    for ($column = 0; $column -lt 13; $column++) {
        $environmentSheet.Cells.Item(1, $column + 1).Value2 = $environmentHeaders[$column]
        $environmentSheet.Cells.Item(2, $column + 1).Value2 = $environmentValues[$column]
    }
    $environmentSheet.Range('A1', 'M2').WrapText = $true
    $environmentSheet.Range('A1', 'M1').Font.Bold = $true
    $environmentSheet.Range('A1', 'M1').Interior.Color = 0xD9EAF7
    $environmentSheet.Columns.AutoFit() | Out-Null
    $environmentSheet.Cells.Font.Name = 'Arial'

    $guideRows = @(
        '项目|说明',
        '未执行|尚未开始；不是失败，也不是通过。',
        '通过|实际结果满足预期，且证据可复核。',
        '失败|实际结果不满足预期；必须填写实际结果和缺陷 ID。',
        '阻塞|因工具链、设备、权限或依赖不可执行；备注中写明解除条件。',
        '不适用|本批次确实不适用；备注中说明原因。',
        '证据要求|自动化填写日志/报告路径；手工测试填写截图、录屏或 issue 链接。',
        'Gate 规则|任一 P0 失败则 Gate 失败；阻塞项目不能计为通过。',
        '透明度定义|不透明度 100%=完全不透明；不透明度 0%=完全透明。透明度定义相反。',
        '关联设计|docs/adr/0001-cross-platform-ui-framework.md 与 docs/verification/ui-spike/summary.md'
    )
    for ($row = 0; $row -lt $guideRows.Count; $row++) {
        $parts = $guideRows[$row].Split('|', 2)
        $guideSheet.Cells.Item($row + 1, 1).Value2 = $parts[0]
        $guideSheet.Cells.Item($row + 1, 2).Value2 = $parts[1]
    }
    $guideSheet.Range('A1', 'B1').Font.Bold = $true
    $guideSheet.Range('A1', 'B1').Interior.Color = 0xD9EAF7
    $guideSheet.Columns.Item(1).ColumnWidth = 20
    $guideSheet.Columns.Item(2).ColumnWidth = 90
    $guideSheet.Range('A1', "B$($guideRows.Count)").WrapText = $true
    $guideSheet.Cells.Font.Name = 'Arial'

    $excel.CalculateFull()
    foreach ($cell in @('B2', 'B3', 'B4', 'B5', 'B6', 'B8', 'B9')) {
        $valueText = [string]$summarySheet.Range($cell).Text
        if ($valueText.StartsWith('#')) { throw "汇总公式错误：$cell = $valueText" }
    }
    $workbook.SaveAs($output, 51)
    $workbook.Close($true)
    $workbook = $null
}
finally {
    if ($null -ne $workbook) { $workbook.Close($false) }
    if ($null -ne $excel) { $excel.Quit() }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}

Write-Output "Generated $output with $($records.Count) test cases."
