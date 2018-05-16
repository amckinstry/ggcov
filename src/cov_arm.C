/*
 * ggcov - A GTK frontend for exploring gcov coverage data
 * Copyright (c) 2017 Alastair McKinstry <mckinstry@debian.org>
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

#include "cov_specific.H"
#include "cov_suppression.H"
#include "string_var.H"

#if defined(HAVE_LIBBFD) && defined(COV_ARM64)

/*
 * Machine-specific code to scan arm64  object code for function calls.
 */

class cov_arm64_call_scanner_t : public cov_call_scanner_t
{
public:
  cov_arm64_call_scanner_t();
  ~cov_arm64_call_scanner_t();

  gboolean attach(cov_bfd_t *);
  boolean is_function_reloc(const arelent *) const;
};

COV_FACTORY_STATIC_REGISTER(cov_call_scanner_t,
			    cov_arm64_call_scanner_t);

cov_amd64_call_scanner_t::cov_arm64_call_scanner_t()
{
}

cov_amd64_call_scanner_t::~cov_arm64_call_scanner_t()
{
}

gboolean
cov_arm64_call_scanner_t::attach(cov_bfd_t *b)
{
    if (b->architecture() != bfd_arch_aarch64)
	return FALSE;
    if (b->mach() != bfd_mach_aarch64)
	return FALSE;
    return cov_call_scanner_t::attach(b);
}

#endif

