set addr=%~dp0
"C:\Program Files (x86)\Microsoft\ILMerge"\ilmerge.exe /ndebug /target:winexe /out:%addr%\Concentrator_Tools.exe /log %addr%\RQ_Concentrator_Tools.exe /log %addr%\UsbLibrary.dll
pause