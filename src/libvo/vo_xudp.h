/*
 * Playback using XMOS UDP Protocol.
 * copyright (c)  2009 XMOS Ltd.
 * 
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef __VO_XUDP_H__

#ifdef _WIN32

#include <windows.h>  /* for Sleep() in dos/mingw */
#define nsleep(nanoseconds) Sleep((nanoseconds)/1000000000) /* from mingw.org list */
#include <winsock2.h>

#else

int nsleep(unsigned long nanosec)  
{  
    struct timespec req={0},rem={0};  
    time_t sec=(int)(nanosec/1000000000);  
    nanosec=nanosec-(sec*1000000000);  
    req.tv_sec=sec;  
    req.tv_nsec=nanosec;  
    nanosleep(&req,&rem);  
    return 1;  
}  

#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#endif 

#define __VO_XUDP_H__

#define PIXELS_PER_PACKET 342
#define intceil(a,b) (int) ((a+b-1)/(b))
#define XMOS_DATA               2
#define XMOS_LATCH              3
#define MIN_PACKET_SIZE        64

#define BCAST_HOST_X          255
#define BCAST_HOST_Y          255
#define DEFAULT_HOST_X        254
#define DEFAULT_HOST_Y          0

#define DEFAULT_PORT          306
#define DEFAULT_HEIGHT         32
#define DEFAULT_WIDTH          16
#define DEFAULT_PREFIX  "192.168"
#define PACKET_HEADER 16


const char magicNumber[4] = {'X', 'M', 'O', 'S' };


typedef struct {
	char *name;
	int img_format;

	int width;
	int height;
	int bpc; /* bits per component: bpc = 3, channels = 3 => bpp = 24*/

  int tilewidth;
  int tileheight;
  int linesperpacket;
  
  int simlevel;
  int noinit;
  
  char prefix[100];
  int port;
  int fd;
  struct sockaddr_in addrs[256][256];
  
  unsigned int ipaddresses[256][256];
  
  int currentchainy;
  int currentchainx;
  int currentscreeny;
  int currentscreenx;
} xudp_properties_t;

typedef struct {
	uint32_t magic;
	uint16_t identifier;
	uint16_t tilewidth;
	uint16_t tileheight;
	uint16_t segid;
	uint16_t pixptr;
	uint16_t datalen;
	uint16_t dummy;
	unsigned char data[PIXELS_PER_PACKET * 3];
} xudp_packet_t;

#define sgn(a) ((a > 0) - (a < 0))

#endif

