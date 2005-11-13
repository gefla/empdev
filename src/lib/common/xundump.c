/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2005, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                           Ken Stevens, Steve McClure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  ---
 *
 *  See the "LEGAL", "LICENSE", "CREDITS" and "README" files for all the
 *  related information and legal notices. It is expected that any future
 *  projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  xundump.c: Text loading functions based on xdump output
 * 
 *  Known contributors to this file:
 *     Ron Koenderink, 2005
 *  
 */

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "prototypes.h"
#include "file.h"
#include "nsc.h"
#include "match.h"

#define MAX_NUM_COLUMNS 256

static char *fname = "";
static int lineno = 0;

/*
 * TODO
 * This structure could be replaced with struct valstr.
 */
enum enum_value {
    VAL_NOTUSED,
    VAL_STRING,
    VAL_SYMBOL,
    VAL_DOUBLE
};

struct value {
    enum enum_value v_type;
    union {
	char *v_string;
	double v_double;
    } v_field;
};

static int
gripe(char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s:%d: ", fname, lineno);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    putc('\n', stderr);

    return -1;
}

static char *
xuesc(char *buf)
{
    char *src, *dst;
    int octal_chr, n;

    dst = buf;
    src = buf;
    while (*src) {
	if (*src == '\\') {
	    if (sscanf(++src, "%3o%n", &octal_chr, &n) != 1 || n != 3)
		return NULL;
	    *dst++ = (char)octal_chr;
	    src += 3;
	} else
	    *dst++ = *src++;
    }
    *dst = '\0';
    return buf;
}

static int
xuflds(FILE *fp, struct value values[])
{
    int i, ch;
    char sep;
    char buf[1024];

    for (i = 0; i < MAX_NUM_COLUMNS; i++) {
	ch = getc(fp);
	ungetc(ch, fp);

	switch (ch) {
	case '+': case '-': case '.':
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    if (fscanf(fp, "%lg%c", &values[i].v_field.v_double, &sep) != 2)
		return gripe("Malformed number in field %d", i + 1);
	    values[i].v_type = VAL_DOUBLE;
	    break;
	case '"':
	    if (fscanf(fp, "\"%1023[^ \n]%c", buf, &sep) != 2
		|| buf[strlen(buf)-1] != '"')
		return gripe("Malformed string in field %d", i + 1);
	    buf[strlen(buf)-1] = '\0';
	    if (!xuesc(buf))
		return gripe("Invalid escape sequence in field %d",
		    i + 1);
	    values[i].v_type = VAL_STRING;
	    values[i].v_field.v_string = strdup(buf);
	    break;
	default:
	    if (fscanf(fp, "%1023[^ \n]%c", buf, &sep) != 2) {
		return gripe("Junk in field %d", i + 1);
	    }
	    if (!strcmp(buf, "nil")) {
		values[i].v_field.v_string = NULL;
		values[i].v_type = VAL_STRING;
	    }
	    else {
		values[i].v_field.v_string = strdup(buf);
		values[i].v_type = VAL_SYMBOL;
	    }
	}
	if (sep == '\n')
	    break;
	if (sep != ' ')
	    return gripe(
		"Expected space or newline as field separator found %c",
		sep);
    }
    if (i >= MAX_NUM_COLUMNS)
	return gripe("Too many columns");
    if (i == 0)
	return gripe("No columns read");
    values[++i].v_type = VAL_NOTUSED;
    return i;
}

static int
xunsymbol(struct castr *ca, char *buf)
{
    struct symbol *symbol = (struct symbol *)empfile[ca->ca_table].cache;
    int i;
    int value = 0;
    char *token;

    if (ca->ca_flags & NSC_BITS)
	token = strtok( buf, "|");
    else
	token = buf;

    if (!token || token[0] == '\0')
	return gripe("Empty symbol value for field %s", ca->ca_name);

    while (token) {
	if ((i = stmtch(token, symbol, offsetof(struct symbol, name),
			sizeof(struct symbol))) != M_NOTFOUND) {
	    if (!(ca->ca_flags & NSC_BITS))
		return(symbol[i].value);
	    value |= symbol[i].value;
	    break;
	}
	else
	    return gripe("Symbol %s was not found for field %s", token,
		ca->ca_name);
	token = strtok(NULL, "|");
    }
    return(value);
}

