@echo off
REM the following should already have been set in the environment variable before getting here
REM set server="http://<ip>:8889"
REM set user="<user>"
REM set pass="<pass>"
REM IF NOT ("%2")==("") (winrm-1 enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/%1 -filter:%2 -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% > class.txt) ELSE (winrm-1 enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/root/cimv2/%1 -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% > class.txt)
IF (%2)==() GOTO ENUM_NO_FILTER
 call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/%1 -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -filter:%filter% -ReturnType:EPR
 GOTO ENUM_END
:ENUM_NO_FILTER
 call winrm enum http://schemas.citrix.com/wbem/wscim/1/cim-schema/2/%1 -r:%server% -encoding:utf-8 -a:basic -u:%user% -p:%pass% -ReturnType:EPR
:ENUM_END


