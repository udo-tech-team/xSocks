/*
 * acl.c - Manage the ACL (Access Control List)
 *
 * Copyright (C) 2013 - 2015, Max Lv <max.c.lv@gmail.com>
 *
 * This file is part of the shadowsocks-libev.
 *
 * shadowsocks-libev is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * shadowsocks-libev is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with shadowsocks-libev; see the file COPYING. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "util.h"
#include "logger.h"
#include "ipset/ipset.h"

static struct ip_set acl_ipv4_set;
static struct ip_set acl_ipv6_set;

static void parse_addr_cidr(const char *str, char *host, int *cidr)
{
    int ret = -1, n = 0;
    char *pch;

    pch = strchr(str, '/');
    while (pch != NULL) {
        n++;
        ret = pch - str;
        pch = strchr(pch + 1, '/');
    }
    if (ret == -1) {
        strcpy(host, str);
        *cidr = -1;
    } else {
        memcpy(host, str, ret);
        host[ret] = '\0';
        *cidr = atoi(str + ret + 1);
    }
}

int init_acl(const char *path)
{
    // initialize ipset
    ipset_init_library();
    ipset_init(&acl_ipv4_set);
    ipset_init(&acl_ipv6_set);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        logger_stderr("Invalid acl path.");
        return -1;
    }

    char line[256];
    while (!feof(f)) {
        if (fgets(line, 256, f)) {
            // Trim the newline
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }

            char host[256];
            int cidr;
            parse_addr_cidr(line, host, &cidr);

            struct cork_ip addr;
            int err = cork_ip_init(&addr, host);
            if (!err) {
                if (addr.version == 4) {
                    if (cidr >= 0) {
                        ipset_ipv4_add_network(&acl_ipv4_set, &(addr.ip.v4), cidr);
                    } else {
                        ipset_ipv4_add(&acl_ipv4_set, &(addr.ip.v4));
                    }
                } else if (addr.version == 6) {
                    if (cidr >= 0) {
                        ipset_ipv6_add_network(&acl_ipv6_set, &(addr.ip.v6), cidr);
                    } else {
                        ipset_ipv6_add(&acl_ipv6_set, &(addr.ip.v6));
                    }
                }
            }
        }
    }

    fclose(f);

    return 0;
}

void free_acl(void)
{
    ipset_done(&acl_ipv4_set);
    ipset_done(&acl_ipv6_set);
}

int acl_contains_ip(const char * host)
{
    struct cork_ip addr;
    int err = cork_ip_init(&addr, host);
    if (err) {
        return 0;
    }

    if (addr.version == 4) {
        return ipset_contains_ipv4(&acl_ipv4_set, &(addr.ip.v4));
    } else if (addr.version == 6) {
        return ipset_contains_ipv6(&acl_ipv6_set, &(addr.ip.v6));
    }

    return 0;
}