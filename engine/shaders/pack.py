import glob
import os

fxc = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\fxc.exe"
dxc = r"E:\workspace\cengine\engine\binaries\dxc.exe"
hlslreflect = r"..\binaries\Release\hlslreflect.exe"

def reflect(input, output):
    input = os.path.abspath(input)
    cmd = f'""{hlslreflect}" {input} > {output}"'
    print(cmd)
    return os.system(cmd)

def basename(path:str):
    return os.path.splitext(os.path.basename(path))[0]

def CompileAndReflect(path:str, shaderType:str):
    print(f, shaderType)
    assert(shaderType in ['vs', 'ps', 'cs'])
    fn = basename(f)

    st = shaderType.lower()
    ST = shaderType.upper()
    output = f"runtime\\{fn}_{st}.cso"
    cmd = f'""{fxc}" {f} /nologo /I .\\include /E {ST} /T {st}_5_1 -Fo {output}"'
    ret = os.system(cmd)
    if ret != 0:
        return

    input = output
    output = f"runtime\\{fn}_{st}.reflect.json"
    ret = reflect(input, output)
    return ret

if __name__ == "__main__":
    # for f in glob.glob("*.hlsl"):
    #     CompileAndReflect(f, 'vs')
    #     CompileAndReflect(f, 'ps')

    for f in glob.glob("*.comp"):
        CompileAndReflect(f, 'cs')
