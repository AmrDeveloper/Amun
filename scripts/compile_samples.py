import os
from pathlib import Path

extension = ".exe" if os == "nt" else ""
executable = "./build/jot" + extension

def collect_all_files(path):
    root = Path(path)
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        yield p

for file in collect_all_files("samples"):
    command = executable + " compile " + str(file)
    os.system(command)