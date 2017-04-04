setLocal EnableDelayedExpansion

set BUILD_DIR=build

mkdir installer
copy %BUILD_DIR%\%PRODUCT_NAME%.exe installer\
mkdir installer\plugins
xcopy "%BUILD_DIR%\plugins\*.dll" installer\plugins
copy %BUILD_DIR%\CrashReporter.exe installer\

set QML_DIRS=
for /d /r %%i in (*) do @if exist %%i\*.qml (set QML_DIRS=--qmldir %%i !QML_DIRS!)
echo %QML_DIRS%

windeployqt %QML_DIRS% --no-angle --no-compiler-runtime ^
	--no-opengl-sw --dir installer installer\%PRODUCT_NAME%.exe || EXIT /B 1

for %%i in (installer\plugins\*.dll) do windeployqt --no-angle --no-compiler-runtime ^
	--no-opengl-sw --dir installer %%i

set QML_DIR=source\crashreporter
IF NOT EXIST %QML_DIR%\NUL EXIT /B 1
windeployqt --qmldir %QML_DIR% --no-angle --no-compiler-runtime ^
	--no-opengl-sw installer\CrashReporter.exe || EXIT /B 1

xcopy "%CRTDIRECTORY%*.*" installer || EXIT /B 1
xcopy "%UniversalCRTSdkDir%redist\ucrt\DLLs\x64\*.*" installer || EXIT /B 1

signtool sign /f %SIGN_KEYSTORE_WINDOWS% /p %SIGN_PASSWORD% ^
	/tr %SIGN_TSA% /td SHA256 installer\%PRODUCT_NAME%.exe || EXIT /B 1

set /a value=0
set /a BUILD_SIZE=0
FOR /R %1 %%I IN (installer\*) DO (
set /a value=%%~zI/1024
set /a BUILD_SIZE=!BUILD_SIZE!+!value!
)

makensis /NOCD /DPRODUCT_NAME=%PRODUCT_NAME% /DVERSION=%VERSION% ^
	/DPUBLISHER="%PUBLISHER%" /DCOPYRIGHT="%COPYRIGHT%" /DBUILD_SIZE=%BUILD_SIZE% ^
	"/XOutFile installer\%PRODUCT_NAME%-%VERSION%-installer.exe" ^
	installers\windows\installer.nsi

signtool sign /f %SIGN_KEYSTORE_WINDOWS% /p %SIGN_PASSWORD% ^
	/tr %SIGN_TSA% /td SHA256 ^
	installer\%PRODUCT_NAME%-%VERSION%-installer.exe || EXIT /B 1

copy installer\%PRODUCT_NAME%-%VERSION%-installer.exe %BUILD_DIR%\

rmdir /s /q installer
