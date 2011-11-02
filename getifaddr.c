/* $Id$ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#if defined(sun)
#include <sys/sockio.h>
#endif

#include "config.h"
#if HAVE_GETIFADDRS
#include <ifaddrs.h>
#ifdef __linux__
#ifndef AF_LINK
#define AF_LINK AF_INET
#endif
#else
#include <net/if_dl.h>
#endif
#endif
#include "getifaddr.h"
#include "log.h"

static uint32_t
get_netmask(struct sockaddr_in *netmask)
{
	uint32_t mask;
	int i;

	if (!netmask)
		return 0;
	mask = ntohl(netmask->sin_addr.s_addr);
	for (i = 0; i < 32; i++)
	{
		if ((mask >> i) & 1)
			break;
	}
	mask = 32 - i;

	return mask;
}

int
getifaddr(const char * ifname, char * buf, int len)
{
	/* SIOCGIFADDR struct ifreq *  */
	uint32_t mask = 0;
	int i;
#if HAVE_GETIFADDRS
	struct ifaddrs *ifap, *p;
	struct sockaddr_in *addr_in;

	if( getifaddrs(&ifap) != 0 )
	{
		DPRINTF(E_ERROR, L_GENERAL, "getifaddrs(): %s\n", strerror(errno));
		return -1;
	}
	for( p = ifap; p != NULL; p = p->ifa_next )
	{
		if( p->ifa_addr && p->ifa_addr->sa_family == AF_INET )
		{
			if( strcmp(p->ifa_name, ifname) != 0 )
				continue;
			addr_in = (struct sockaddr_in *)p->ifa_addr;
			if( !inet_ntop(AF_INET, &addr_in->sin_addr, buf, len) )
			{
				DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
				break;
			}
			addr_in = (struct sockaddr_in *)p->ifa_netmask;
			mask = get_netmask(addr_in);
			break;
		}
	}
	freeifaddrs(ifap);
	if( !p )
	{
		DPRINTF(E_ERROR, L_GENERAL, "Network interface %s not found\n", ifname);
		return -1;
	}
#else
	int s;
	struct ifreq ifr;
	int ifrlen;
	struct sockaddr_in * addr;

	ifrlen = sizeof(ifr);
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "socket(PF_INET, SOCK_DGRAM): %s\n", strerror(errno));
		return -1;
	}
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if(ioctl(s, SIOCGIFADDR, &ifr, &ifrlen) < 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "ioctl(s, SIOCGIFADDR, ...): %s\n", strerror(errno));
		close(s);
		return -1;
	}
	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	if(!inet_ntop(AF_INET, &addr->sin_addr, buf, len))
	{
		DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
		close(s);
		return -1;
	}
	if(ioctl(s, SIOCGIFNETMASK, &ifr, &ifrlen) == 0)
	{
		addr = (struct sockaddr_in *)&ifr.ifr_netmask;
		mask = get_netmask(addr);
	}
	else
		DPRINTF(E_ERROR, L_GENERAL, "ioctl(s, SIOCGIFNETMASK, ...): %s\n", strerror(errno));
	close(s);
#endif
	if (mask)
	{
		i = strlen(buf);
		snprintf(buf+i, len-i, "/%u", mask);
	}
	return 0;
}

int
getsysaddr(char * buf, int len)
{
	int i;
	uint32_t mask = 0;
	int ret = -1;
#if HAVE_GETIFADDRS
	struct ifaddrs *ifap, *p;
	struct sockaddr_in *addr_in;
	uint8_t a;

	if( getifaddrs(&ifap) != 0 )
	{
		DPRINTF(E_ERROR, L_GENERAL, "getifaddrs(): %s\n", strerror(errno));
		return -1;
	}
	for( p = ifap; p != NULL; p = p->ifa_next )
	{
		if( p->ifa_addr && p->ifa_addr->sa_family == AF_INET )
		{
			addr_in = (struct sockaddr_in *)p->ifa_addr;
			a = (htonl(addr_in->sin_addr.s_addr) >> 0x18) & 0xFF;
			if( a == 127 )
				continue;
			if( !inet_ntop(AF_INET, &addr_in->sin_addr, buf, len) )
			{
				DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
				break;
			}
			addr_in = (struct sockaddr_in *)p->ifa_netmask;
			mask = get_netmask(addr_in);
			ret = 0;
			break;
		}
	}
	freeifaddrs(ifap);
#else
	int s = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	struct ifreq ifr;
	for (i=1; i > 0; i++)
	{
		ifr.ifr_ifindex = i;
		if( ioctl(s, SIOCGIFNAME, &ifr) < 0 )
			break;
		if(ioctl(s, SIOCGIFADDR, &ifr, sizeof(struct ifreq)) < 0)
			continue;
		memcpy(&addr, &ifr.ifr_addr, sizeof(addr));
		if(strncmp(inet_ntoa(addr.sin_addr), "127.", 4) == 0)
			continue;
		if(ioctl(s, SIOCGIFNETMASK, &ifr, sizeof(struct ifreq)) < 0)
			continue;
		if(!inet_ntop(AF_INET, &addr.sin_addr, buf, len))
		{
			DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
			close(s);
			break;
		}
		memcpy(&addr, &ifr.ifr_netmask, sizeof(addr));
		mask = get_netmask(&addr);
		ret = 0;
		break;
	}
	close(s);
#endif

	if (mask)
	{
		i = strlen(buf);
		snprintf(buf+i, len-i, "/%u", mask);
	}

	return(ret);
}

