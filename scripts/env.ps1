# Dot-source this to put the project toolchain on PATH for the session:
#   . .\scripts\env.ps1
$tools = "$env:USERPROFILE\tools"
$env:PATH = "$tools\cmake\bin;$tools\ninja;$tools\mingw64\bin;" + $env:PATH
# Emscripten (adds emcc/emcmake and its own node/python):
& "$env:USERPROFILE\emsdk\emsdk_env.ps1" | Out-Null
