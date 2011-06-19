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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"
#include "m_option.h"
#include "fastmemcpy.h"
#include "vo_xudp.h"



static const vo_info_t info = 
{
	"XMOS UDP driver",
	"xudp",
	"Sam Garcia <sam@xmos.com>",
	""
};

const LIBVO_EXTERN (xudp)
/* General variables */
static xudp_packet_t    xudp_packet;
static int              xudp_packetlen_b;
static xudp_properties_t xudp = { "rgb", IMGFMT_YV12,
                                   -1, -1, 24 ,
                                   DEFAULT_WIDTH, DEFAULT_HEIGHT, 255,
                                   255, 0,
                                   DEFAULT_PREFIX, DEFAULT_PORT, -1
                                   };

// UDP Functions .....................................

static int udp_send(int x, int y) {
  if ( x < 256 && y < 256)
  {
      int ip = xudp.ipaddresses[x][y];
	  if (sendto(xudp.fd, &xudp_packet, xudp_packetlen_b, 0, (struct sockaddr *)&xudp.addrs[ip & 0xFF][(ip >> 8) & 0xFF], sizeof(struct sockaddr_in)) != xudp_packetlen_b)
	  { 
		  mp_msg(MSGT_VO, MSGL_ERR, "xudp: Unable to send to node %i,%i with IP %i, %i.\n", x,y, (ip & 0xFF), (ip >>8) & 0xFF);
		  return 1;
	  }
	  else
	  {
	    return 0;
	  }
	}
	else
	{
	  mp_msg(MSGT_VO, MSGL_ERR, "xudp: Node %i,%i out of bounds.\n", x,y);
	  return 1;
  }
}

// ------------------ Other ------------------------------


static int config(
  uint32_t width, uint32_t height,
  uint32_t d_width, uint32_t d_height,
  uint32_t flags, char *title, uint32_t format)
{
	
	if (xudp.tilewidth == 0 || xudp.tileheight == 0)
	{
	  // Tile width uninitialised, set to video width as default
		mp_msg(MSGT_VO, MSGL_V, "xudp: Using default tile size\n");
	  xudp.tilewidth = width;
	  xudp.tileheight = height;
	}

	if (xudp.width < 0 || xudp.height < 0)
	{
		if (xudp.width < 0)
		{ 
		  // use width of movie
			xudp.width = width;
		}
		if (xudp.height < 0)
		{
		  // use height of movie
			xudp.height = height;
		}
		
    xudp.linesperpacket = PIXELS_PER_PACKET / xudp.tilewidth;
	}


	mp_msg(MSGT_VO, MSGL_V, "vo_config xudp called, format %x\n", format);
	return 0;
}


void handle_frame( mp_image_t *data )
{
  // Now we send the image via udp
  int x, y,packet,packetline;
    
	xudp_packet.identifier = htons(XMOS_DATA);
	xudp_packet.tilewidth = xudp.tilewidth;
	xudp_packet.tileheight = xudp.tileheight; 	
  
  // Loop through the packets to be sent to each node
  for (packet=0; packet < intceil(xudp.tileheight, xudp.linesperpacket); packet++)
  {
    xudp_packet.segid = packet;
    xudp_packet.pixptr = xudp.tilewidth * packet * xudp.linesperpacket;

    for (x=0; x < xudp.width / xudp.tilewidth; x ++)
    {
      for (y=0; y < xudp.height / xudp.tileheight; y++)
      {      
        // Send packet
        unsigned char *imageptr = data->planes[0] + ( (x * xudp.tilewidth) + (y * xudp.tileheight + packet * xudp.linesperpacket) * xudp.width) * 3;
        int linestosend = xudp.tileheight - xudp.linesperpacket * packet;
        unsigned char *packetptr = (unsigned char*)xudp_packet.data;
        
        if (linestosend > xudp.linesperpacket)
          linestosend = xudp.linesperpacket;
        xudp_packet.datalen = xudp.tilewidth * 3 * linestosend;
        xudp_packetlen_b = PACKET_HEADER + xudp_packet.datalen + 12;
         
        for (packetline=0; packetline < linestosend; packetline++)
        {
          fast_memcpy(packetptr, imageptr, xudp.tilewidth * 3);
          packetptr += xudp.tilewidth * 3;
          imageptr  += xudp.width * 3;
        }
        
        if (xudp.noinit)
        {
          if (y == 0 && x == 0)
          {
            if (udp_send(0, 0))
            {
              mp_msg(MSGT_VO, MSGL_ERR, "xudp: unable to send packet\n");		
            }
          }
        }            
        else
        {
          if (y >= xudp.simlevel)
          {
            udp_send(BCAST_HOST_X, BCAST_HOST_Y);
          }
          else
          {
            if (udp_send(x, y))
            {
              mp_msg(MSGT_VO, MSGL_ERR, "xudp: unable to send packet\n");		
            }
          }
        }
      }
    }
  }
  
  // Send latch signal
  xudp_packet.identifier = htons(XMOS_LATCH);
  xudp_packetlen_b = MIN_PACKET_SIZE;
  udp_send(BCAST_HOST_X, BCAST_HOST_Y);
  
	return;
}


