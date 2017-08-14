/* -------------------------------------------------------------------------- *
 *
 *   Copyright 2017 Deter-Project - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */


/*
 * deterboot.c
 *
 * This module implements the Deter stage-0 boot loader functionality
 */

#include <syslinux/pxe.h>
#include <dhcp.h>
#include <stdio.h>
#include <lwip/sockets.h>
#include <syslinux/loadfile.h>
#include <syslinux/boot.h>
#include <string.h>
#include "deterboot.h"

/*-----------------------------------------------------------------------------
 *
 * PXE Network Environment
 *
 *---------------------------------------------------------------------------*/

int getNetInfo(struct NetInfo *info)
{
  void *dhcpdata = NULL;
  size_t dhcplen = 0;
  int err = pxe_get_cached_info(
      PXENV_PACKET_TYPE_DHCP_ACK, 
      &dhcpdata, 
      &dhcplen);

  if(err) 
  {
    printf("Failed to get network information\n");
    return err;
  }

  struct dhcp_packet *pkt = (struct dhcp_packet*)dhcpdata;
  info->myAddr.s_addr = pkt->yiaddr;
  info->bossAddr.s_addr = pkt->siaddr;

  struct dhcp_option opts[256];
  dhcp_unpack_packet(pkt, dhcplen, opts);

  char *search = (char*)opts[15].data;
  printf("search domain: %s\n", search);
  info->domain = malloc(strlen(search)+1);
  strncpy(info->domain, search, strlen(search));
  info->domain[strlen(search)] = 0;

  char *host = (char*)opts[12].data;
  if(host) {
    printf("my hostname: %s\n", host);
    info->host = malloc(strlen(host)+1);
    strncpy(info->host, host, strlen(host));
    info->host[strlen(host)] = 0;
  } else {
    const char* unk = "unknown";
    printf("my hostname: %s\n", unk);
    info->host = malloc(strlen(unk)+1);
    strncpy(info->host, unk, strlen(unk));
    info->host[strlen(unk)] = 0;
  }

  return 0;
}

/*-----------------------------------------------------------------------------
 *
 * Question interface
 *
 *---------------------------------------------------------------------------*/

size_t ask(struct Question *q)
{
  struct sockaddr_in dest;
  dest.sin_family = AF_INET;
  dest.sin_port = htons(q->port);
  dest.sin_addr = q->who;
  int sock = socket(AF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in reply;
  reply.sin_family = AF_INET;
  reply.sin_port = htons(q->response_port);
  reply.sin_addr = q->me;
  int rsock = socket(AF_INET, SOCK_DGRAM, 0);
  
  int result = QUESTION_OK;

  bind(rsock, (struct sockaddr*)&reply, sizeof(reply));

  q->out_sz = sendto(
      sock, 
      q->what, 
      q->what_size, 
      0, 
      (struct sockaddr*)&dest, 
      sizeof(dest));

  if(q->out_sz < q->what_size)
  {
    result |= QUESTION_PARTIAL_SEND;
  }

  dest.sin_family = AF_INET;
  dest.sin_port = htons(q->port);
  dest.sin_addr = q->who;
  socklen_t dest_sz = sizeof(dest);
  q->in_sz = recvfrom(
      rsock, 
      q->response, 
      q->response_size, 
      0, 
      (struct sockaddr*)&dest, 
      &dest_sz);

  closesocket(sock);
  closesocket(rsock);

  return result;
}

/*-----------------------------------------------------------------------------
 *
 * Bootinfo protocol
 *
 *---------------------------------------------------------------------------*/

int bootWhat(const struct NetInfo *netinfo, struct BootWhatResponse *br)
{
  struct boot_info bi_out = {
    .version = BIVERSION_CURRENT,
    .opcode = BIOPCODE_REQUEST,
    .status = 0
  };
  memset(&bi_out.data, 0, MAX_BOOT_DATA);

  struct boot_info bi_in;
  memset(&bi_in, 0, sizeof(struct boot_info));

  struct Question question = {
    .who = netinfo->bossAddr,
    .port = BICLIENT_PORT,
    .response_port = BISERVER_PORT,
    .what = &bi_out,
    .what_size = sizeof(struct boot_info),
    .me = netinfo->myAddr,
    .response = &bi_in,
    .response_size = sizeof(struct boot_info)
  };

  int err = ask(&question);
  if(err != QUESTION_OK)
  {
    return BOOTWHAT_COMMS_ERROR;
  }

  memcpy(&br->info, &bi_in, sizeof(struct boot_info));
  br->what = NULL;

  if(bi_in.opcode == BIOPCODE_REPLY)
  {
    br->what = (struct boot_what*)&br->info.data;
  }

  return BOOTWHAT_OK;
}

/*-----------------------------------------------------------------------------
 *
 * Boot loading functionality
 *
 *---------------------------------------------------------------------------*/

int loadMFS(const char *path, void **buf, size_t *len)
{
  return loadfile(path, buf, len);
}

int bootMFS(const void *path)
{
  const char *fmt =
    "linux.c32 %s/bzImage initrd=%s/rootfs.cpio.gz BOOT=live "
    "console=ttyS0,115200, console=ttyS1,115200, console=ttyS2,115200, console=tty1 "
    "biosdevname=0 net.ifnames=0 ";

  size_t sz = snprintf(NULL, 0, fmt, path, path);
  char *cmd = malloc(sz+1);
  snprintf(cmd, sz+1, fmt, path, path);
  cmd[sz] = 0;

  int result = syslinux_run_command(cmd);
	free(cmd);
	return result;
}

int chainBoot(const char *disk, int partition)
{
	const char *fmt = "chain.c32 %s %d";
  size_t sz = snprintf(NULL, 0, fmt, disk, partition);
  char *cmd = malloc(sz+1);
  snprintf(cmd, sz+1, fmt, disk, partition);
  cmd[sz] = 0;

  int result = syslinux_run_command(cmd);
	free(cmd);
	return result;
}
