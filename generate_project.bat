if not exist engine\build\engine mkdir engine\build\engine
cd engine\buildeengine
cmake -G"Visual Studio 16 2019" -TClangCL ../../Source

pause

rem start FishEngine.sln