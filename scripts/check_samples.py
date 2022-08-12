import os

extension = ".exe" if os == "nt" else ""
executable = "./build/jot" + extension

files = os.listdir("samples")
for file in files:
    command = executable + " check samples/" + file
    os.system(command)
