/*
 *  Game Categories Lite 1.2
 *
 *  Copyright (C) 2011  Codestation
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <string.h>
#include <stdio.h>
#include "psppaf.h"

#ifdef DEBUG

#define LOGFILE "ms0:/category_lite.log"

extern char _buffer_log[256];

int kwrite(const char *path, void *buffer, int buflen);

#define kprintf(format, ...) do { \
    sce_paf_private_snprintf(_buffer_log, sizeof(_buffer_log), format, ## __VA_ARGS__); \
    kwrite(LOGFILE, _buffer_log, sce_paf_private_strlen(_buffer_log)); \
} while(0)

#else

#define kprintf(format, ...)
// uncomment this to use the logger with psplink
//#define kprintf(format, ...) printf(format, ## __VA_ARGS__)

#endif

int kwrite(const char *path, void *buffer, int buflen);

#endif /* LOGGER_H_ */
