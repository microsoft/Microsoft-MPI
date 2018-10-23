@rem
@rem Manage MSMPI tracing
@rem
@rem

@if "%_echo%"=="" echo off
setlocal 


echo mpitrace 1.0 - MSMPI tracing manager

set tr_bin_etl=%windir%\debug\mpitrace.etl
set tr_txt_etl=%windir%\debug\mpitrace.txt
set tr_session=msmpi
set tr_cfgfile=%windir%\debug\mpitrcfg.ini
set tr_realtime=
set tr_file_mode_options=-f bincirc -max 4
set tr_bin_etl_cmd=-o %tr_bin_etl%



@rem
@rem Jump to where we handle usage
@rem 
if /I "%1" == "help" goto Usage
if /I "%1" == "-help" goto Usage
if /I "%1" == "/help" goto Usage
if /I "%1" == "-h" goto Usage
if /I "%1" == "/h" goto Usage
if /I "%1" == "-?" goto Usage
if /I "%1" == "/?" goto Usage

@rem
@rem Set TraceFormat environment variable
@rem
if /I "%1" == "-path" shift&goto SetPath
if /I "%1" == "/path" shift&goto SetPath
goto EndSetPath
:SetPath
if /I not "%1" == "" goto DoSetPath
echo ERROR: Argument '-path' specified without argument for TraceFormat folder.
echo Usage example: mpitrace -path x:\symbols.pri\TraceFormat
goto :eof
:DoSetPath
echo Setting TRACE_FORMAT_SEARCH_PATH to '%1'
endlocal
set TRACE_FORMAT_SEARCH_PATH=%1&shift
goto :eof
:EndSetPath

@rem
@rem Format binary log file to text file
@rem
if /I "%1" == "-format" shift&goto FormatFile
if /I "%1" == "/format" shift&goto FormatFile
goto EndFormatFile
:FormatFile
if /I not "%TRACE_FORMAT_SEARCH_PATH%" == "" goto DoFormatFile
echo ERROR: Argument '-format' specified without running 'mpitrace -path' first.
echo Usage example: mpitrace -path x:\symbols.pri\TraceFormat
echo                mpitrace -format (print '%tr_bin_etl%')
goto :eof
:DoFormatFile
if /I not "%1" == "" set tr_bin_etl=%1&shift
call tracefmt -displayonly -nosummary %tr_bin_etl%
goto :eof
:EndFormatFile



@rem
@rem Handle the -query argument
@rem 
if /I "%1" == "-query" shift& goto HandleQueryCommand
if /I "%1" == "/query" shift& goto HandleQueryCommand
goto EndHandleQueryCommand
:HandleQueryCommand
echo Querying %tr_session% trace session...
logman query %tr_session% -ets
goto :eof
:EndHandleQueryCommand


@rem
@rem Consume the -rt switch
@rem
if /I "%1" == "-rt" shift&goto ConsumeRealTimeArgument
if /I "%1" == "/rt" shift&goto ConsumeRealTimeArgument
goto EndConsumeRealTimeArgument
:ConsumeRealTimeArgument
if /I not "%TRACE_FORMAT_SEARCH_PATH%" == "" goto DoConsumeRealTimeArgument
echo ERROR: Argument '-rt' specified without running 'mpitrace -path' first.
echo Usage example: mpitrace -path x:\symbols.pri\TraceFormat
echo                mpitrace -rt (start real-time tracing)
goto :eof
:DoConsumeRealTimeArgument
set tr_realtime=-rt -ft 1
:EndConsumeRealTimeArgument



@rem
@rem Handle the -stop argument
@rem 
if /I "%1" == "-stop" shift& goto HandleStopCommand
if /I "%1" == "/stop" shift& goto HandleStopCommand
goto EndHandleStopCommand
:HandleStopCommand
echo Stopping %tr_session% trace session...
logman stop %tr_session% -ets
goto :eof
:EndHandleStopCommand






:ProcessChangeLevel
shift 
set TraceLevel=
if /I "%1" == "none" set TraceLevel=()
if /I "%1" == "error" set TraceLevel=(error)
if /I "%1" == "warning" set TraceLevel=(error,warning)
if /I "%1" == "info" set TraceLevel=(error,warning,info)
if /I "%TraceLevel%"=="" goto usage

logman update %tr_session% -p %Module% %TraceLevel% -ets

goto :eof

:EndChangeTrace




@rem
@rem Consume the "-start" argument if it exists. Default is to start.
@rem 
echo Starting %tr_session% trace logging to '%tr_bin_etl%'...
if /I "%1" == "-start" shift& goto HandleStart 
if /I "%1" == "/start" shift& goto HandleStart 
goto EndStart

:HandleStart
set tr_command=start
set tr_bin_etl_cmd=-o %tr_bin_etl%

:EndStart


