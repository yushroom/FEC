if not exist engine\build\engine mkdir engine\build\engine
cd engine\build\engine
cmake -G"Visual Studio 16 2019" -TClangCL ../../Source

pause

start FishEngine.sln