@ECHO OFF

SET BOOST_VERSION=1_40
SET BOOST_DIR=c:\Boost_%BOOST_VERSION%

ECHO %LIB% | FIND "%BOOST_DIR%" > NUL && GOTO Quit

SET BOOSTLIB=%BOOST_DIR%\lib%1%
SET LIB=%BOOSTLIB%;%LIB%
SET INCLUDE=%BOOST_DIR%;%INCLUDE%
SET BOOSTLIB=
:Quit
