#!/bin/bash
find . \( -iname "*.h" -or -iname "*.c" \) -and -not -path "*lvgl*" -and -not -iname "ui_*.c" -exec clang-format -i {} \;


