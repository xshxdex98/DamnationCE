/*
TAG_FILES.H
*/

#ifndef __TAG_FILES_H
#define __TAG_FILES_H
#pragma once

/* ---------- constants */

enum
{
	TAG_STRING_LENGTH = 31,
};

/* ---------- prototypes/TAG_FILES.C */

void tag_files_open(void);
void tag_files_close(void);

const char *tag_name_strip_path(char const *name);

char *tag_get_name(long tag_index);

#endif // __TAG_FILES_H
