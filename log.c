#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <pthread.h>


/*****************************************************/

#define LOG_FILENAME "/var/log/chessviewer"
#define LOG_SEPARATOR "/"

/*****************************************************/

FILE* log_file = NULL;
pthread_mutex_t log_mutex;
bool log_init_done = false;

/*****************************************************/

static bool log_open_file(const char* filename);
static void log_close_file();

static void log_add_timestamp();
static void log_add_level(LogLevel level);

/*****************************************************/

void log_init()
{
	 log_init_done = true;
	 (void) pthread_mutex_init( &log_mutex, NULL );
}

void log_close()
{
	if ( log_init_done ) {
		log_init_done = false;
		(void) pthread_mutex_destroy( &log_mutex );
	}
}

void log_add(LogLevel level, const char* frmt, ...)
{
	if ( !log_init_done ) {
		return;
	}

	pthread_mutex_lock( &log_mutex );

	if ( !log_open_file( LOG_FILENAME ) )
			{
		/* failed */
		pthread_mutex_unlock( &log_mutex );
		return;
	}

	log_add_timestamp();
	fputs( LOG_SEPARATOR, log_file);
	log_add_level(level);
	fputs( LOG_SEPARATOR, log_file);

	va_list args;

	va_start(args, frmt);
	vfprintf(log_file, frmt, args);
	va_end(args);

	size_t len = strlen( frmt );
	if ( len > 0 && frmt[ len - 1 ] != '\n' ) {
		fputs("\n", log_file);
	}

	log_close_file();

	pthread_mutex_unlock( &log_mutex );
}

/*****************************************************/

static bool log_open_file(const char* filename)
{
	if ( NULL == log_file)
			{
		log_file = fopen(filename, "a");
	}
	return NULL != log_file;
}

static void log_close_file()
{
	if ( NULL == log_file) {
		return;
	}

	fflush(log_file);
	fclose(log_file);
	log_file = NULL;
}

static void log_add_timestamp()
{
	if (NULL == log_file) {
		return;
	}

	time_t now;
	time(&now);

	struct tm* localtm = localtime(&now);

	char buf[32];
	strftime(buf, 32, "%F %T", localtm);

	fputs(buf, log_file);
}

static void log_add_level(LogLevel level)
{
	if ( NULL == log_file ) {
		return;
	}

	switch (level)
	{
	case DEBUG:
		fputs("D", log_file);
		break;
	case INFO:
		fputs("I", log_file);
		break;
	case WARNING:
		fputs("W", log_file);
		break;
	case ERROR:
		fputs("E", log_file);
		break;
	}
}