static int
has_const(struct castr ca[])
{
    int i;

    for (i = 0; ca[i].ca_name; i++) {
	if (ca[i].ca_flags & NSC_CONST)
	    return 1;
    }
    return 0;
}

static void
xuinitrow(int type, int row)
{
    struct empfile *ep = &empfile[type];
    char *ptr = ep->cache + ep->size * row;

    memset(ptr, 0, ep->size);

    if (ep->init)
	ep->init(row, ptr);
}

static int
xuloadrow(int type, int row, struct value values[])
{
    int i,j,k;
    struct empfile *ep = &empfile[type];
    char *ptr = ep->cache + ep->size * row;
    struct castr *ca = ep->cadef;
    void *row_ref;

    i = 0;
    j = 0;
    while (ca[i].ca_type != NSC_NOTYPE &&
	   values[j].v_type != VAL_NOTUSED) {
	row_ref = (char *)ptr + ca[i].ca_off;
	k = 0;
	do {
	    /*
	     * TODO
	     * factor out NSC_CONST comparsion
	     */
	    switch (values[j].v_type) {
	    case VAL_SYMBOL:
		if (ca[i].ca_table == EF_BAD)
		    return(gripe("Found symbol string %s, but column %s "
			"is not symbol or symbol sets",
			values[j].v_field.v_string, ca[i].ca_name));
		values[j].v_field.v_double =
		    (double)xunsymbol(&ca[i], values[j].v_field.v_string);
		free(values[i].v_field.v_string);
		if (values[j].v_field.v_double < 0.0)
		    return -1;
		/*
		 * fall through
		 */
	    case VAL_DOUBLE:
		switch (ca[i].ca_type) {
		case NSC_INT:
		    if (ca[i].ca_flags & NSC_CONST) {
			if (((int *)row_ref)[k] != (int)
			     values[j].v_field.v_double)
			    gripe("Field %s must be same, "
				"read %d != expected %d",
				ca[i].ca_name,
				((int *)row_ref)[k],
				(int)values[j].v_field.v_double);

		    } else
			((int *)row_ref)[k] =
			    (int)values[j].v_field.v_double;
		    break;
		case NSC_LONG:
		    if (ca[i].ca_flags & NSC_CONST) {
			if (((long *)row_ref)[k] != (long)
			     values[j].v_field.v_double)
			    gripe("Field %s must be same, "
				"read %ld != expected %ld",
				ca[i].ca_name,
				((long *)row_ref)[k],
				(long)values[j].v_field.v_double);
		    } else
			((long *)row_ref)[k] = (long)
			    values[j].v_field.v_double;
		    break;
		case NSC_USHORT:
		    if (ca[i].ca_flags & NSC_CONST) {
			if (((unsigned short *)row_ref)[k] !=
			     (unsigned short)values[j].v_field.v_double)
			    gripe("Field %s must be same, "
				"read %d != expected %d",
				ca[i].ca_name,
				((unsigned short *)row_ref)[k],
				(unsigned short)values[j].v_field.v_double);
		    } else
			((unsigned short *)row_ref)[k] = (unsigned short)
			    values[j].v_field.v_double;
		    break;
		case NSC_UCHAR:
		    if (ca[i].ca_flags & NSC_CONST) {
			if (((unsigned char *)row_ref)[k] != (unsigned char)
			     values[j].v_field.v_double)
			    gripe("Field %s must be same, "
				"read %d != expected %d",
				ca[i].ca_name,
				((unsigned char *)row_ref)[k],
				(unsigned char)values[j].v_field.v_double);
		    } else
			((unsigned char *)row_ref)[k] = (unsigned char)
			    values[j].v_field.v_double;
		    break;
		case NSC_FLOAT:
		    if (ca[i].ca_flags & NSC_CONST) {
			if (((float *)row_ref)[k] != (float)
			     values[j].v_field.v_double)
			    gripe("Field %s must be same, "
				"read %g != expected %g",
				ca[i].ca_name,
				((float *)row_ref)[k],
				(float)values[j].v_field.v_double);
		    } else
			((float *)row_ref)[k] = (float)
			    values[j].v_field.v_double;
		    break;
		case NSC_STRING:
		    return gripe("Field %s is a string type, "
			"but %lg was read which is a number",
			ca[i].ca_name, values[j].v_field.v_double);
		default:
   		    return gripe("Field %s's type %d is not supported",
			ca[i].ca_name, ca[i].ca_type);
		}
		break;
	    case VAL_STRING:
		switch(ca[i].ca_type) {
		case NSC_STRINGY:
		    return gripe("Field %s is of NSC_STRINGY type "
			"which is not supported", ca[i].ca_name);
		case NSC_STRING:
    		    if (ca[i].ca_flags & NSC_CONST) {
			if (strcmp(((char **)row_ref)[k],
				   values[j].v_field.v_string) != 0)
			    gripe("Field %s must be same, "
				"read %s != expected %s",
				ca[i].ca_name,
				*((char **)row_ref)[k],
				*values[j].v_field.v_string);
		    } else
			((char **)row_ref)[k] = values[j].v_field.v_string;
		    break;
		case NSC_INT:
		case NSC_LONG:
		case NSC_USHORT:
		case NSC_UCHAR:
		case NSC_FLOAT:
		    return gripe("Field %s is a number type %d, "
			"but %s was read which is a string",
			ca[i].ca_name, ca[i].ca_type,
			values[j].v_field.v_string);
		default:
   		    return gripe("Field %s's type %d is not supported",
			    ca[i].ca_name, ca[i].ca_type);
		}
		break;
	    case VAL_NOTUSED:
		return gripe("Missing column %s in file", ca[i].ca_name);
	    default:
		return gripe("Unknown value type %d", values[j].v_type);
	    }
	    k++;
	    j++;
	} while (k < ca[i].ca_len);
	i++;
    }
    if (ca[i].ca_type != NSC_NOTYPE)
	return gripe("Missing column %s in file", ca[i].ca_name);
    switch  (values[j].v_type) {
    case VAL_NOTUSED:
	break;
    case VAL_STRING:
    case VAL_SYMBOL:
	return gripe("Extra junk after the last column, read %s",
	    values[j].v_field.v_string);
    case VAL_DOUBLE:
	return gripe("Extra junk after the last column, read %lg",
	    values[j].v_field.v_double);
    default:
	return gripe("Extra junk after the last column, "
	    "unknown value type %d", values[j].v_type);
    }
    return 0;
}

