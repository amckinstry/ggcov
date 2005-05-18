/*
 * ggcov - A GTK frontend for exploring gcov coverage data
 * Copyright (c) 2005 Greg Banks <gnb@alphalink.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "common.h"
#include "cov.H"
#include "filename.h"
#include "estring.H"
#include "tok.H"
#include "php_serializer.H"
#include "report.H"
#include "fakepopt.h"
#include <db.h>

CVSID("$Id: ggcov-webdb.c,v 1.1 2005-05-18 13:07:50 gnb Exp $");

char *argv0;
GList *files;	    /* incoming specification from commandline */

static int recursive = FALSE;	/* needs to be int (not gboolean) for popt */
static char *suppressed_ifdefs = 0;
static char *object_dir = 0;
static const char *debug_str = 0;
static char *output_tarball = "ggcov.webdb.tgz";

static hashtable_t<void, unsigned int> *file_index;
static hashtable_t<void, unsigned int> *function_index;
static hashtable_t<void, unsigned int> *callnode_index;
static list_t<cov_function_t> *all_functions;

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

inline DBT *dbt(char *data, unsigned int length)
{
#define MAX_DBTS    2
    static DBT bufs[MAX_DBTS];
    static int count = 0;
    DBT *d = &bufs[(count++) % MAX_DBTS];

    memset(d, 0, sizeof(*d));
    d->data = data;
    d->size = length;

    return d;
#undef MAX_DBTS
}

inline DBT *dbt(const estring &e)
{
    return dbt((char *)e.data(), e.length());
}

inline DBT *dbt(const php_serializer_t &ser)
{
    return dbt(ser.data());
}

