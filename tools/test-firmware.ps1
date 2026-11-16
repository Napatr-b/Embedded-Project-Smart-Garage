param([string]$Compiler='zig',[string]$IdfPath='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$previousTaskTimezone=$env:TZ
Push-Location $taskRoot
try {
  New-Item -ItemType Directory -Force tmp | Out-Null
  $source='gateway'
  $isZig=[IO.Path]::GetFileNameWithoutExtension($Compiler) -eq 'zig'
  $env:ZIG_GLOBAL_CACHE_DIR=Join-Path $taskRoot 'tmp/zig-cache'
  $env:ZIG_LOCAL_CACHE_DIR=Join-Path $taskRoot 'tmp/zig-local-cache'
  $env:TZ='UTC0'
  $cppArgs=@('-std=c++17','-Wall','-Wextra',"-I$source/include","$source/test/host/logic_test.cpp","$source/src/door_controller.cpp",'-o','tmp/gateway-logic-tests.exe')
  if($isZig){& $Compiler c++ @cppArgs}else{& $Compiler @cppArgs}
  if($LASTEXITCODE -ne 0){throw 'Host C++ compile failed'}
  & ./tmp/gateway-logic-tests.exe
  if($LASTEXITCODE -ne 0){throw 'Host logic test failed'}
  if(!$IdfPath){$IdfPath=Join-Path $env:USERPROFILE '.platformio/packages/framework-espidf'}
  $cjson=Join-Path $IdfPath 'components/json/cJSON'
  if(!(Test-Path -LiteralPath (Join-Path $cjson 'cJSON.c'))){throw 'Logic passed, but parser test needs ESP-IDF cJSON. Build PlatformIO once or pass -IdfPath.'}
  if($isZig){& $Compiler cc -c (Join-Path $cjson 'cJSON.c') -o tmp/cjson.o}else{& $Compiler -x c -c (Join-Path $cjson 'cJSON.c') -o tmp/cjson.o}
  if($LASTEXITCODE -ne 0){throw 'cJSON compile failed'}
  $parserArgs=@('-std=c++17',"-I$source/include","-I$cjson","$source/test/host/protocol_test.cpp","$source/src/protocol.cpp",'tmp/cjson.o','-o','tmp/gateway-protocol-tests.exe')
  if($isZig){& $Compiler c++ @parserArgs}else{& $Compiler @parserArgs}
  if($LASTEXITCODE -ne 0){throw 'Parser compile failed'}
  & ./tmp/gateway-protocol-tests.exe
  if($LASTEXITCODE -ne 0){throw 'Parser checks failed'}
} finally {$env:TZ=$previousTaskTimezone;Pop-Location}