int
xundump(FILE *fp, char *file, int expected_table)
{
    char name[64];
    char sep;
    int row, rows, ch;
    struct value values[MAX_NUM_COLUMNS + 1];
    int type;
    int fixed_rows;

    if (strcmp(fname, file) != 0) {
        fname = file;
	lineno = 1;
    } else
	lineno++;

    if (fscanf(fp, "XDUMP %63[^0123456789]%*d%c", name, &sep) != 2)
	return gripe("Expected XDUMP header");
    if (sep != '\n')
	return gripe("Junk after XDUMP header");

    if (strlen(name) < 2)
	return gripe("Invalid table name in header %s", name);
    if (name[strlen(name) - 1] != ' ')
	return gripe("Missing space after table name in header %s",
	    name);
    name[strlen(name) - 1] = '\0';
    
    type = ef_byname(name);
    if (type < 0)
	return gripe("Table not found %s", name);

    if (expected_table != EF_BAD && expected_table != type)
	return gripe("Incorrect Table expecting %s got %s",
	    ef_nameof(expected_table), name);

    fixed_rows = has_const(ef_cadef(type));

    for (row = 0; ; row++) {
	lineno++;
	ch = getc(fp);
	ungetc(ch, fp);
	if (ch == EOF)
	    return gripe("Unexpected EOF");
	if (ch == '/')
	    break;
	/*
	 * TODO
	 * Add column count check to the return value of xuflds()
	 */
	if (xuflds(fp, values) <= 0)
	    return -1;
	else {
	    if (row >= empfile[type].csize - 1)
		return gripe("Too many rows for table %s", name);
	    empfile[type].fids = row + 1;
	    if (!fixed_rows)
		xuinitrow(type, row);
	    if (xuloadrow(type, row, values) < 0)
		return -1;
	}
    }

    if (fscanf(fp, "/%d%c", &rows, &sep) != 2)
	return gripe("Failed to find number of rows trailer");
    if (row != rows)
	return gripe("Number of rows doesn't match between "
	    "the trailer and what was read");
    if (fixed_rows && row != empfile[type].csize -1)
	return gripe("Number of rows doesn't match, and "
	    "it must for table %s", name);
    if (sep != '\n')
	return gripe("Junk after number of rows trailer");

    if (!fixed_rows)
	xuinitrow(type, row);

    return 0;
}
