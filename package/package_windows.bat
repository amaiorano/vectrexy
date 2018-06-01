rem Script creates a "vectrexy" folder root directory, and populates it. Doesn't create a zip file
rem because AppVeyor does it for us (folder defined as artifact)

setlocal
set package_name=%1
set package_base_url=%2

set root_dir=%cd%
set output_dir=%root_dir%\%package_name%
set data_zip_url=%package_base_url%data.zip

mkdir %output_dir%
copy /y %root_dir%\build\RelWithDebInfo\vectrexy.exe %output_dir%
copy /y %root_dir%\bios_rom.bin %output_dir%
copy /y %root_dir%\README.md %output_dir%
copy /y %root_dir%\COMPAT.md %output_dir%
copy /y %root_dir%\LICENSE.txt %output_dir%

git describe > %output_dir%\version.txt

powershell -Command (new-object System.Net.WebClient).DownloadFile('%data_zip_url%', 'data.zip')
powershell -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('data.zip', '%output_dir%'); }"
del /f /q data.zip
