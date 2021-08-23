#
#	This file is part of cxmb
#	Copyright (C) 2008  Poison
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#	File:		Makefile
#	Author:		Poison <hbpoison@gmail.com>
#	Date Created:	2008-07-01
all:	test
#debug

full:
	make clean
	make -f Makefile.PSP

clean:
	make -f Makefile.PSP clean

lite:
	make clean
	CXMB_LITE=1 make -f Makefile.PSP

debug:
	make clean
	DEBUG=1 make -f Makefile.PSP

debuglog:
	make clean
	DEBUG=2 make -f Makefile.PSP
	
psp:
	make clean
	DEBUG=1 make -f Makefile.PSP
	if [ -e /cygdrive/p/seplugins ]; then cp -a pergame.prx /cygdrive/p/seplugins/pergame.prx; else cp -a pergame.prx /cygdrive/f/seplugins/pergame.prx; fi

test:
	make clean
	DEBUG=1 make -f Makefile.PSP

dist:
	make clean
	make -f Makefile.PSP
	cp -a pergame.prx ../seplugins/
	make clean
	cd ../ && zip -r pergame-svn.zip seplugins src/Makefile* src/exports.exp src/*.c src/*.h README.txt --exclude "*/.svn/*"

