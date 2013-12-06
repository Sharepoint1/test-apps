/*
 * common.h
 *
 * Author: Johan Hovold <jhovold@gmail.com>
 */

#ifndef COMMON_H
#define COMMON_H

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>


#define SCREEN_WIDTH	800
#define SCREEN_HEIGHT	480


#define err(fmt, ...) \
	fprintf(stderr, fmt, ##__VA_ARGS__)

#define err_errno(fmt, ...) \
	err("%s: " fmt ": %s (%d)\n", __func__, ##__VA_ARGS__, \
						strerror(errno), errno)

#define die(fmt, ...) \
	do { \
		err(fmt, ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

#define die_errno(fmt, ...) \
	do { \
		err_errno(fmt, ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

#endif
