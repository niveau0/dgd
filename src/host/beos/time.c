# include <time.h>
# include <sys/time.h>
# include "dgd.h"

static struct timeval timeout;

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the current time
 */
Uint P_time()
{
    return (Uint) time((time_t *) NULL);
}

/*      
 * NAME:	P->mtime()
 * DESCRIPTION:	return the current time in milliseconds
 */ 
Uint P_mtime(milli)
unsigned short *milli;  
{   
    struct timeval time;

    gettimeofday(&time, (struct timezone *) NULL);
    *milli = time.tv_usec / 1000;
    return (Uint) time.tv_sec;
}    

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	convert the given time to a string
 */
char *P_ctime(char *buf, Uint t)
{
    register int offset;

    offset = 0;
    for (offset = 0; t > 2147397248; t -= 883612800, offset += 28) ;
    memcpy(buf, ctime((time_t *) &t), 26);
    if (offset != 0) {
	long year;

	year = strtol(buf + 20, (char **) NULL, 10) + offset;
	if (year > 2100 ||
	    (year == 2100 && (buf[4] != 'J' || buf[5] != 'a') &&
	     (buf[4] != 'F' || (buf[8] == '2' && buf[9] == '9')))) {
	    /* 2100 is not a leap year */
	    t -= 378604800;
	    offset += 12;
	    memcpy(buf, ctime((time_t *) &t), 26);
	    year = strtol(buf + 20, (char **) NULL, 10) + offset;
	}
	sprintf(buf + 20, "%ld\012", year);
    }
    return buf;
}

/*
 * NAME:	P->timer()
 * DESCRIPTION:	set the timer to go off at some time in the future, or disable
 *		it
 */
void P_timer(t, mtime)
Uint t;
unsigned int mtime;
{
    timeout.tv_sec = t;
    timeout.tv_usec = mtime * 1000;
}

/*
 * NAME:	P->timeout()
 * DESCRIPTION:	return TRUE if there is a timeout, FALSE otherwise
 */
bool P_timeout()
{
    struct timeval t;

    if (timeout.tv_sec == 0) {
	/* timer disabled */
	return FALSE;
    }
    gettimeofday(&t, (struct timezone *) NULL);
    return (t.tv_sec > timeout.tv_sec ||
	    (t.tv_sec == timeout.tv_sec && t.tv_usec >= timeout.tv_usec));
}
