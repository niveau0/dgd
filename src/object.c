# include "dgd.h"
# include <ctype.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"

static object *otab;		/* object table */
static int otabsize;		/* size of object table */
static hashtab *htab;		/* object name hash table */
static object *free_obj;	/* free object list */
static object *dest_obj;	/* destructed object list */
static uindex nobjects;		/* number of objects in object table */
static uindex nfreeobjs;	/* number of objects in free list */
static Int count;		/* object creation count */

/*
 * NAME:	object->init()
 * DESCRIPTION:	initialize the object tables
 */
void o_init(n)
int n;
{
    otab = ALLOC(object, otabsize = n);
    htab = ht_new(OBJTABSZ, OBJHASHSZ);
    free_obj = (object *) NULL;
    nobjects = 0;
    nfreeobjs = 0;
    count = 0;
}

/*
 * NAME:	object->new()
 * DESCRIPTION:	create a new object. If the master argument is non-NULL, the
 *		new object is a clone of this master object; otherwise it is
 *		a new master object.
 */
object *o_new(name, master, ctrl)
char *name;
object *master;
control *ctrl;
{
    register object *o;

    /* allocate object */
    if (free_obj != (object *) NULL) {
	/* get space from free object list */
	o = free_obj;
	free_obj = (object *) o->chain.next;
	--nfreeobjs;
    } else {
	/* use new space in object table */
	if (nobjects == otabsize) {
	    fatal("too many objects");
	}
	o = &otab[nobjects];
	o->index = nobjects++;
    }
    o->count = ++count;

    if (master == (object *) NULL) {
	register dinherit *inh;
	register int i;
	hte **h;

	/* put object in object name hash table */
	mstatic();
	strcpy(o->chain.name = ALLOC(char, strlen(name) + 1), name);
	mdynamic();
	h = ht_lookup(htab, name);
	o->chain.next = *h;
	*h = (hte *) o;
	o->flags = O_MASTER;
	o->ctrl = ctrl;
	ctrl->inherits[ctrl->ninherits - 1].obj = o;

	/* add reference to all inherited objects */
	o->u.ref = 0;	/* increased to 1 in following loop */
	inh = ctrl->inherits;
	for (i = ctrl->ninherits; i > 0; --i) {
	    (inh++)->obj->u.ref++;
	}
    } else {
	/* ignore the object name */
	o->chain.name = (char *) NULL;

	/* add reference to master object */
	o->flags = 0;
	(o->u.master = master)->u.ref++;
	o->ctrl = (control *) NULL;
    }
    o->data = (dataspace *) NULL;
    o->dfirst = SW_UNUSED;

    return o;
}

/*
 * NAME:	object->delete()
 * DESCRIPTION:	delete a reference to an object, and put it in the deleted
 *		list if it was the last reference
 */
static void o_delete(o)
register object *o;
{
    if (o->flags & O_MASTER) {
	register int i;
	register dinherit *inh;

	if (--(o->u.ref) != 0) {
	    /* last reference not removed yet */
	    return;
	}
	if (o->ctrl == (control *) NULL) {
	    o->ctrl = d_load_control(o->cfirst);
	}
	/* remove references to inherited objects too */
	inh = o->ctrl->inherits;
	for (i = o->ctrl->ninherits; i > 0; --i) {
	    /*
	     * NOTE: this will call o_delete for this object again,
	     * but now the u.ref field will become -1
	     */
	    o_delete((inh++)->obj);
	}
    } else {
	o_delete(o->u.master);
    }

    /* put in deleted list */
    o->chain.next = (hte *) dest_obj;
    dest_obj = o;
}

/*
 * NAME:	object->del()
 * DESCRIPTION:	delete an object
 */
void o_del(o)
register object *o;
{
    if (o->count == 0) {
	/* objects can only be destructed once */
	error("Destructing destructed object");
    }
    o->count = 0;

    if (o->chain.name != (char *) NULL) {
	/* remove from object name hash table */
	*ht_lookup(htab, o->chain.name) = o->chain.next;
    }
    o_delete(o);
}

/*
 * NAME:	object->object()
 * DESCRIPTION:	get an object, given index and count
 */
object *o_object(idx, count)
uindex idx;
Int count;
{
    register object *o;

    o = &otab[idx];
    return (o->count == count) ? o : (object *) NULL;
}

/*
 * NAME:	object->name()
 * DESCRIPTION:	return the name of an object
 */
char *o_name(o)
register object *o;
{
    if (o->chain.name != (char *) NULL) {
	return o->chain.name;
    } else {
	static char name[STRINGSZ + 12];

	/*
	 * return the name of the master object with the index appended
	 */
	sprintf(name, "%s#%d", o->u.master->chain.name, o->index);
	return name;
    }
}

