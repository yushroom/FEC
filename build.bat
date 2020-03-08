cd engine/build/engine
cmake --build . --target texconv --config Release
cmake --build . --target hlslreflect --config Release
cmake --build . --target texconv --config Debug
cmake --build . --target hlslreflect --config Debug
cmake --build . --target glTFViewer --config Release
pause
cd ..\..\..\
copy .\engine\source\thirdparty\imgui\misc\fonts\DroidSans.ttf /N .\engine\binaries\Release
copy .\engine\source\thirdparty\imgui\misc\fonts\DroidSans.ttf /N .\engine\binaries\Debug
copy .\engine\source\bin\CMake\Release\texconv.exe /Y .\engine\binaries\Release
copy .\engine\source\bin\CMake\Debug\texconv.exe /Y .\engine\binaries\Debug
pause