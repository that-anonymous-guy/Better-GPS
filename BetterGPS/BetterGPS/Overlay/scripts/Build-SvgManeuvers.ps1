# Build-time only: Windows WPF rasterizes the supplied rounded SVG paths.
# Launch PowerShell with -STA. The plugin requires neither WPF nor an SVG parser.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase
$phone=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output=Join-Path $phone 'generated';New-Item -ItemType Directory -Path $output -Force | Out-Null
foreach($name in @('ramp_left','ramp_right')){
 $source=Join-Path $phone "BetterGPS_assets/material/ui/rounded_material_symbols/navigation/$name.svg"
 [xml]$svg=Get-Content -LiteralPath $source -Raw
 $viewBox=$svg.DocumentElement.GetAttribute('viewBox').Split(' ',[StringSplitOptions]::RemoveEmptyEntries)
 $visual=[Windows.Media.DrawingVisual]::new();$drawing=$visual.RenderOpen()
 $transform=[Windows.Media.TransformGroup]::new()
 $transform.Children.Add([Windows.Media.TranslateTransform]::new(-[double]$viewBox[0],-[double]$viewBox[1]))
 $transform.Children.Add([Windows.Media.ScaleTransform]::new(384/[double]$viewBox[2],384/[double]$viewBox[3]))
 $drawing.PushTransform($transform)
 foreach($path in $svg.SelectNodes('//*[local-name()="path"]')){
  $drawing.DrawGeometry([Windows.Media.Brushes]::White,$null,[Windows.Media.Geometry]::Parse($path.GetAttribute('d')))
 }
 $drawing.Pop();$drawing.Close()
 $bitmap=[Windows.Media.Imaging.RenderTargetBitmap]::new(384,384,96,96,[Windows.Media.PixelFormats]::Pbgra32);$bitmap.Render($visual)
 $encoder=[Windows.Media.Imaging.PngBitmapEncoder]::new();$encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
 $stream=[IO.File]::Create((Join-Path $output "$name.png"));try{$encoder.Save($stream)}finally{$stream.Dispose()}
}
