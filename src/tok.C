/*
 * ggcov - A GTK frontend for exploring gcov coverage data
 * Copyright (c) 2002-2003 Greg Banks <gnb@users.sourceforge.net>
 * copied from
 * CANT - A C implementation of the Apache/Tomcat ANT build system
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

#include "tok.H"

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

void
tok_t::init(char *str, const char *sep)
{
    first_ = TRUE;
    buf_ = str;
    state_ = 0;
    sep_ = (sep == 0 ? " \t\r\n" : sep);
}

tok_t::tok_t(const char *str, const char *sep)
{
    init((str == 0 ? 0 : g_strdup(str)), sep);
}

tok_t::tok_t(char *str, const char *sep)
{
    init(str, sep);
}

tok_t::~tok_t()
{
    if (buf_)
	g_free(buf_);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

const char *
tok_t::next()
{
    if (!buf_)
	return 0;

    if (first_)
    {
	first_ = FALSE;
	return strtok_r(buf_, sep_, &state_);
    }

    return strtok_r(0, sep_, &state_);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
/*END*/