int
getsyshwaddr(char * buf, int len)
{
	unsigned char mac[6];
	int ret = -1;
#if HAVE_GETIFADDRS
	struct ifaddrs *ifap, *p;
	struct sockaddr_in *addr_in;
	uint8_t a;

	if( getifaddrs(&ifap) != 0 )
	{
		DPRINTF(E_ERROR, L_GENERAL, "getifaddrs(): %s\n", strerror(errno));
		return -1;
	}
	for( p = ifap; p != NULL; p = p->ifa_next )
	{
		if( p->ifa_addr && p->ifa_addr->sa_family == AF_LINK )
		{
			addr_in = (struct sockaddr_in *)p->ifa_addr;
			a = (htonl(addr_in->sin_addr.s_addr) >> 0x18) & 0xFF;
			if( a == 127 )
				continue;
#ifdef __linux__
			struct ifreq ifr;
			int fd;
			fd = socket(AF_INET, SOCK_DGRAM, 0);
			if( fd < 0 )
				continue;
			strncpy(ifr.ifr_name, p->ifa_name, IFNAMSIZ);
			if( ioctl(fd, SIOCGIFHWADDR, &ifr) < 0 )
			{
				close(fd);
				continue;
			}
			memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
#else
			struct sockaddr_dl *sdl;
			sdl = (struct sockaddr_dl*)p->ifa_addr;
			memcpy(mac, LLADDR(sdl), sdl->sdl_alen);
#endif
			if( MACADDR_IS_ZERO(mac) )
				continue;
			ret = 0;
			break;
		}
	}
	freeifaddrs(ifap);
#else
	struct if_nameindex *ifaces, *if_idx;
	struct ifreq ifr;
	int fd;

	memset(&mac, '\0', sizeof(mac));
	/* Get the spatially unique node identifier */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if( fd < 0 )
		return(ret);

	ifaces = if_nameindex();
	if(!ifaces)
		return(ret);

	for(if_idx = ifaces; if_idx->if_index; if_idx++)
	{
		strncpy(ifr.ifr_name, if_idx->if_name, IFNAMSIZ);
		if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
			continue;
		if(ifr.ifr_ifru.ifru_flags & IFF_LOOPBACK)
			continue;
		if( ioctl(fd, SIOCGIFHWADDR, &ifr) < 0 )
			continue;
		if( MACADDR_IS_ZERO(ifr.ifr_hwaddr.sa_data) )
			continue;
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
		ret = 0;
		break;
	}
	if_freenameindex(ifaces);
	close(fd);
#endif
	if(ret == 0)
	{
		if(len > 12)
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
			        mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF,
			        mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
		else if(len == 6)
			memmove(buf, mac, 6);
	}
	return ret;
}

int
get_remote_mac(struct in_addr ip_addr, unsigned char * mac)
{
	struct in_addr arp_ent;
	FILE * arp;
	char remote_ip[16];
	int matches, hwtype, flags;
	memset(mac, 0xFF, 6);

 	arp = fopen("/proc/net/arp", "r");
	if( !arp )
		return 1;
	while( !feof(arp) )
	{
	        matches = fscanf(arp, "%15s 0x%8X 0x%8X %2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
		                      remote_ip, &hwtype, &flags,
		                      &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
		if( matches != 9 )
			continue;
		inet_pton(AF_INET, remote_ip, &arp_ent);
		if( ip_addr.s_addr == arp_ent.s_addr )
			break;
		mac[0] = 0xFF;
	}
	fclose(arp);

	if( mac[0] == 0xFF )
	{
		memset(mac, 0xFF, 6);
		return 1;
	}

	return 0;
}
