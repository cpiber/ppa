#
# Regular cron jobs for the cgdb package
#
0 4	* * *	root	[ -x /usr/bin/cgdb_maintenance ] && /usr/bin/cgdb_maintenance
