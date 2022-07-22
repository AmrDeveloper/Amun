#!/usr/bin/env bash

FULL_PATH_TO_SCRIPT="$(realpath "../$0")"
SCRIPT_DIRECTORY="$(dirname "$FULL_PATH_TO_SCRIPT")"

RETURN_CODE_SUCCESS=0

FILE_LIST="$(find "$SCRIPT_DIRECTORY" | grep -E ".*\.(cpp|c|h|hpp)$")"
IFS=$'\n' read -r -d '' -a FILE_LIST_ARRAY <<< "$FILE_LIST"

num_files="${#FILE_LIST_ARRAY[@]}"
echo -e "$num_files files found to format:"
if [ "$num_files" -eq 0 ]; then
    echo "Nothing to do."
    exit $RETURN_CODE_SUCCESS
fi

clang-format --verbose -i --style=file "${FILE_LIST_ARRAY[@]}"