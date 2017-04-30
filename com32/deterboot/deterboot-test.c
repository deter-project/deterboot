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
 * deterboot-test.c
 *
 * This module tests the Deter stage-0 boot loader code
 */

#include <string.h>
#include <stdio.h>
#include <consoles.h>
#include <unistd.h>
#include <syslinux/pxe.h>
#include <dhcp.h>
#include <net.h>
#include <netinet/in.h>
#include <lwip/sockets.h>

#include "deterboot.h"
#include "testing.h"
#include "walrus.h"

int test_getNetInfo(void);
int test_bootWhat_mfs(void);
int tryBoot(void);

struct NetInfo netinfo;

struct WTFTest wtf = {
  .collector = "192.168.1.247", 
  .test = "boot-mfs", 
  .participant = "deterboot",
  .counter = 0
};

int main(void)
{
  lwip_socket_init();

  test_getNetInfo();

  WTFok(&wtf, "starting boot process");
  int response = tryBoot();
  if(response == BIBOOTWHAT_TYPE_WAIT)
  {
    do { sleep(5); } while(tryBoot() == BIBOOTWHAT_TYPE_WAIT);
  }

  printf("boot process finished\n");
}

int test_getNetInfo(void)
{
  int err = getNetInfo(&netinfo);
  if(err) 
  {
    WTFerror(&wtf, "getNetInfo failed %d", err);
    return TEST_ERROR;
  }

  char *me = inet_ntoa(netinfo.myAddr);
  char *buf = malloc(strlen(wtf.participant)+3);
  sprintf(buf, "%s-%s", wtf.participant, &me[strlen(me)-3]);
  wtf.participant = buf;

  u32_t bossExpected = inet_addr("192.168.252.1"); 
  if(bossExpected != netinfo.bossAddr.s_addr)
  {
    WTFerror(&wtf, "unexpected boss address %s", inet_ntoa(netinfo.bossAddr));
    return TEST_ERROR;
  }

  WTFok(&wtf, "getNetInfo test finished");
  return TEST_OK;
}

int doMFSBoot(const char *path) 
{
	if(strcmp(path, "http://192.168.252.1/linux-mfs") != 0)
	{
		WTFerror(&wtf, "unexpected mfs path: %s\n", path);
		return TEST_ERROR;
	}
	WTFok(&wtf, "jumping into mfs now ...");
	return bootMFS(path);
}

int doChainBoot(const char *disk, int partition)
{
	WTFok(&wtf, "chainbooting into %s:%d ...", disk, partition);
	return chainBoot(disk, partition);
}



int tryBoot(void)
{
  struct BootWhatResponse br;
  int err = bootWhat(&netinfo, &br);
  
  if(err != BOOTWHAT_OK)
  {
    WTFerror(&wtf, "boot-what comms failure %d", err);
    return TEST_ERROR;
  }
  
  if(br.info.opcode != BIOPCODE_REPLY)
  {
    WTFerror(&wtf, "unexpected opcode: %d", br.info.opcode);
    return TEST_ERROR;
  }

	switch(br.what->type) {
		case BIBOOTWHAT_TYPE_MFS: doMFSBoot(br.what->what.mfs); break;
		case BIBOOTWHAT_TYPE_PART: doChainBoot("hd0", br.what->what.partition); break;
    case BIBOOTWHAT_TYPE_WAIT: 
      WTFok(&wtf, "entering wait period");
      printf("\rwaiting ... ");
      return BIBOOTWHAT_TYPE_WAIT;
		
		// *** NOT IMPLEMENTED *** //
		case BIBOOTWHAT_TYPE_SYSID:
		case BIBOOTWHAT_TYPE_MB:
		case BIBOOTWHAT_TYPE_REBOOT:
		case BIBOOTWHAT_TYPE_AUTO:
		case BIBOOTWHAT_TYPE_RESTART:
		case BIBOOTWHAT_TYPE_DISKPART:
			WTFerror(&wtf, "unexpected bootwhat type: %d", br.what->type);
			return TEST_ERROR;
	}
  

  if(err)
  {
    WTFerror(&wtf, "booting mfs failed %d", err);
  }
  
  return TEST_OK;
}
