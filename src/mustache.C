/*
 * ggcov - A GTK frontend for exploring gcov coverage data
 * Copyright (c) 2001-2015 Greg Banks <gnb@users.sourceforge.net>
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

#include "mustache.H"
#include <fstream>
#include <ext/stdio_filebuf.h>
#include "logging.H"

static logging::logger_t &_log = logging::find_logger("ggcov-html");

namespace mustache
{

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

environment_t::environment_t() {}

environment_t::~environment_t() {}

template_t *environment_t::make_template(const char *name, const char *output_name)
{
    if (output_name == 0)
	output_name = name;
    return new template_t(*this, name, output_name);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

template_t::template_t(environment_t &env, const char *name, const char *output_name)
 :  env_(env),
    stdin_fp_(0),
    stdin_sb_(0),
    stdin_stream_(0),
    yaml_(0)
{
    if (name)
	set_template_name(name);
    if (output_name)
	set_output_name(output_name);
}

template_t::~template_t()
{
    cleanup();
}

yaml_generator_t &template_t::begin_render()
{
    char cmd[1024];
    _log.info("Expanding template %s to %s\n",
	      template_path_.data(), output_path_.data());
    // TODO: generate a temporary file for output_path_ if its null
    snprintf(cmd, sizeof(cmd), "mustache - \"%s\" > \"%s\"",
	     template_path_.data(), output_path_.data());
    stdin_fp_ = popen(cmd, "w");
    stdin_sb_ = new __gnu_cxx::stdio_filebuf<char>(fileno(stdin_fp_), std::ios::out);
    stdin_stream_ = new std::ostream(stdin_sb_);
    yaml_ = new yaml_generator_t(*stdin_stream_);
    return *yaml_;
}

void template_t::cleanup()
{
    if (stdin_stream_)
	stdin_stream_->flush();
    if (stdin_fp_)
    {
	pclose(stdin_fp_);
	stdin_fp_ = 0;
    }
    if (stdin_stream_)
    {
	delete stdin_stream_;
	stdin_stream_ = 0;
    }
    if (stdin_sb_)
    {
	delete stdin_sb_;
	stdin_sb_ = 0;
    }
    if (yaml_)
    {
	delete yaml_;
	yaml_ = 0;
    }
}

void template_t::end_render()
{
    cleanup();
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
// end the namespace
}
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
/*END*/
