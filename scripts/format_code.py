import os

file_paths = []

for root, directories, files in os.walk("."):
    for filename in files:
        filepath = os.path.join(root, filename)
        if filepath.endswith(('.h', '.hpp', '.c', '.cpp'))
            file_paths.append(filepath)

for file in file_paths:
    os.system("clang-format --verbose -i --style=file " + file)