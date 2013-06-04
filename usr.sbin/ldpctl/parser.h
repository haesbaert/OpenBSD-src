/*	$OpenBSD: parser.h,v 1.6 2013/06/04 02:40:17 claudio Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _PARSER_H_
#define _PARSER_H_

#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>

enum actions {
	NONE,
	FIB,
	FIB_COUPLE,
	FIB_DECOUPLE,
	LOG_VERBOSE,
	LOG_BRIEF,
	SHOW,
	SHOW_DISC,
	SHOW_IFACE,
	SHOW_NBR,
	SHOW_LIB,
	SHOW_FIB,
	SHOW_FIB_IFACE,
	RELOAD
};

struct parse_result {
	struct in_addr	addr;
	char		ifname[IF_NAMESIZE];
	int		flags;
	enum actions	action;
	u_int8_t	prefixlen;
};

struct parse_result	*parse(int, char *[]);
int			 parse_addr(const char *, struct in_addr *);
int			 parse_prefix(const char *, struct in_addr *,
			     u_int8_t *);

#endif	/* _PARSER_H_ */
