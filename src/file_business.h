/* Circumvent problems with different Microsoft runtime versions and
   libintl *printf overrides.

This file is in Public Domain.  */

/**
 * @file
 * Circumvent problems with different Microsoft runtime versions and
 * libintl *printf overrides.
 */

#ifndef FILE_BUSINESS_H
#define FILE_BUSINESS_H

void do_save_printing (FILE* fp);
void do_export_printing (FILE* fp);

#endif /* FILE_BUSINESS_H */
