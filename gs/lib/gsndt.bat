@echo off
@rem $Id$

call "%~dp0gssetgs.bat"
%GSC% -P- -dSAFER -DNODISPLAY %1 %2 %3 %4 %5 %6 %7 %8 %9 >t
