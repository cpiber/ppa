#
# Regular cron jobs for the rofi package
#
0 4	* * *	root	[ -x /usr/bin/rofi_maintenance ] && /usr/bin/rofi_maintenance
