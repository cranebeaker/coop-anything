#!/bin/bash
# Screenshot one instance's window. Usage: shot.sh <pid> <out.png>  (also writes <out>_small.png at 1/3)
H=$(powershell -NoProfile -Command "(Get-Process -Id $1).MainWindowHandle")
um win shot --hwnd $H "$2" --scale 0.33 >/dev/null 2>&1
