@echo off
@rem $Id$

rem Convert PostScript to PDF 1.2 (Acrobat 3-and-later compatible).

rem Make sure an empty argument list stays empty.
set PS2PDFSW=
if "%1"=="" goto bot

rem This clunky loop is the only way to ensure we pass the full
rem argument list!
set PS2PDFSW=-dCompatibilityLevel#1.2
goto bot
:top
set PS2PDFSW=%PS2PDFSW% %1
shift
:bot
if not "%1"=="" goto top
ps2pdfwr %PS2PDFSW%
