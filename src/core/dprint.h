/*
 * dprint.h 2010-08-11 14:30:00 liu fan
 *
 * Copyright (C) 2010-2012 liu fan
 *
 * This file is common for linux software project.
 *
 */

/*!
 * \file
 * \brief Common Debug console print functions
 * \see syslog.h
 */


/*! \page DebugLogFunction Description of the logging functions:
 *
 *  A) macros to log on a predefine log level and with standard prefix
 *     for with additional info: [time] 
 *     No dynamic FMT is accepted (due macro processing).
 *       LM_ALERT( fmt, ....)
 *       LM_CRIT( fmt, ....)
 *       LM_ERR( fmt, ...)
 *       LM_WARN( fmt, ...)
 *       LM_NOTICE( fmt, ...)
 *       LM_INFO( fmt, ...)
 *       LM_DBG( fmt, ...)
 *  B) macros for generic logging ; no additional information is added;
 *     Works with dynamic FMT.
 *       LM_GEN1( log_level, fmt, ....)
 *       LM_GEN2( log_facility, log_level, fmt, ...)
 */



#ifndef dprint_h
#define dprint_h

#include <syslog.h>
#include <time.h>
#include <stdio.h> 

#define L_ALERT (-3)	/*!< Alert level */
#define L_CRIT  (-2)	/*!< Critical level */
#define L_ERR   (-1)	/*!< Error level */
#define L_WARN   (1)	/*!< Warning level */
#define L_NOTICE (2)	/*!< Notice level */
#define L_INFO   (3)	/*!< Info level */
#define L_DBG    (4)	/*!< Debug level */


#define DP_PREFIX  "%s [%d] "


#define DP_ALERT_TEXT    "ALERT:"
#define DP_CRIT_TEXT     "CRITICAL:"
#define DP_ERR_TEXT      "ERROR:"
#define DP_WARN_TEXT     "WARNING:"
#define DP_NOTICE_TEXT   "NOTICE:"
#define DP_INFO_TEXT     "INFO:"
#define DP_DBG_TEXT      "DBG:"

#define DP_ALERT_PREFIX  DP_PREFIX DP_ALERT_TEXT
#define DP_CRIT_PREFIX   DP_PREFIX DP_CRIT_TEXT
#define DP_ERR_PREFIX    DP_PREFIX DP_ERR_TEXT
#define DP_WARN_PREFIX   DP_PREFIX DP_WARN_TEXT
#define DP_NOTICE_PREFIX DP_PREFIX DP_NOTICE_TEXT
#define DP_INFO_PREFIX   DP_PREFIX DP_INFO_TEXT
#define DP_DBG_PREFIX    DP_PREFIX DP_DBG_TEXT

#define DPRINT_LEV   L_ERR

#ifndef MOD_NAME
	#define MOD_NAME "core"
#endif

#ifndef NO_DEBUG
	#undef NO_LOG
#endif

/* vars:*/

extern int *debug_level;

extern int log_stderr;
extern int log_facility;
extern char* log_name;
extern char ctime_buf[];


int dp_my_pid(void);

void dprint (char* format, ...);

int str2facility(char *s);

void set_debug_level(int level);

void reset_debug_level(void);

void set_log_stderr(int log_stderr_val);

void set_log_facility(char* log_facility_str);


inline static char* dp_time(void)
{

	time_t ltime;
	time(&ltime);
	ctime_r( &ltime, ctime_buf);
	ctime_buf[19] = 0; // remove year

	return ctime_buf+4;  // remove name of day
	
/*	struct tm * lt; 
    	time_t tick; 
	tick=time(NULL); 
    	lt=localtime(&tick); 
    	sprintf(ctime_buf, 
			"%04d-%02d-%02d %02d:%02d:%02d ",  
			lt->tm_year+1900,
			lt->tm_mon+1,
			lt->tm_mday,
			lt->tm_hour,
			lt->tm_min,
			lt->tm_sec); 
	
	return ctime_buf;
*/
}


#define is_printable(_level)  ((*debug_level)>=(_level))

#if defined __GNUC__
	#define __DP_FUNC  __FUNCTION__
#elif defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
	#define __DP_FUNC  __func__
#else
	#define __DP_FUNC  ((__const char *) 0)
#endif


#ifdef NO_LOG

	#define LM_GEN2(facility, lev, fmt, args...)
	#define LM_GEN1(lev, fmt, args...)
	#define LM_ALERT(fmt, args...)
	#define LM_CRIT(fmt, args...)
	#define LM_ERR(fmt, args...)
	#define LM_WARN(fmt, args...)
	#define LM_NOTICE(fmt, args...)
	#define LM_INFO(fmt, args...)
	#define LM_DBG(fmt, args...)

