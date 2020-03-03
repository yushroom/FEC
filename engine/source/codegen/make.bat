python parser.py
python codegen.py
cd ..\fishengine
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\Llvm\bin\clang-format.exe" --style=file ..\codegen\jsbinding.gen.cpp > jsbinding.gen.cpp
pause