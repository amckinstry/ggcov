#
# ggcov - A GTK frontend for exploring gcov coverage data
# Copyright (c) 2001-2003 Greg Banks <gnb@users.sourceforge.net>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 
# $Id: Makefile.am,v 1.9 2010-05-09 05:37:14 gnb Exp $
#

SUBDIRS=	src ui php doc test scripts lib

EXTRA_DIST=	.gitignore .gdbinit autogen.sh

ggcov:
	cd src ; $(MAKE) ggcov

@REDHAT_TRUE@package: dist
@REDHAT_TRUE@	$(RM) -rf rpm.d
@REDHAT_TRUE@	mkdir rpm.d rpm.d/SOURCES rpm.d/BUILD rpm.d/SRPMS rpm.d/RPMS
@REDHAT_TRUE@	ln -s ../../$(distdir).tar.gz rpm.d/SOURCES
@REDHAT_TRUE@	rpmbuild -ba --define="_topdir $(PWD)/rpm.d" ggcov.spec
@REDHAT_TRUE@
@REDHAT_TRUE@CLEANFILES= rpm.d

COVFLAGS=		-fprofile-arcs -ftest-coverage
coverage:
	cd src ; $(MAKE) \
	    CFLAGS="$(CFLAGS) $(COVFLAGS)" \
	    CXXFLAGS="$(CXXFLAGS) $(COVFLAGS)" \
	    all

profile:
	cd src ; $(MAKE) \
	    CFLAGS="`echo ' $(CFLAGS) ' | sed -e 's| -O[1-9] | |g'` -pg" \
	    CXXFLAGS="`echo ' $(CXXFLAGS) ' | sed -e 's| -O[1-9] | |g'` -pg" \
	    all
