# include "dgd.h"
# include <time.h>
# include <sys\timeb.h>

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970
 */
Uint P_time(void)
{
    return (Uint) time((time_t *) NULL);
}

/*
 * NAME:	P->mtime()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970 in milliseconds
 */
Uint P_mtime(unsigned short *milli)
{
    struct _timeb t;

    _ftime(&t);
    *milli = t.millitm;
    return (Uint) t.time;
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	return time as string
 */
char *P_ctime(char *buf, Uint t)
{
    int offset;

    offset = 0;
    for (offset = 0; t > 2147397248; t -= 883612800, offset += 28) ;
    memcpy(buf, ctime((time_t *) &t), 26);
    if (offset != 0) {
	long year;

	year = strtol(buf + 20, (char **) NULL, 10) + offset;
	if ((year > 2100 ||
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
    if (buf[8] == '0') {
	buf[8] = ' ';	/* MSDEV ctime weirdness */
    }
    return buf;
}

static struct _timeb timeout;

/*
 * NAME:        P->timer()
 * DESCRIPTION: set the timer to go off at some time in the future, or disable
 *		it
 */
void P_timer(Uint t, unsigned int mtime)
{
    timeout.time = t;
    timeout.millitm = mtime;
}

/*
 * NAME:        P->timeout()
 * DESCRIPTION: return TRUE if there is a timeout, FALSE otherwise
 */
bool P_timeout(Uint *t, unsigned short *mtime)
{
    struct _timeb time;

    _ftime(&time);
    *t = time.time;
    *mtime = time.millitm;

    if (timeout.time == 0) {
	return FALSE;
    }
    return (time.time > timeout.time || 
	    (time.time == timeout.time && time.millitm >= timeout.millitm));
}
