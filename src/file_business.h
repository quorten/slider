/* Circumvent problems with different Microsoft runtime versions and
   libintl *printf overrides.

Copyright (C) 2011, 2012, 2013 Andrew Makousky
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.  */

/**
 * @file
 * Circumvent problems with different Microsoft runtime versions and
 * libintl *printf overrides.
 *
 * When <libintl.h> is included, *printf functions are overrided to
 * use *printf functions in libintl that support certain POSIX
 * features before calling fprintf in msvcrt.dll.  However, since only
 * some file functions from <stdio.h> were overrided but not all file
 * functions, using these routines can result in file descriptors that
 * are valid in one C runtime library to be passed to another C
 * runtime library which they are invalid in.  It is for this reason
 * that all fprintf functions involved in file saving and exporting
 * are isolated in this file that does not include <libintl.h>.
 */

#ifndef FILE_BUSINESS_H
#define FILE_BUSINESS_H

void do_save_printing (FILE * fp);
void do_export_printing (FILE * fp);

#endif /* FILE_BUSINESS_H */
