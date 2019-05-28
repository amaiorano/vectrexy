@echo off
if not exist CMakeCache.txt (
    echo Run this script from the CMake build folder
    exit /b
)

if not exist graphviz mkdir graphviz
cmake --graphviz=.\graphviz\deps.dot . && dot -Tpng -Gdpi=100 .\graphviz\deps.dot > deps.png && deps.png
