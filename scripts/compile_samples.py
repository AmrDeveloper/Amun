import os
import sys
from pathlib import Path

extension = ".exe" if os == "nt" else ""
executable = "./jot" + extension
samples_directory = "../samples"

argv = sys.argv[1:]
if len(argv) > 1:
    print("Expect only one argument which is folder name")
    exit(1)

if len(argv) == 1:
    samples_directory = samples_directory + "/" + argv[0]

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

# Compile each jot file
number_of_samples = 0
for file in collect_all_files(samples_directory):
    command = executable + " compile " + file
    os.system(command)
    number_of_samples += 1

print("Compiled", number_of_samples, "source code file")