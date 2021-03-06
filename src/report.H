/*
 * ggcov - A GTK frontend for exploring gcov coverage data
 * Copyright (c) 2004 Greg Banks <gnb@users.sourceforge.net>
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

#ifndef _ggcov_report_H_
#define _ggcov_report_H_ 1

typedef struct
{
    const char *name;       /* name for commandline option */
    const char *label;      /* label for GUI menu */
    int (*func)(FILE *,     /* function to perform the report, returns #lines */
		const char *filename/*NULL for tmpfile */);
    const char *filename;   /* default filename for Save As dialog */
}
report_t;

extern const report_t all_reports[];    /* ends with .name=0 */

#endif /* _ggcov_report_H_ */
