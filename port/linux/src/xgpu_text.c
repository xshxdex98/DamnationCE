/*
XGPU_TEXT.C

A growable string for the shader translators.
*/

#include "xgpu.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void xgpu_text_append(struct xgpu_text *text, const char *format, ...)
{
	va_list arguments;
	int needed;

	for (;;)
	{
		unsigned long capacity;
		char *grown;

		va_start(arguments, format);
		needed = vsnprintf(text->buffer ? text->buffer + text->length : NULL,
			text->buffer ? text->capacity - text->length : 0, format, arguments);
		va_end(arguments);
		if (needed < 0)
			return;
		if (text->buffer && text->length + (unsigned long)needed < text->capacity)
		{
			text->length += (unsigned long)needed;
			return;
		}
		/* (out of memory: the text stays as it was) */
		capacity = (text->capacity + (unsigned long)needed + 1) * 2;
		grown = realloc(text->buffer, capacity);
		if (!grown)
			return;
		text->buffer = grown;
		text->capacity = capacity;
	}
}
