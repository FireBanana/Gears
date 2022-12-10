@echo off
adb logcat -c
adb logcat | findstr native-activity
pause