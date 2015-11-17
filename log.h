#ifndef __log_h__
#define __log_h__

/* use LOG macro for logging */
#define LOG_LEVEL_OFF ERROR + 1

#define LOG_LEVEL DEBUG

#define LOG( level, ... ) \
		do { if ( level >= LOG_LEVEL ) log_add( level, __VA_ARGS__ ); } while (0)

typedef enum
{
	DEBUG,
	INFO,
	WARNING,
	ERROR
} LogLevel;


void log_init();
void log_close();

void log_add(LogLevel level, const char* frmt, ...);

#endif /* __log_h__ */
