/*
 * dprint.c 2010-08-11 14:30:00 liu fan
 *
 * debug print 
 *
 * Copyright (C) 2010-2012 liu fan
 *
 * This file is common for linux software project.
 *
 */

/*!
 * \file
 * \brief Common Debug console print functions
 */


#include "dprint.h"
 
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char* str_fac[]={"LOG_AUTH","LOG_CRON","LOG_DAEMON",
					"LOG_KERN","LOG_LOCAL0","LOG_LOCAL1",
					"LOG_LOCAL2","LOG_LOCAL3","LOG_LOCAL4","LOG_LOCAL5",
					"LOG_LOCAL6","LOG_LOCAL7","LOG_LPR","LOG_MAIL",
					"LOG_NEWS","LOG_USER","LOG_UUCP",
					"LOG_AUTHPRIV","LOG_FTP","LOG_SYSLOG",
					0};
static int int_fac[]={LOG_AUTH ,  LOG_CRON , LOG_DAEMON ,
					LOG_KERN , LOG_LOCAL0 , LOG_LOCAL1 ,
					LOG_LOCAL2 , LOG_LOCAL3 , LOG_LOCAL4 , LOG_LOCAL5 ,
					LOG_LOCAL6 , LOG_LOCAL7 , LOG_LPR , LOG_MAIL ,
					LOG_NEWS , LOG_USER , LOG_UUCP
					,LOG_AUTHPRIV,LOG_FTP,LOG_SYSLOG
					};

static int default_debug_level = 4;

int *debug_level = &default_debug_level;

int log_stderr = 0;
int log_facility = LOG_LOCAL2;
char* log_name = "core";

char ctime_buf[256];

/* log_stderr_val 
 *   0:log to syslog
 *   1:log to stderr 
 */
void set_log_stderr(int log_stderr_val)
{
    log_stderr = log_stderr_val;
}


void set_log_facility(char* log_facility_str)
{
    if(log_facility_str) {
        log_facility = str2facility(log_facility_str);
    }
}


int str2facility(char *s)
{
	int i;

    if(!s) { 
        return -1; 
    }

	for( i=0; str_fac[i] ; i++) {
		if (!strcmp(s,str_fac[i]))
			return int_fac[i];
	}
	return -1;
}


int dp_my_pid(void)
{
	return getpid();
}


void dprint(char * format, ...)
{
	va_list ap;

	//fprintf(stderr, "%2d(%d) ", process_no, my_pid());
	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}


static int *old_proc_level=NULL;

void set_debug_level(int level)
{
	static int proc_level;

	proc_level = level;
	if (old_proc_level==NULL) {
		old_proc_level = debug_level;
		debug_level = &proc_level;
	}
}


void reset_debug_level(void)
{
	if (old_proc_level) {
		debug_level = old_proc_level;
		old_proc_level = NULL;
	}
}
