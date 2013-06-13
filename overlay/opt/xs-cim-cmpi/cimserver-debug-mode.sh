#!/bin/bash

OPENPEG_TRACE="/opt/openpegasus/trace/cimserver.trc"
cimconfig=/opt/openpegasus/bin/cimconfig
WATCHDOG_OFF_FLAG="/opt/xs-cim-cmpi/watchdog-off"

clear_log_files() {
    #clear existing log files
    echo "" > $SBLIM_TRACE_FILE
    echo "" > $OPENPEG_TRACE
}

restart_openpegasus() {
    #Stop the watchdog
    touch $WATCHDOG_OFF_FLAG
    
    #Start the cimserver
    /etc/init.d/openpegasus restart
    
    #Re-start the watchdog
    rm $WATCHDOG_OFF_FLAG
}

start() {
    echo "Turning on debug mode for the cimserver"
    export SBLIM_TRACE=4
    export SBLIM_TRACE_FILE=/var/log/xencim.log

    OPENPEG_TRACE="/opt/openpegasus/trace/cimserver.trc"
    
    clear_log_files
    
    #Set the cimserver debug options
    $cimconfig -s traceComponents=all -p
    $cimconfig -s traceLevel=4 -p
    
    restart_openpegasus
}

stop() {
    echo "Turning off the debug mode for the cimserver"
    export SBLIM_TRACE=0
    export SBLIM_TRACE_FILE=""
    
    #Set the cimserver debug options
    $cimconfig -s traceComponents="" -p
    $cimconfig -s traceLevel=0 -p
        
    restart_openpegasus

}

case "$1" in
    start)
	start
	;;
    stop)
	stop
	;;
    *)
	echo "Usage: {start|stop}"
	exit 1
esac
