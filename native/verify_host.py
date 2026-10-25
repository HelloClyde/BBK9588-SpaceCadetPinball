"""Run the retained original engine with standard host C++ and the 9588 UI."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import json

HERE=Path(__file__).resolve().parent
PROJECT=HERE.parent

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx',type=Path,default=Path(os.environ.get('CXX') or shutil.which('g++') or 'g++'))
    parser.add_argument('--data',type=Path,default=PROJECT/'original-data')
    args=parser.parse_args()
    (PROJECT/'qa').mkdir(exist_ok=True)
    stage=PROJECT/'qa/runtime/native-host'
    engine=stage/'engine';engine.mkdir(parents=True,exist_ok=True)
    data=stage/'data';data.mkdir(exist_ok=True)
    for source in args.data.iterdir():
        if source.suffix.upper() in ('.DAT','.WAV','.MID'):
            shutil.copy2(source,data/source.name)
    for name in ('SCORE0.SAV','SCORE1.SAV'):
        slot=data/name
        if slot.exists():slot.unlink()
    for source in (HERE/'engine').iterdir():
        if source.suffix in ('.h','.cpp'):shutil.copy2(source,engine/source.name)
    shutil.copy2(HERE/'graphics_host_shim.h',engine/'pch.h')
    for name in ('native_types.h','platform.cpp','platform.h','ui.cpp','ui.h','sound_port.cpp','gdrv_port.cpp','test_engine_host.cpp'):
        shutil.copy2(HERE/name,stage/name)
    excluded={'SpaceCadetPinball','gdrv','winmain','options','fullscrn','Sound','midi','DebugOverlay',
              'font_selection','high_score','EmbeddedData'}
    sources=sorted(p for p in engine.glob('*.cpp') if p.stem not in excluded and not p.stem.startswith(('imgui','imstb')))
    sources += [stage/name for name in ('platform.cpp','ui.cpp','sound_port.cpp','gdrv_port.cpp','test_engine_host.cpp')]
    out=PROJECT/'qa/test_original_engine.exe'
    cmd=[str(args.cxx),'-std=c++11','-O2','-Wall','-Wextra','-Wno-unused-parameter','-Wno-misleading-indentation',
         '-ffunction-sections','-fdata-sections','-I',str(engine),'-I',str(stage),
         *map(str,sources),'-Wl,--gc-sections','-o',str(out)]
    env=os.environ.copy();env['PATH']=str(args.cxx.parent)+os.pathsep+env.get('PATH','')
    subprocess.run(cmd,check=True,env=env)
    result=subprocess.run([str(out),str(data.resolve()),str(PROJECT/'qa')],check=True,env=env,
                          text=True,encoding='utf-8',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (PROJECT/'qa/original-host-results.txt').write_text(result.stdout,encoding='utf-8')
    for line in result.stdout.splitlines():
        if line.startswith('HOST_RESULT '):
            report=json.loads(line[len('HOST_RESULT '):])
            (PROJECT/'qa/original-host-results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(result.stdout)

if __name__=='__main__':main()