#else /* NO_LOG */

	#define LOG_PREFIX  MOD_NAME ":%s: "

	#define MY_DPRINT( _prefix, _fmt, args...) \
			dprint( _prefix LOG_PREFIX _fmt, dp_time(), \
				dp_my_pid(), __DP_FUNC, ## args) \

	#define MY_SYSLOG( _log_level, _prefix, _fmt, args...) \
			syslog( (_log_level)|log_facility, \
						_prefix LOG_PREFIX _fmt, __DP_FUNC, ##args);\

	#define LM_GEN1(_lev, args...) \
		LM_GEN2( log_facility, _lev, ##args)

	#define LM_GEN2( _facility, _lev, fmt, args...) \
		do { \
			if (is_printable(_lev)){ \
				if (log_stderr) dprint ( fmt, ## args); \
				else { \
					switch(_lev){ \
						case L_CRIT: \
							syslog(LOG_CRIT|_facility, fmt, ##args); \
							break; \
						case L_ALERT: \
							syslog(LOG_ALERT|_facility, fmt, ##args); \
							break; \
						case L_ERR: \
							syslog(LOG_ERR|_facility, fmt, ##args); \
							break; \
						case L_WARN: \
							syslog(LOG_WARNING|_facility, fmt, ##args);\
							break; \
						case L_NOTICE: \
							syslog(LOG_NOTICE|_facility, fmt, ##args); \
							break; \
						case L_INFO: \
							syslog(LOG_INFO|_facility, fmt, ##args); \
							break; \
						case L_DBG: \
							syslog(LOG_DEBUG|_facility, fmt, ##args); \
							break; \
					} \
				} \
			} \
		}while(0)

	#define LM_ALERT( fmt, args...) \
		do { \
			if (is_printable(L_ALERT)){ \
				if (log_stderr)\
					MY_DPRINT( DP_ALERT_PREFIX, fmt, ##args);\
				else \
					MY_SYSLOG( LOG_ALERT, DP_ALERT_TEXT, fmt, ##args);\
			} \
		}while(0)

	#define LM_CRIT( fmt, args...) \
		do { \
			if (is_printable(L_CRIT)){ \
				if (log_stderr)\
					MY_DPRINT( DP_CRIT_PREFIX, fmt, ##args);\
				else \
					MY_SYSLOG( LOG_CRIT, DP_CRIT_TEXT, fmt, ##args);\
			} \
		}while(0)

	#define LM_ERR( fmt, args...) \
		do { \
			if (is_printable(L_ERR)){ \
				if (log_stderr)\
					MY_DPRINT( DP_ERR_PREFIX, fmt, ##args);\
				else \
					MY_SYSLOG( LOG_ERR, DP_ERR_TEXT, fmt, ##args);\
			} \
		}while(0)

	#define LM_WARN( fmt, args...) \
		do { \
			if (is_printable(L_WARN)){ \
				if (log_stderr)\
					MY_DPRINT( DP_WARN_PREFIX, fmt, ##args);\
				else \
					MY_SYSLOG( LOG_WARNING, DP_WARN_TEXT, fmt, ##args);\
			} \
		}while(0)

	#define LM_NOTICE( fmt, args...) \
		do { \
			if (is_printable(L_NOTICE)){ \
				if (log_stderr)\
					MY_DPRINT( DP_NOTICE_PREFIX, fmt, ##args);\
				else \
					MY_SYSLOG( LOG_NOTICE, DP_NOTICE_TEXT, fmt, ##args);\
			} \
		}while(0)

	#define LM_INFO( fmt, args...) \
		do { \
			if (is_printable(L_INFO)){ \
				if (log_stderr)\
					MY_DPRINT( DP_INFO_PREFIX, fmt, ##args);\
				else \
					MY_SYSLOG( LOG_INFO, DP_INFO_TEXT, fmt, ##args);\
			} \
		}while(0)

	#ifdef NO_DEBUG
		#define LM_DBG( fmt, args...)
	#else
		#define LM_DBG( fmt, args...) \
			do { \
				if (is_printable(L_DBG)){ \
					if (log_stderr)\
						MY_DPRINT( DP_DBG_PREFIX, fmt, ##args);\
					else \
						MY_SYSLOG( LOG_DEBUG, DP_DBG_TEXT, fmt, ##args);\
				} \
			}while(0)
	#endif /*NO_DEBUG*/
#endif


#endif /* ifndef dprint_h */
