#
# Regular cron jobs for the iverilog package
#
0 4	* * *	root	[ -x /usr/bin/iverilog_maintenance ] && /usr/bin/iverilog_maintenance
