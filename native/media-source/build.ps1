# Builds C4DPortalMediaSource.dll directly with cl.exe (no MSBuild project —
# this DLL is loaded by the Windows Frame Server service via COM, not by
# Node, so it doesn't go through node-gyp like native/virtual-camera does).
$ErrorActionPreference = "Stop"

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
$sdkInclude = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"
$srcDir = Join-Path $PSScriptRoot "src"
$outDir = Join-Path $PSScriptRoot "build"
$wilInclude = Join-Path $PSScriptRoot "..\reference\wil-src\include"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$sourceFiles = @(
  "dllmain.cpp",
  "winrtCommon.cpp",
  "VirtualCameraMediaSourceActivate.cpp",
  "SimpleMediaSource.cpp",
  "SimpleMediaStream.cpp",
  "SimpleFrameGenerator.cpp"
) -join " "

$libs = "mfplat.lib mf.lib mfuuid.lib mfsensorgroup.lib ole32.lib propsys.lib onecore.lib"

$batchContent = @"
call "$vcvars" x64
cd /d "$srcDir"
cl.exe /LD /EHsc /std:c++20 /DUNICODE /D_UNICODE /nologo /I"$srcDir" /I"$wilInclude" /I"$sdkInclude\cppwinrt" $sourceFiles /Fo"$outDir\\" /Fe"$outDir\C4DPortalMediaSource.dll" /link /DEF:"$srcDir\C4DPortalMediaSource.def" $libs
"@

$batchPath = Join-Path $PSScriptRoot "_build.cmd"
Set-Content -Path $batchPath -Value $batchContent -Encoding ASCII

cmd /c "`"$batchPath`""