static int query_format(uint32_t flags)
{
  int caps = VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
               
  switch(flags)
  {
    case (IMGFMT_RGB24):
      return caps;
    default:
      mp_msg(MSGT_VO, MSGL_V, "xudp: colorspace fail %X \n", flags);
      return 0;
  }
}

static void uninit(void)
{
	mp_msg(MSGT_VO, MSGL_V, "xudp: uninit called\n");	
	close(xudp.fd);
}

static void check_events(void)
{
}

static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
  return 0;
}

static void draw_osd(void)
{
}

static void flip_page(void)
{
}

static int draw_frame(uint8_t *src[])
{
	return 0;
}

void parsecoords(char *p, int *x, int *y)
{
  char *q = p;
		      
  while (*q != '\0' && *q != '_') q++;
  if (*q == '\0') return;
  *q = '\0';
  *x = atoi(p);
  q++;
  *y = atoi(q);
  return;
}

static int preinit(const char *arg) {

	int x,y;
	
	// Defaults
	for (x=0; x<256; x++)
	{
	  for (y=0; y<256; y++)
	  {
	    xudp.ipaddresses[x][y] = 0;
	  }
	}
  xudp.currentchainy = 0;
  xudp.currentchainx = 0;
  xudp.noinit = 1;
  strcpy(xudp.prefix, DEFAULT_PREFIX);
  xudp.port = DEFAULT_PORT;
  xudp.tilewidth = 0;
  xudp.tileheight = 0;

  // Create socket
  xudp.fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (xudp.fd < 0)
	{
	  mp_msg(MSGT_VO, MSGL_ERR, "xudp: Couldn't create socket.\n");	
		return 1;
	}
	
	// Set broadcast messages possible
	{
    int bOptVal = 1;
    int bOptLen = sizeof(bOptVal);
	  setsockopt (xudp.fd, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, bOptLen);
	}
	
	// Set magic number
	memcpy(&xudp_packet.magic, magicNumber, 4);

  if (arg != NULL)
  {
    if (strlen(arg) > 1)
    {
  	  char *p;
      char *tempargs = malloc(strlen(arg) + 1);
      if (!tempargs) {
	      mp_msg(MSGT_VO, MSGL_ERR, "xudp: out of memory error\n");
	      return 1;
      }

      p = tempargs;
      strcpy(p, arg);
      mp_msg(MSGT_VO, MSGL_V, "xudp: preinit called with %s\n", arg);
      
      p = strtok(tempargs, ":");
      
      while (p != NULL)
      {
        if (!strncmp(p, "prefix=", 7))
        {
          p += 7;
          sprintf(xudp.prefix, "%s", p);
		      mp_msg(MSGT_VO, MSGL_V, "xudp: prefix = %s\n", p);
	      }
	      else if (!strncmp(p, "port=", 5))
	      {
	        p += 5;
		      xudp.port = atoi(p);
		      mp_msg(MSGT_VO, MSGL_V, "xudp: port = %s\n", p);
	      }
	      else if (!strncmp(p, "tilewidth=", 10))
	      {
	        p += 10;
          xudp.tilewidth = atoi(p);
		      mp_msg(MSGT_VO, MSGL_V, "xudp: tilewidth = %s\n", p);
	      }
        else if (!strncmp(p, "tileheight=", 11))
	      {
	        p += 11;
          xudp.tileheight = atoi(p);
		      mp_msg(MSGT_VO, MSGL_V, "xudp: tileheight = %s\n", p);
	      }
	      else if (!strncmp(p, "chainstart=", 11))
	      {
	        p += 11;
	        parsecoords(p, &x, &y);
		      mp_msg(MSGT_VO, MSGL_V, "xudp: chainstart: %i, %i\n", x, y);
		      
		      xudp.noinit = 0;
		      xudp.currentchainy++;
		      xudp.currentchainx = 1;
		      xudp.currentscreeny = y;
		      xudp.currentscreenx = x;
		      xudp.ipaddresses[xudp.currentscreenx][xudp.currentscreeny] = (xudp.currentchainy << 8) | xudp.currentchainx;
     		      mp_msg(MSGT_VO, MSGL_V, "xudp: node %i,%i now has IP address %i,%i\n", xudp.currentscreenx, xudp.currentscreeny, xudp.currentchainx, xudp.currentchainy);

	      }
	      else if (!strncmp(p, "chainpoint=", 11))
	      {
	        p += 11;
	        parsecoords(p, &x, &y);
		      mp_msg(MSGT_VO, MSGL_V, "xudp: chainpoint %i, %i\n", x, y);
		      
		      if ( x == xudp.currentscreenx && y == xudp.currentscreeny)
		      {
  		      mp_msg(MSGT_VO, MSGL_V, "xudp: chainpoint ignored\n");
		      }
		      else if (x == xudp.currentscreenx)
		      {
		        while (xudp.currentscreeny != y)
		        {
		          xudp.currentscreeny += sgn(y - xudp.currentscreeny);
		          xudp.currentchainx++;
     		      xudp.ipaddresses[xudp.currentscreenx][xudp.currentscreeny] = (xudp.currentchainy << 8) | xudp.currentchainx;
     		      mp_msg(MSGT_VO, MSGL_V, "xudp: node %i,%i now has IP address %i,%i\n", xudp.currentscreenx, xudp.currentscreeny, xudp.currentchainx, xudp.currentchainy);
		        }
		      }
		      else if (y == xudp.currentscreeny)
		      {
		        while (xudp.currentscreenx != x)
		        {
		          xudp.currentscreenx += sgn(x - xudp.currentscreenx);
		          xudp.currentchainx++;
     		      xudp.ipaddresses[xudp.currentscreenx][xudp.currentscreeny] = (xudp.currentchainy << 8) | xudp.currentchainx;;
     		      mp_msg(MSGT_VO, MSGL_V, "xudp: node %i,%i now has IP address %i,%i\n", xudp.currentscreenx, xudp.currentscreeny, xudp.currentchainx, xudp.currentchainy);
		        }
		      }
		      else
		      {
  		      mp_msg(MSGT_VO, MSGL_V, "xudp: chainpoint ignored\n");
		      }
	      }
        else if (!strncmp(p, "noinit", 6))
	      {
          xudp.noinit = 1;
		      mp_msg(MSGT_VO, MSGL_V, "xudp: noinit\n" );
	      }
	      else if (!strncmp(p, "sim=", 4))
	      {
	        p += 4;
	        xudp.simlevel = atoi(p);
	        mp_msg(MSGT_VO, MSGL_V, "xudp: sim = %s\n", p);
	      }
	      else
	      {
		      mp_msg(MSGT_VO, MSGL_V, "xudp: unrecognised argument '%s'.\n", p);
		      return 1;
	      }
         p = strtok(NULL, ":");
      }
      
      free(tempargs);
    }
  }

  // Broadcast IP
  xudp.ipaddresses[255][255] = 255*0x101;
  
  
  if (xudp.noinit)
  {
	  struct hostent *dest;
	  char address[100];
	  xudp.ipaddresses[0][0] = 0;
	  sprintf(address, "%s.%i.%i", xudp.prefix, DEFAULT_HOST_Y, DEFAULT_HOST_X);
	  dest = gethostbyname(address);
	  if (!dest) {
	    mp_msg(MSGT_VO, MSGL_ERR, "xudp: gethostbyname failed for %s\n", address);
	    return 1;
	  }
	  xudp.addrs[0][0].sin_family = AF_INET;
	  xudp.addrs[0][0].sin_port = htons(xudp.port);
	  memcpy(&xudp.addrs[0][0].sin_addr.s_addr, dest->h_addr_list[0], dest->h_length);
	  
	  sprintf(address, "%s.%i.%i", xudp.prefix, BCAST_HOST_Y, BCAST_HOST_X);
	  dest = gethostbyname(address);
	  if (!dest) {
	    mp_msg(MSGT_VO, MSGL_ERR, "xudp: gethostbyname failed for %s\n", address);
	    return 1;
	  }
	  xudp.addrs[255][255].sin_family = AF_INET;
	  xudp.addrs[255][255].sin_port = htons(xudp.port);
	  memcpy(&xudp.addrs[255][255].sin_addr.s_addr, dest->h_addr_list[0], dest->h_length);
  }
  else
  {
	  /* Init all addresses */
	  for (x = 0; x < 256; x++)
	  {
      for (y = 0; y < 256; y++)
      {
        struct hostent *dest;
        char address[100];
        
        sprintf(address, "%s.%i.%i", xudp.prefix, y, x);
        dest = gethostbyname(address);
        if (!dest) {
          mp_msg(MSGT_VO, MSGL_ERR, "xudp: gethostbyname failed for %s\n", address);
	        return 1;
        }

        xudp.addrs[x][y].sin_family = AF_INET;
        xudp.addrs[x][y].sin_port = htons(xudp.port);

        memcpy(&xudp.addrs[x][y].sin_addr.s_addr, dest->h_addr_list[0], dest->h_length);
      }
    }
  }
  
  mp_msg(MSGT_VO, MSGL_STATUS, "xudp: UDP initialised\n");

	return 0;
}

static int control(uint32_t request, void *data, ...) {
  mp_image_t *mpi = (mp_image_t *)data;
	switch (request) {
		case VOCTRL_QUERY_FORMAT:
       return query_format(*((uint32_t*)data));
    case VOCTRL_DRAW_IMAGE:
      {
	      if (mpi)
	      {
	        handle_frame( mpi );
	        nsleep(10000000);
	      }
	      else
	      {
          mp_msg(MSGT_VO, MSGL_ERR, "xudp: VOCTRL_DRAW_IMAGE ->DEAD\n");
	    	}
	      return VO_TRUE;
	    }
	    break;
	    default:
      	return VO_NOTIMPL;	    
    }
}
