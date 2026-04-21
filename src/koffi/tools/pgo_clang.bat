@echo off
setlocal

cd "%~dp0\.."

set PGO_DIR="%cd%\..\..\bin\Koffi\pgo\clang"

rmdir /S /Q "%PGO_DIR%" 2>NUL
mkdir "%PGO_DIR%"

node "../cnoke/cnoke.js" clean --release
node "../cnoke/cnoke.js" -d "PGO_GENERATE=%PGO_DIR%" --clang --release -v

node ../cnoke/cnoke.js -D benchmark --release -v
node ../cnoke/cnoke.js -D test --release -v

echo Running tests and benchmarks to generate profile data...
node benchmark/benchmark.js --koffi >NUL
for /l %%x in (1, 1, 100) do node test/sync.js >NUL
for /l %%x in (1, 1, 100) do node test/callbacks.js --no_poll >NUL

node ../cnoke/cnoke.js -D benchmark --release -v
echo Running benchmarks to generate profile data...
node benchmark/benchmark.js >/dev/null

llvm-profdata merge -output="%PGO_DIR%\koffi.profdata" "%PGO_DIR%\*.profraw"

node "../cnoke/cnoke.js" clean --release
node "../cnoke/cnoke.js" -d "PGO_PROFILE=%PGO_DIR%\koffi.profdata" --clang --release -v