@rem
@rem Consume the "-update" argument if it exists. Default is to start.
@rem 
echo Updating %tr_session% trace logging to '%tr_bin_etl%'...
if /I "%1" == "-update" shift& set tr_command=update
if /I "%1" == "/update" shift& set tr_command=update





@rem
@rem Process the noise level argument if it exists. Default is error level.
@rem 

if /I "%1" == "-info" shift&goto ConsumeInfoArgument
if /I "%1" == "/info" shift&goto ConsumeInfoArgument
goto EndConsumeInfoArgument
:ConsumeInfoArgument
echo %tr_session% trace noise level is INFORMATION...
Call :FSETUPTRCINIFILE "(error,warning,info)"
goto EndConsumeNoiseLevelArgument
:EndConsumeInfoArgument

if /I "%1" == "-warning" shift&goto ConsumeWarningArgument
if /I "%1" == "/warning" shift&goto ConsumeWarningArgument
goto EndConsumeWarningArgument
:ConsumeWarningArgument
echo %tr_session% trace noise level is WARNING...
Call :FSETUPTRCINIFILE "(error,warning)"
goto EndConsumeNoiseLevelArgument
:EndConsumeWarningArgument

echo %tr_session% trace noise level is ERROR...
Call :FSETUPTRCINIFILE "(error)"
if /I "%1" == "-error" shift
if /I "%1" == "/error" shift
:EndConsumeNoiseLevelArgument




@rem
@rem At this point if we have any argument it's an error
@rem 
if /I not "%1" == "" goto Usage

@rem
@rem Query if %tr_session% logger is running. If so only update the flags and append to logfile.
@rem 
echo Querying if %tr_session% logger is currently running...
logman query %tr_session% -ets 
if ERRORLEVEL 1 goto settrace
echo %tr_session% logger is currently running, changing existing trace settings...
set tr_command=update
set tr_bin_etl_cmd=
set tr_file_mode_options=


:settrace
if /I "%tr_realtime%"=="" goto settracecontinue
set tr_bin_etl_cmd=
set tr_file_mode_options=
:settracecontinue
logman %tr_command% %tr_session% %tr_realtime% -pf %tr_cfgfile%  %tr_bin_etl_cmd% %tr_file_mode_options% -ets 



@rem
@rem In real time mode, start formatting
@rem 
if /I "%tr_realtime%" == "" goto EndRealTimeFormat
echo Starting %tr_session% real time formatting...
if defined tr_bin_etl goto NormalRealTimeStart
logman update %tr_session% -rt -ets
:NormalRealTimeStart
call tracefmt -display -rt %tr_session% -o %tr_txt_etl%
:EndRealTimeFormat
set tr_realtime=
goto :eof


:FSETUPTRCINIFILE
set TEST=%1
echo "MSMQ: AC" %TEST:~1,-1% > %tr_cfgfile%
echo "MSMQ: Networking" %TEST:~1,-1%  >> %tr_cfgfile%
echo "MSMQ: SRMP" %TEST:~1,-1%  >> %tr_cfgfile%
echo "MSMQ: XACT_General" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: RPC" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: DS" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: Security" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: Routing" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: General" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: XACT_Send" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: XACT_Receive" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: XACT_Log" %TEST:~1,-1% >> %tr_cfgfile%
echo "MSMQ: Log" %TEST:~1,-1% >> %tr_cfgfile%
goto :eof


:Usage
echo.
echo Usage:
echo   mpitrace [-rt] -start [complist]
echo   mpitrace [-rt] -stop [complist]
echo   mpitrace -path trace_format_folder
echo   mpitrace -format [etl]
echo   mpitrace -query
echo.
echo Options:
echo    -start  start/update tracing for the list of components in [complist].
echo    -stop   stop tracing for the list of compoenets in [complist].
echo            [complist] is a space delimited list of components (case insensitive)
echo            apply to all componenets if [complist] is not specified.
echo.
echo PT2PT IPROB COLL RMA COMM ERRH GROUP ATTR DTYPE IO TOPO SPAWN INIT INFO MISC
echo.
echo    -path   set the environment variable TRACE_FORMAT_SEARCH_PATH to the trace
echo            format folder. 
echo            this environment variable is used by the -rt and -format options
echo            and needs to be set once (per command-line box).
echo.
echo    -rt     start tracing and display to the console window in real-time mode.
echo            a the binary etl file is saved to '%tr_bin_etl%'.
echo.
echo    -format translate a binary [etl] file into text format. if [etl] is not
echo            specified the default etl file is translated.
echo            '%tr_bin_etl%'
echo.
echo Examples,
echo mpitrace (start tracing all componenets into '%tr_bin_etl%')
echo mpitrace -path x:\Symbols.pri\TraceFormat
echo mpitrace -rt (start real time tracing)
echo mpitrace -format (print '%tr_bin_etl%')
echo mpitrace -stop (stop logging for all components)
echo mpitrace -stop (stop logging for all components)
echo mpitrace -query (query MSMPI trace settings)
echo.

