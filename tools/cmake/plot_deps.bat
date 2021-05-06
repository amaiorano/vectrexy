if not exist CMakeCache.txt (
    echo Run this script from the CMake build folder
    pause
    exit /b
)

copy /y %~dp0\CMakeGraphVizOptions.cmake .
if not exist graphviz mkdir graphviz
cmake --graphviz=.\graphviz\deps.dot . && dot -Tpng .\graphviz\deps.dot > deps.png && deps.png
