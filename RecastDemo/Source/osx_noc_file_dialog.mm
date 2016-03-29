/* noc_file_dialog library
 *
 * Copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// Safeguard in case this gets compiled on non-apple OSes.
#ifndef __APPLE__
#	define NOC_FILE_DIALOG_IMPLEMENTATION
#endif

#ifndef NOC_FILE_DIALOG_IMPLEMENTATION
#define NOC_FILE_DIALOG_IMPLEMENTATION

#include "noc_file_dialog.h"

// assuming NOC_FILE_DIALOG_OSX defined

#include <stdlib.h>
#include <string.h>

//static char *g_noc_file_dialog_ret = NULL;

#include <AppKit/AppKit.h>

const char *noc_file_dialog_open(int flags,
								 const char *filters,
								 const char *default_path,
								 const char *default_name)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSURL *url;
	const char *utf8_path;
	NSSavePanel *panel;
	NSOpenPanel *open_panel;
	NSMutableArray *types_array;
	NSURL *default_url;
	
	if (flags & NOC_FILE_DIALOG_OPEN) {
		panel = open_panel = [NSOpenPanel openPanel];
	} else {
		panel = [NSSavePanel savePanel];
	}
	
	if (flags & NOC_FILE_DIALOG_DIR) {
		[open_panel setCanChooseDirectories:YES];
		[open_panel setCanChooseFiles:NO];
	}
	
	if (default_path) {
		default_url = [NSURL fileURLWithPath:
					   [NSString stringWithUTF8String:default_path]];
		[panel setDirectoryURL:default_url];
		[panel setNameFieldStringValue:default_url.lastPathComponent];
	}
	
	if (filters) {
		types_array = [NSMutableArray array];
		while (*filters) {
			filters += strlen(filters) + 1; // skip the name
			assert(strncmp(filters, "*.", 2) == 0);
			filters += 2; // Skip the "*."
			[types_array addObject:[NSString stringWithUTF8String: filters]];
			filters += strlen(filters) + 1;
		}
		[panel setAllowedFileTypes:types_array];
	}
	
	free(g_noc_file_dialog_ret);
	g_noc_file_dialog_ret = NULL;
	if ( [panel runModal] == NSModalResponseOK ) {
		url = [panel URL];
		utf8_path = [[url path] UTF8String];
		g_noc_file_dialog_ret = strdup(utf8_path);
	}
	
	[pool drain];
	[pool release];
	return g_noc_file_dialog_ret;
}

#endif // NOC_FILE_DIALOG_IMPLEMENTATION
