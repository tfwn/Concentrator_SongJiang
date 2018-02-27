set addr=%~dp0
set name=6009集中器GPRS参数设置工具V1.0.exe
"C:\Program Files (x86)\Microsoft\ILMerge"\ilmerge.exe /ndebug /target:winexe /out:%addr%\%name% /log %addr%\SR6009_Concentrator_Tools.exe /log %addr%\UsbLibrary.dll
pause