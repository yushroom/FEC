import glob
import os

fxc = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\fxc.exe"
dxc = r"E:\workspace\cengine\engine\binaries\dxc.exe"
hlslreflect = r"..\binaries\Release\hlslreflect.exe"

def reflect(input, output):
    input = os.path.abspath(input)
    cmd = f'""{hlslreflect}" {input} > {output}"'
    print(cmd)
    os.system(cmd)

for f in glob.glob("*.hlsl"):
    basename = os.path.splitext(os.path.basename(f))[0]
    print(f)
    output = f"runtime\\{basename}_vs.cso"
    cmd = f'""{fxc}" {f} /nologo /I .\\include /E VS /T vs_5_1 -Fo {output}"'
    os.system(cmd)

    input = output
    output = f"runtime\\{basename}_vs.reflect.json"
    reflect(input, output)

    output = f"runtime\\{basename}_ps.cso"
    cmd = f'""{fxc}" {f} /nologo /I .\\include /E PS /T ps_5_1 -Fo {output}"'
    os.system(cmd)

    input = output
    output = f"runtime\\{basename}_ps.reflect.json"
    reflect(input, output)
