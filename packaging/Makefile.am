## Process this file with automake to produce Makefile.in

# Copyright (C) 2013 Andrew Makousky
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#   * Redistributions in binary form must reproduce the above
#     copyright notice, this list of conditions and the following
#     disclaimer in the documentation and/or other materials provided
#     with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

EXTRA_DIST = 2.12.10-dist.txt \
	sndstud.nsi english.nsh spanish.nsh license.rtf \
	bannrbmp.bmp features.txt uuids.txt Property.idt.in \
	Directory.idt.cat Icon.idt RemoveFile.idt.in Shortcut.idt.in

if WITH_WIN32

if MAINTAINER_MODE
noinst_DATA = sndstud-$(VERSION)-setup.exe
endif

MOSTLYCLEANFILES = \
	ls-r.txt ls-r2.txt File.idt Directory.idt Component.idt \
	Feature.idt FeatureComponents.idt Media.idt Directory.idt.old \
	RemoveFile.idt Shortcut.idt Component.idt.old archive.cab

CLEANFILES = sndstud-$(VERSION)-setup.exe sndstud-$(VERSION).msi

nsis: sndstud-$(VERSION)-setup.exe
msi: sndstud-$(VERSION).msi

2.12.10-dist: gtk+-bundle-2.12.10.zip
	mkdir $@
	-cd $@ && unzip -q ../gtk+-bundle-2.12.10.zip \
	  `cat ../2.12.10-dist.txt`
	echo 'gtk-theme-name = "MS-Windows"' > \
	  2.12.10-dist/etc/gtk-2.0/gtkrc

instdir:
	tmpinst=`pwd`/instdir && \
	  cd $(top_builddir) && $(MAKE) install prefix=$$tmpinst
	if [ -d instdir/lib ]; then \
	  mv instdir/lib/locale instdir/share; \
	  rmdir instdir/lib; \
	fi

sndstud-$(VERSION)-setup.exe: sndstud.nsi english.nsh spanish.nsh \
  license.rtf instdir 2.12.10-dist
	if [ -n "$(TEST_INST)" ]; then \
	  makensis -DVERSION=$(VERSION) -DTEST_INST sndstud.nsi; \
	else \
	  makensis -DVERSION=$(VERSION) sndstud.nsi; \
	fi

ls-r.txt: 2.12.10-dist
	ls -RF $< | \
	  sed -e '/^.*\/$$/d' -e 's/\(^.*\)\*/\1/g' > $@

ls-r2.txt: instdir
	ls -RF $< | \
	  sed -e '/^.*\/$$/d' -e 's/\(^.*\)\*/\1/g' > $@

uuids.txt:
	uuidgen -c -n1000 > uuids.txt

File.idt Directory.idt Component.idt Feature.idt \
FeatureComponents.idt Media.idt: features.txt uuids.txt ls-r.txt ls-r2.txt
	msi-tool -r -d"sndstud|Sound Studio" ls-r.txt ls-r2.txt

archive.cab: File.idt
	if [ -n "$(TEST_INST)" ]; then \
	  cd 2.12.10-dist && cabarc n archive.cab @cablist.txt; \
	else \
	  cd 2.12.10-dist && cabarc -m LZX:21 n archive.cab @cablist.txt; \
	fi
	mv 2.12.10-dist/archive.cab .
	rm -rf 2.12.10-dist instdir

sndstud-$(VERSION).msi: Property.idt File.idt Directory.idt Component.idt \
  Feature.idt FeatureComponents.idt Media.idt archive.cab Schema.msi
# Prepare tables that the installer will use to create shortcuts.
	mv Directory.idt Directory.idt.old
	cat Directory.idt.old Directory.idt.cat > Directory.idt
	mkdir Icon
	cp ../svgs/sndstud.ico Icon/sndstud_icon.exe.ibd
# Note: The component manipulation code is an inelegant solution to
# get the shortcut to the user manual to work.
	mv Component.idt Component.idt.old
	SSCOMP=`grep -F 'sndstud.exe' File.idt | cut -f2`; \
	  HELPNAME=`grep -F 'help.txt' File.idt | cut -f1`; \
	  HELPCOMP=`grep -F 'help.txt' File.idt | cut -f2`; \
	  HCLINENUM=`grep -n -F $$HELPCOMP Component.idt.old | cut -d: -f1`; \
	  HCOLD=`sed -ne $${HCLINENUM}p Component.idt.old`; \
	  sed -e $${HCLINENUM}d Component.idt.old > Component.idt; \
	  echo "`echo \"$$HCOLD\" | cut -f1-5`"$$'\t'$$HELPNAME >> \
	    Component.idt; \
	  sed -e s/SndStudComp/$${SSCOMP}/g -e s/SndStudFeat/ft1/g \
	    -e s/HelpComp/$${HELPCOMP}/g \
	    Shortcut.idt.in > Shortcut.idt; \
	  sed -e s/SndStudComp/$${SSCOMP}/g RemoveFile.idt.in > \
	    RemoveFile.idt
# Build the installer.
	cp Schema.msi $@
	msiinfo $@ -t "Installation Database" -j "Sound Studio" \
	  -a "Andrew Makousky" -k "Installer, MSI, Database" \
	  -o "This installer database contains the logic and data required to install Sound Studio." \
	  -p "Intel;1033" -v {`uuidgen -c`} -g 110 -w 2 -u 0
	msidb -d$@ -f`pwd | sed -e 's.^\(\|/cygdrive\)/\([[:alpha:]]\)/.\2:/.g' -e 's./.\\\\.g'` \
	  -i Property.idt Component.idt Directory.idt \
	  Feature.idt FeatureComponents.idt File.idt Media.idt \
	  Icon.idt Shortcut.idt RemoveFile.idt
	msidb -d$@ -aarchive.cab
	! msival2 $@ "C:\\Program Files\\MsiVal2\\darice.cub" | grep -e 'ERROR'

mostlyclean: mostlyclean-am
	rm -rf instdir 2.12.10-dist Icon

clean: clean-am mostlyclean

endif
