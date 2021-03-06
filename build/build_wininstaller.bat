@echo off

echo ============================
echo Setup Build Environment
echo ============================
cd ./../
set SRCDIR=%cd%
set QTDIR=C:\\Qt\\5.4\\msvc2012_opengl\\bin\\
set PATH=C:\Python27\;%QTDIR%;C:\msys\1.0\bin\;%PATH%
qmake -v
if not defined DevEnvDir (
call "C:\\Program Files\\Microsoft Visual Studio 11.0\\VC\\vcvarsall.bat"
)
echo %PATH%
echo DONE

echo ============================
echo Set version %1
echo ============================
python ./build/set_version.py %1 > ./src/version.h
python ./build/set_version.py %1 > ./3rdparty/crashreporter/src/version.h
echo DONE

echo ============================
echo Build project
echo ============================
echo Build Crash Reporter :
cd ./3rdparty/crashreporter
qmake CONFIG+=release DESTDIR=%SRCDIR%/bin/windows/release
nmake /NOLOGO /S

if %errorlevel% neq 0 (
 echo !!! Crashreporter Compilation Error !!!
 exit /b %errorlevel%
)

echo ============================
echo Build Application :
cd %SRCDIR%/src
qmake CONFIG+=release
nmake /NOLOGO /S

if %errorlevel% neq 0 (
 echo !!! Compilation Error !!!
 exit /b %errorlevel%
)

echo ============================
echo Export debug symbols
echo ============================
cd %SRCDIR%/bin/windows/release
%SRCDIR%/3rdparty/breakpad/src/tools/windows/binaries/dump_syms.exe rdm.pdb > rdm.sym

echo ============================
echo Build installer
echo ============================
cd %SRCDIR%
cp -f bin/windows/release/rdm.exe build/windows/installer/resources/
cd build/windows/installer/resources/
echo ===============
echo Windeploy tool:
windeployqt --no-translations --compiler-runtime --release-with-debug-info --qmldir %SRCDIR%/src/resources/qml rdm.exe
cd %SRCDIR%

echo ===============
echo Build installer:
"C:\\Program Files\\NSIS\\Unicode\\makensis.exe" /V1 /DVERSION=%1  ./build/windows/installer/installer.nsi

echo ============================
echo Copy installer
echo ============================
rm -f ./bin/*.exe
cp -f ./build/windows/installer/redis-desktop-manager-%1.exe ./bin/redis-desktop-manager-%1.exe
