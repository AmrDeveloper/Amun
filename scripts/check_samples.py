import os
from pathlib import Path

extension = ".exe" if os == "nt" else ""
executable = "./jot" + extension
samples_directory = "../samples"

# Setup directory to be inside the executable directory
current_directly = os.getcwd()
if not current_directly.endswith('bin'):
    current_directly += "/bin"
    os.chdir(current_directly)

# Collect all jot source files
def collect_all_files(path):
    root = Path(path)
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        file_path = str(p)
        if file_path.endswith(".jot"):
            yield file_path

# Check each jot file
for file in collect_all_files(samples_directory):
    command = executable + " check " + file
    os.system(command)