/*
 * NAME:	object->rename()
 * DESCRIPTION:	rename an object
 */
void o_rename(o, name)
register object *o;
char *name;
{
    hte **h;

    if (strchr(name, '#') != (char *) NULL) {
	error("Illegal object name");
    }
    if (o_find(name) != (object *) NULL) {
	error("Rename to existing object");
    }
    if (o->chain.name != (char *) NULL) {
	/* remove from object name hash table */
	*ht_lookup(htab, o->chain.name) = o->chain.next;
	FREE(o->chain.name);
    }
    /* put new name in object name hash table */
    mstatic();
    strcpy(o->chain.name = ALLOC(char, strlen(name) + 1), name);
    mdynamic();
    h = ht_lookup(htab, name);
    o->chain.next = *h;
    *h = (hte *) o;
}

/*
 * NAME:	object->find()
 * DESCRIPTION:	find an object by name
 */
object *o_find(name)
char *name;
{
    char *hash;

    hash = strchr(name, '#');
    if (hash != (char *) NULL) {
	register char *p;
	register unsigned long number;
	register object *o;

	/*
	 * Look for a cloned object, which cannot be found directly in the
	 * object name hash table.
	 * The name must be of the form filename#1234, where 1234 is the
	 * decimal representation of the index in the object table.
	 */
	p = hash + 1;
	if (*p == '\0' || (*p == '0' && p[1] != '\0')) {
	    /* don't accept "filename#" or "filename#01" */
	    return (object *) NULL;
	}

	/* convert the string to a number */
	number = 0;
	do {
	    if (!isdigit(*p)) {
		return (object *) NULL;
	    }
	    number = number * 10 + *p++ - '0';
	    if (number >= nobjects) {
		return (object *) NULL;
	    }
	} while (*p != '\0');

	o = &otab[number];
	if (o->count == 0 || (o->flags & O_MASTER) ||
	    strncmp(name, o->u.master->chain.name, hash - name) != 0 ||
	    o->u.master->chain.name[hash - name] != '\0') {
	    /*
	     * no entry, not a clone, or object name doesn't match
	     */
	    return (object *) NULL;
	}
	return o;
    } else {
	/* look it up in the hash table */
	return (object *) *ht_lookup(htab, name);
    }
}

/*
 * NAME:	object->control()
 * DESCRIPTION:	return the control block for an object
 */
control *o_control(obj)
object *obj;
{
    register object *o;

    o = obj;
    if (!(o->flags & O_MASTER)) {
	o = o->u.master;	/* get control block of master object */
    }
    if (o->ctrl == (control *) NULL) {
	o->ctrl = d_load_control(o->cfirst);	/* reload */
    } else {
	d_ref_control(o->ctrl);
    }
    return obj->ctrl = o->ctrl;
}

/*
 * NAME:	object->dataspace()
 * DESCRIPTION:	return the dataspace block for an object
 */
dataspace *o_dataspace(o)
register object *o;
{
    if (o->data == (dataspace *) NULL) {
	if (o->dfirst == SW_UNUSED) {
	    /* create new dataspace block */
	    o->data = d_new_dataspace(o);
	} else {
	    /* load dataspace block */
	    o->data = d_load_dataspace(o, o->dfirst);
	}
    } else {
	d_ref_dataspace(o->data);
    }
    return o->data;
}

/*
 * NAME:	object->clean()
 * DESCRIPTION:	actually remove all deleted objects
 */
void o_clean()
{
    register object *o, *next;

    for (o = dest_obj; o != (object *) NULL; o = next) {
	/* free dataspace and control block (if they exist) */
	if (o->data == (dataspace *) NULL && o->dfirst != SW_UNUSED) {
	    /* reload dataspace block (sectors are needed) */
	    o->data = d_load_dataspace(o, o->dfirst);
	}
	if (o->data != (dataspace *) NULL) {
	    d_del_dataspace(o->data);
	}
	if (o->flags & O_MASTER) {
	    d_del_control(o_control(o));
	}
	if (o->chain.name != (char *) NULL) {
	    FREE(o->chain.name);	/* free object name */
	}

	next = (object *) o->chain.next;
	o->chain.next = (hte *) free_obj;	/* put object in free list */
	free_obj = o;
	nfreeobjs++;
    }
    dest_obj = (object *) NULL;
}

/*
 * NAME:	object->count()
 * DESCRIPTION:	return the number of objects in use
 */
uindex o_count()
{
    return nobjects - nfreeobjs;
}