inline DBT *dbt(const char *s)
{
    return dbt((char *)s, (s == 0 ? 0 : strlen(s)));
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static inline unsigned int
ftag(const cov_file_t *f)
{
    return *file_index->lookup((void *)f);
}

static inline unsigned int
fntag(const cov_function_t *fn)
{
    return *function_index->lookup((void *)fn);
}

static inline unsigned int
cntag(const cov_callnode_t *cn)
{
    return *callnode_index->lookup((void *)cn);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static inline const char *
get_tmpdir(void)
{
    char *v = getenv("TMPDIR");
    return (v == 0 ? "/tmp" : v);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/


#ifdef __GNUC__
static int systemf(const char *fmt, ...) __attribute__(( format(printf,1,2) ));
#endif

static int
systemf(const char *fmt, ...)
{
    estring cmd;
    va_list args;

    va_start(args, fmt);
    cmd.append_vprintf(fmt, args);
    va_end(args);

    dprintf1(D_WEB, "Running \"%s\"\n", cmd.data());
    return system(cmd.data());
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
/* 
 * TODO: This function should be common with the identical code in ggcov.c
 */

static void
read_gcov_files(void)
{
    GList *iter;
    
    cov_init();

    if (suppressed_ifdefs != 0)
    {
    	tok_t tok(/*force copy*/(const char *)suppressed_ifdefs, ", \t");
	const char *v;
	
	while ((v = tok.next()) != 0)
    	    cov_suppress_ifdef(v);
    }

    cov_pre_read();
    
    if (object_dir != 0)
    	cov_add_search_directory(object_dir);

    if (files == 0)
    {
    	if (!cov_read_directory(".", recursive))
	    exit(1);
    }
    else
    {
	for (iter = files ; iter != 0 ; iter = iter->next)
	{
	    const char *filename = (const char *)iter->data;
	    
	    if (file_is_directory(filename) == 0)
	    	cov_add_search_directory(filename);
    	}

	for (iter = files ; iter != 0 ; iter = iter->next)
	{
	    const char *filename = (const char *)iter->data;
	    
	    if (file_is_directory(filename) == 0)
	    {
	    	if (!cov_read_directory(filename, recursive))
		    exit(1);
	    }
	    else if (file_is_regular(filename) == 0)
	    {
	    	if (cov_is_source_filename(filename))
		{
		    if (!cov_read_source_file(filename))
			exit(1);
		}
		else
		{
		    if (!cov_read_object_file(filename))
			exit(1);
		}
	    }
	    else
	    {
	    	fprintf(stderr, "%s: don't know how to handle this filename\n",
		    	filename);
		exit(1);
	    }
	}
    }
    
    cov_post_read();
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
save_file_lines(DB *db, cov_file_t *f)
{
    php_serializer_t ser;
    unsigned int lineno;
    int ret;
    char blocks_buf[64];

    // PHP-serialise the line array
    ser.begin_array(f->num_lines());
    for (lineno = 1 ; lineno <= f->num_lines() ; lineno++)
    {
	cov_line_t *ln = f->nth_line(lineno);

	ser.integer(lineno);

	ser.begin_array(3);
	ser.next_key(); ser.integer(ln->status());
	ser.next_key(); ser.integer(ln->count());
	ln->format_blocks(blocks_buf, sizeof(blocks_buf)-1);
	ser.next_key(); ser.string(blocks_buf);
	ser.end_array();
    }
    ser.end_array();
    
    // Store the serialised line array
    string_var key = g_strdup_printf("FL%u", ftag(f));
    if (ret = db->put(db, 0, dbt(key), dbt(ser), 0))
    {
	db->err(db, ret, "save_filename_index");
	exit(1);
    }
}

static void
save_lines(DB *db)
{
    list_iterator_t<cov_file_t> iter;
    
    for (iter = cov_file_t::first() ; iter != (cov_file_t *)0 ; ++iter)
    	save_file_lines(db, *iter);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
build_filename_index(void)
{
    list_iterator_t<cov_file_t> iter;
    unsigned int n = 0;

    // Build the filename index
    file_index = new hashtable_t<void, unsigned int>;
    for (iter = cov_file_t::first() ; iter != (cov_file_t *)0 ; ++iter)
    {
	cov_file_t *f = *iter;
	file_index->insert((void *)f, new unsigned int(++n));
    }
}

static void
save_filename_index(DB *db)
{
    php_serializer_t ser;
    list_iterator_t<cov_file_t> iter;
    int ret;

    // PHP-serialise the filename index.
    ser.begin_array(file_index->size());
    for (iter = cov_file_t::first() ; iter != (cov_file_t *)0 ; ++iter)
    {
	cov_file_t *f = *iter;
	ser.string(f->minimal_name());
	ser.integer(ftag(f));
    }
    ser.end_array();

    // Store the serialised filename index
    if (ret = db->put(db, 0, dbt("FI"), dbt(ser), 0))
    {
	db->err(db, ret, "save_filename_index");
	exit(1);
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

// like cov_function_t::list_all() but includes suppressed
// functions as well.  TODO: is that a good idea?

static list_t<cov_function_t> *
really_list_all_functions(void)
{
    list_t<cov_function_t> *all = new list_t<cov_function_t>;
    list_iterator_t<cov_file_t> iter;
    
    for (iter = cov_file_t::first() ; iter != (cov_file_t *)0 ; ++iter)
    {
    	cov_file_t *f = *iter;
	unsigned int fnidx;

	for (fnidx = 0 ; fnidx < f->num_functions() ; fnidx++)
	{
    	    cov_function_t *fn = f->nth_function(fnidx);

	    all->append(fn);
	}
    }

    all->sort(cov_function_t::compare);
    return all;
}

static void
build_global_function_index(void)
{
    list_iterator_t<cov_function_t> iter;
    unsigned int n = 0;

    all_functions = really_list_all_functions();

    function_index = new hashtable_t<void, unsigned int>;
    for (iter = all_functions->first() ; iter != (cov_function_t *)0 ; ++iter)
    {
	cov_function_t *fn = *iter;
	function_index->insert((void *)fn, new unsigned int(++n));
    }
}

static gboolean
unique_remove_one(const char *key, list_t<cov_function_t> *value, void *closure)
{
    value->remove_all();
    delete value;
    return TRUE;
}

static void
save_global_function_index(DB *db)
{
    php_serializer_t ser;
    hashtable_t<const char, list_t<cov_function_t> > *unique;
    list_t<const char> keys;
    list_iterator_t<cov_function_t> fniter;
    list_iterator_t<const char> kiter;
    int ret;

    // Uniquify the function names
    unique = new hashtable_t<const char, list_t<cov_function_t> >;
    for (fniter = all_functions->first() ; fniter != (cov_function_t *)0 ; ++fniter)
    {
    	cov_function_t *fn = *fniter;
	if (fn->is_suppressed())
	    continue;
	list_t<cov_function_t> *list = unique->lookup(fn->name());
	if (list == 0)
	{
	    list = new list_t<cov_function_t>;
	    unique->insert(fn->name(), list);
	}
	list->append(fn);
    }
    
    // PHP-serialise the function index
    unique->keys(&keys);
    ser.begin_array(unique->size());
    for (kiter = keys.first() ; kiter != (const char *)0 ; ++kiter)
    {
    	const char *fname = *kiter;
	list_t<cov_function_t> *list = unique->lookup(fname);

	ser.string(fname);
	ser.begin_array(list->length());
	for (fniter = list->first() ; fniter != (cov_function_t *)0 ; ++fniter)
	{
	    cov_function_t *fn = *fniter;
	    ser.string(fn->file()->minimal_name());
	    ser.integer(fntag(fn));
	}
	ser.end_array();
    }
    ser.end_array();

    // Store the serialised function list
    if (ret = db->put(db, 0, dbt("UI"), dbt(ser), 0))
    {
	db->err(db, ret, "save_global_function_index");
	exit(1);
    }

    unique->foreach_remove(unique_remove_one, 0);
    delete unique;
}


static void
save_global_function_list(DB *db)
{
    php_serializer_t ser;
    list_iterator_t<cov_function_t> iter;
    estring label;
    unsigned int n = 0;
    int ret;

    for (iter = all_functions->first() ; iter != (cov_function_t *)0 ; ++iter)
    {
    	cov_function_t *fn = *iter;

	if (fn->is_suppressed())
	    continue;
	n++;
    }

    // PHP-serialise the function list
    ser.begin_array(n);
    for (iter = all_functions->first() ; iter != (cov_function_t *)0 ; ++iter)
    {
    	cov_function_t *fn = *iter;

	if (fn->is_suppressed())
	    continue;

    	label.truncate();
	label.append_string(fn->name());

    	/* see if we need to present some more scope to uniquify the name */
	list_iterator_t<cov_function_t> niter = iter.peek_next();
	list_iterator_t<cov_function_t> piter = iter.peek_prev();
	if ((niter != (cov_function_t *)0 &&
	     !strcmp((*niter)->name(), fn->name())) ||
	    (piter != (cov_function_t *)0 &&
	     !strcmp((*piter)->name(), fn->name())))
	{
	    label.append_string(" [");
	    label.append_string(fn->file()->minimal_name());
	    label.append_string("]");
	}
	
	ser.string(label);
	ser.string(fn->file()->minimal_name());
    }
    ser.end_array();

    // Store the serialised function list
    if (ret = db->put(db, 0, dbt("UL"), dbt(ser), 0))
    {
	db->err(db, ret, "save_global_function_list");
	exit(1);
    }
}

static void
save_file_function_indexes(DB *db)
{
    list_iterator_t<cov_file_t> iter;
    
    for (iter = cov_file_t::first() ; iter != (cov_file_t *)0 ; ++iter)
    {
    	cov_file_t *f = *iter;
	unsigned int fnidx;
	php_serializer_t ser;
	int ret;

	ser.begin_array(f->num_functions());
	for (fnidx = 0 ; fnidx < f->num_functions() ; fnidx++)
	{
    	    cov_function_t *fn = f->nth_function(fnidx);

	    ser.string(fn->name());
	    ser.integer(fntag(fn));
	}
	ser.end_array();

	// Store the serialised function index
	string_var key = g_strdup_printf("FUI%u", ftag(f));
	if (ret = db->put(db, 0, dbt(key), dbt(ser), 0))
	{
	    db->err(db, ret, "save_file_function_indexes");
	    exit(1);
	}
    }
}

static inline void
serlineno(php_serializer_t *ser, const cov_location_t *loc)
{
    ser->integer(loc == 0 ? 0UL : loc->lineno);
}

static void
save_functions(DB *db)
{
    list_iterator_t<cov_function_t> iter;
    
    for (iter = all_functions->first() ; iter != (cov_function_t *)0 ; ++iter)
    {
	cov_function_t *fn = *iter;
	php_serializer_t ser;
	int ret;

	ser.begin_array(5);
	ser.next_key(); ser.string(fn->name());
	ser.next_key(); ser.integer(fn->status());
	ser.next_key(); ser.string(fn->file()->minimal_name());
	ser.next_key(); serlineno(&ser, fn->get_first_location());
	ser.next_key(); serlineno(&ser, fn->get_last_location());
	ser.end_array();

	// Store the function data
	string_var key = g_strdup_printf("U%u", fntag(fn));
	if (ret = db->put(db, 0, dbt(key), dbt(ser), 0))
	{
	    db->err(db, ret, "save_functions");
	    exit(1);
	}
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
serialise_ulong_array(
    php_serializer_t *ser,
    unsigned int n,
    const unsigned long *p)
{
    unsigned int i;

    ser->begin_array(n);
    for (i = 0 ; i < n ; i++)
    {
	ser->next_key();
	ser->integer(p[i]);
    }
    ser->end_array();
}

static void
save_one_summary_f(DB *db, cov_scope_t *sc, const char *key)
{
    php_serializer_t ser;
    const cov_stats_t *stats = sc->get_stats();
    int ret;

    // PHP-serialise the scope's stats
    ser.begin_array(5);
    ser.next_key();
    serialise_ulong_array(&ser, cov::NUM_STATUS, stats->blocks_by_status());
    ser.next_key();
    serialise_ulong_array(&ser, cov::NUM_STATUS, stats->lines_by_status());
    ser.next_key();
    serialise_ulong_array(&ser, cov::NUM_STATUS, stats->functions_by_status());
    ser.next_key();
    serialise_ulong_array(&ser, cov::NUM_STATUS, stats->calls_by_status());
    ser.next_key();
    serialise_ulong_array(&ser, cov::NUM_STATUS, stats->branches_by_status());
    ser.end_array();
    
    if (ret = db->put(db, 0, dbt(key), dbt(ser), 0))
    {
	db->err(db, ret, "save_one_summary_f");
	exit(1);
    }
}

static void
save_summaries(DB *db)
{
    cov_scope_t *sc;

    // Save an overall scope object
    sc = new cov_overall_scope_t;
    save_one_summary_f(db, sc, "OS");
    delete sc;

    // Save a file scope object for each file
    list_iterator_t<cov_file_t> fiter;
    for (fiter = cov_file_t::first() ; fiter != (cov_file_t *)0 ; ++fiter)
    {
	cov_file_t *f = *fiter;
	sc = new cov_file_scope_t(f);
	string_var key = g_strdup_printf("FS%u", ftag(f));
	save_one_summary_f(db, sc, key);
	delete sc;
    }

    // Save a function scope object for each function
    list_iterator_t<cov_function_t> fniter;
    for (fniter = all_functions->first() ; fniter != (cov_function_t *)0 ; ++fniter)
    {
    	cov_function_t *fn = *fniter;
	sc = new cov_function_scope_t(fn);
	string_var key = g_strdup_printf("US%u", fntag(fn));
	save_one_summary_f(db, sc, key);
	delete sc;
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
add_callnode_to_index(cov_callnode_t *cn, void *closure)
{
    unsigned int *np = (unsigned int *)closure;
    callnode_index->insert((void *)cn, new unsigned int(++(*np)));
}

static void
build_callnode_index(void)
{
    unsigned int n = 0;

    callnode_index = new hashtable_t<void, unsigned int>;
    cov_callnode_t::foreach(add_callnode_to_index, &n);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
add_callnode(cov_callnode_t *cn, void *closure)
{
    list_t<cov_callnode_t> *list = (list_t<cov_callnode_t> *)closure;

    list->append(cn);
}

static int
compare_node_by_name(const cov_callnode_t *cn1, const cov_callnode_t *cn2)
{
    return strcmp(cn1->name, cn2->name);
}

static void
save_callnode_index(DB *db)
{
    list_t<cov_callnode_t> all;
    list_iterator_t<cov_callnode_t> iter;
    php_serializer_t ser;
    int ret;

    // Sort the callnode index, for the node <select>
    cov_callnode_t::foreach(add_callnode, &all);
    all.sort(compare_node_by_name);

    // PHP-serialise the callnode index
    ser.begin_array(callnode_index->size());
    for (iter = all.first() ; iter != (cov_callnode_t *)0 ; ++iter)
    {
	cov_callnode_t *cn = *iter;

	ser.string(cn->name);
	ser.integer(cntag(cn));
    }
    ser.end_array();

    all.remove_all();

    // Save the callnode index
    if (ret = db->put(db, 0, dbt("NI"), dbt(ser), 0))
    {
	db->err(db, ret, "save_callnode_index");
	exit(1);
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static int
compare_in_arc_by_count(gconstpointer a, gconstpointer b)
{
    const cov_callarc_t *ca1 = (const cov_callarc_t *)a;
    const cov_callarc_t *ca2 = (const cov_callarc_t *)b;
    int r;

    r = -u64cmp(ca1->count, ca2->count);
    if (!r)
	r = strcmp(ca1->from->name, ca2->from->name);
    return r;
}

static int
compare_out_arc_by_count(gconstpointer a, gconstpointer b)
{
    const cov_callarc_t *ca1 = (const cov_callarc_t *)a;
    const cov_callarc_t *ca2 = (const cov_callarc_t *)b;
    int r;

    r = -u64cmp(ca1->count, ca2->count);
    if (!r)
	r = strcmp(ca1->to->name, ca2->to->name);
    return r;
}

static void
serialize_arc_list(php_serializer_t *ser, GList *arcs, gboolean out)
{
    GList *copy = g_list_sort(g_list_copy(arcs),
		    (out ? compare_out_arc_by_count : compare_in_arc_by_count));
    GList *iter;

    ser->begin_array(g_list_length(copy));
    for (iter = copy ; iter != NULL ; iter = iter->next)
    {
	cov_callarc_t *ca = (cov_callarc_t *)iter->data;
	cov_callnode_t *peer;

	ser->next_key();
	ser->begin_array(3);
	peer = (out ? ca->to : ca->from);
	ser->next_key(); ser->string(peer->name);
	ser->next_key(); ser->integer(cntag(peer));
	ser->next_key(); ser->integer(ca->count);
	ser->end_array();
    }
    ser->end_array();

    listclear(copy);
}

static void
save_callnode(cov_callnode_t *cn, void *closure)
{
    DB *db = (DB *)closure;
    php_serializer_t ser;
    int ret;

    ser.begin_array(4);
    ser.next_key(); ser.string(cn->name);
    ser.next_key(); ser.integer(cn->count);
    ser.next_key(); serialize_arc_list(&ser, cn->in_arcs, FALSE);
    ser.next_key(); serialize_arc_list(&ser, cn->out_arcs, TRUE);
    ser.end_array();

    // Save the callnode
    string_var key = g_strdup_printf("N%u", cntag(cn));
    if (ret = db->put(db, 0, dbt(key), dbt(ser), 0))
    {
	db->err(db, ret, "save_callnode");
	exit(1);
    }
}

static void
save_callgraph(DB *db)
{
    cov_callnode_t::foreach(save_callnode, db);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
save_report_index(DB *db)
{
    const report_t *rep;
    unsigned int n;
    php_serializer_t ser;
    int ret;

    // PHP-serialise the report index
    for (rep = all_reports, n = 0 ; rep->name != 0 ; rep++, n++)
	;
    ser.begin_array(n);
    for (rep = all_reports, n = 0 ; rep->name != 0 ; rep++, n++)
    {
	ser.string(rep->name);
	ser.begin_array(2);
	ser.next_key(); ser.integer(n);
	ser.next_key(); ser.string(rep->label);
	ser.end_array();
    }
    ser.end_array();

    // Save the report index
    if (ret = db->put(db, 0, dbt("RI"), dbt(ser), 0))
    {
	db->err(db, ret, "save_report_index");
	exit(1);
    }
}

static void
save_reports(DB *db)
{
    const report_t *rep;
    unsigned int n;
    string_var tmp_filename;
    int fd;
    FILE *fp;
    int len;
    estring buffer;

    tmp_filename = g_strconcat(get_tmpdir(),
			       "/ggcov-web-reportXXXXXX",
			       (char *)0);
    if ((fd = mkstemp((char *)tmp_filename.data())) < 0)
    {
	perror(tmp_filename);
	exit(1);
    }
    if ((fp = fdopen(fd, "r+")) == NULL)
    {
	perror("fdopen");
	close(fd);
	exit(1);
    }

    for (rep = all_reports, n = 0 ; rep->name != 0 ; rep++, n++)
    {
	php_serializer_t ser;
	string_var key;
	int ret;

	// Rewind and truncate the temp file
	rewind(fp);
	if (ftruncate(fd, (off_t)0) < 0)
	{
	    perror(tmp_filename);
	    exit(1);
	}

	// Run the report, capturing the output in the temp file
	(*rep->func)(fp);
	fflush(fp);

	// Read the report output into buffer
	if ((len = fd_length(fd)) < 0)
	{
	    perror(tmp_filename);
	    exit(1);
	}
	buffer.truncate_to(len);
	rewind(fp);
	int r = fread((char *)buffer.data(), 1, len, fp);
	if (r != len)
	{
	    perror(tmp_filename);
	    fprintf(stderr, "len=%d r=%d\n", len, r);
	    exit(1);
	}

	// PHP-serialise the report data
	ser.stringl(buffer.data(), buffer.length());
	
	// Save the report data
	key = g_strdup_printf("R%u", n);
	if (ret = db->put(db, 0, dbt(key), dbt(ser), 0))
	{
	    db->err(db, ret, "save_reports");
	    exit(1);
	}
    }

    fclose(fp);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
create_source_symlinks(const char *tempdir)
{
    list_iterator_t<cov_file_t> iter;

    for (iter = cov_file_t::first() ; iter != (cov_file_t *)0 ; ++iter)
    {
	cov_file_t *f = *iter;
	string_var link = g_strconcat(tempdir, "/", f->minimal_name(), (char *)0);
	string_var tempmindir = file_dirname(link);
	file_build_tree(tempmindir, 0755);
	dprintf2(D_WEB, "symlink %s -> %s\n", link.data(), f->name());
	symlink(f->name(), link);
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
/*
 * With the old GTK, we're forced to parse our own arguments the way
 * the library wants, with popt and in such a way that we can't use the
 * return from poptGetNextOpt() to implement multiple-valued options
 * (e.g. -o dir1 -o dir2).  This limits our ability to parse arguments
 * for both old and new GTK builds.  Worse, gtk2 doesn't depend on popt
 * at all, so some systems will have gtk2 and not popt, so we have to
 * parse arguments in a way which avoids potentially buggy duplicate
 * specification of options, i.e. we simulate popt in fakepopt.c!
 */
static poptContext popt_context;
static struct poptOption popt_options[] =
{
    {
    	"recursive",	    	    	    	/* longname */
	'r',  	    	    	    	    	/* shortname */
	POPT_ARG_NONE,  	    	    	/* argInfo */
	&recursive,     	    	    	/* arg */
	0,  	    	    	    	    	/* val 0=don't return */
	"recursively scan directories for source", /* descrip */
	0	    	    	    	    	/* argDescrip */
    },
    {
    	"suppress-ifdef",	    	    	/* longname */
	'X',  	    	    	    	    	/* shortname */
	POPT_ARG_STRING,  	    	    	/* argInfo */
	&suppressed_ifdefs,     	    	/* arg */
	0,  	    	    	    	    	/* val 0=don't return */
	"suppress source which is conditional on the "
	"given cpp define/s (comma-separated)", /* descrip */
	0	    	    	    	    	/* argDescrip */
    },
    {
    	"object-directory",    	    	    	/* longname */
	'o',  	    	    	    	    	/* shortname */
	POPT_ARG_STRING,  	    	    	/* argInfo */
	&object_dir,     	    	    	/* arg */
	0,  	    	    	    	    	/* val 0=don't return */
	"directory in which to find .o,.bb,.bbg,.da files", /* descrip */
	0	    	    	    	    	/* argDescrip */
    },
    {
    	"output-file",    	    	    	/* longname */
	'f',  	    	    	    	    	/* shortname */
	POPT_ARG_STRING,  	    	    	/* argInfo */
	&output_tarball,     	    	    	/* arg */
	0,  	    	    	    	    	/* val 0=don't return */
	"name of the output (in .tgz format), or - for stdout", /* descrip */
	0	    	    	    	    	/* argDescrip */
    },
    {
    	"debug",	    	    	    	/* longname */
	'D',  	    	    	    	    	/* shortname */
	POPT_ARG_STRING,  	    	    	/* argInfo */
	&debug_str,     	    	    	/* arg */
	0,  	    	    	    	    	/* val 0=don't return */
	"enable tggcov debugging features",  	/* descrip */
	0	    	    	    	    	/* argDescrip */
    },
    POPT_AUTOHELP
    { 0, 0, 0, 0, 0, 0, 0 }
};

static void
parse_args(int argc, char **argv)
{
    const char *file;
    
    argv0 = argv[0];
    
    popt_context = poptGetContext(PACKAGE, argc, (const char**)argv,
    	    	    	    	  popt_options, 0);
    poptSetOtherOptionHelp(popt_context,
    	    	           "[OPTIONS] [executable|source|directory]...");

    int rc;
    while ((rc = poptGetNextOpt(popt_context)) > 0)
    	;
    if (rc < -1)
    {
    	fprintf(stderr, "%s:%s at or near %s\n",
	    argv[0],
	    poptStrerror(rc),
	    poptBadOption(popt_context, POPT_BADOPTION_NOALIAS));
    	exit(1);
    }
    
    while ((file = poptGetArg(popt_context)) != 0)
	files = g_list_append(files, (gpointer)file);
	
    poptFreeContext(popt_context);
    
    if (debug_str != 0)
    	debug_set(debug_str);

    if (debug_enabled(D_DUMP|D_VERBOSE))
    {
    	GList *iter;
	string_var token_str = debug_enabled_tokens();

	duprintf1("parse_args: recursive=%d\n", recursive);
	duprintf1("parse_args: suppressed_ifdefs=%s\n", suppressed_ifdefs);
	duprintf2("parse_args: debug = 0x%lx (%s)\n", debug, token_str.data());

	duprintf0("parse_args: files = ");
	for (iter = files ; iter != 0 ; iter = iter->next)
	    duprintf1(" \"%s\"", (char *)iter->data);
	duprintf0(" }\n");
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

#define DEBUG_GLIB 1
#if DEBUG_GLIB

static const char *
log_level_to_str(GLogLevelFlags level)
{
    static char buf[32];

    switch (level & G_LOG_LEVEL_MASK)
    {
    case G_LOG_LEVEL_ERROR: return "ERROR";
    case G_LOG_LEVEL_CRITICAL: return "CRITICAL";
    case G_LOG_LEVEL_WARNING: return "WARNING";
    case G_LOG_LEVEL_MESSAGE: return "MESSAGE";
    case G_LOG_LEVEL_INFO: return "INFO";
    case G_LOG_LEVEL_DEBUG: return "DEBUG";
    default:
    	snprintf(buf, sizeof(buf), "%d", level);
	return buf;
    }
}

void
log_func(
    const char *domain,
    GLogLevelFlags level,
    const char *msg,
    gpointer user_data)
{
    fprintf(stderr, "%s:%s:%s\n",
    	(domain == 0 ? PACKAGE : domain),
	log_level_to_str(level),
	msg);
    if (level & G_LOG_FLAG_FATAL)
    	exit(1);
}

#endif
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

int
main(int argc, char **argv)
{
    char *tempdir;
    char *webdb_file;
    char *output_tarball_abs;
    DB *db;
    int ret;

#if DEBUG_GLIB
    g_log_set_handler("GLib",
    	    	      (GLogLevelFlags)(G_LOG_LEVEL_MASK|G_LOG_FLAG_FATAL),
    	    	      log_func, /*user_data*/0);
#endif

    parse_args(argc, argv);
    read_gcov_files();

    tempdir = g_strdup_printf("%s/ggcov-web-%d.d", get_tmpdir(), (int)getpid());
    webdb_file = g_strconcat(tempdir, "/", "ggcov.webdb", (char*)0);
    output_tarball_abs = (!strcmp(output_tarball, "-") ? (char *)"-" :
			    file_make_absolute(output_tarball));

    systemf("/bin/rm -rf \"%s\"", tempdir);
    file_build_tree(tempdir, 0755);

    if (ret = db_create(&db, 0, 0))
    {
	fprintf(stderr, "%s: db_create(): %s\n", argv0, db_strerror(ret));
	exit(1);
    }

    ret = db->open(db, 0, webdb_file, 0, DB_HASH, DB_CREATE, 0644);
    if (ret)
    {
	db->err(db, ret, "%s", webdb_file);
	exit(1);
    }

    build_filename_index();
    save_filename_index(db);
    save_lines(db);
    build_global_function_index();
    save_global_function_index(db);
    save_global_function_list(db);
    save_file_function_indexes(db);
    save_functions(db);
    save_summaries(db);
    build_callnode_index();
    save_callnode_index(db);
    save_callgraph(db);
    save_report_index(db);
    save_reports(db);

    db->close(db, 0);

    create_source_symlinks(tempdir);

    systemf("cd \"%s\" ; tar -cvhzf \"%s\" *", tempdir, output_tarball_abs);
    systemf("/bin/rm -rf \"%s\"", tempdir);

    return 0;
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
/*END*/