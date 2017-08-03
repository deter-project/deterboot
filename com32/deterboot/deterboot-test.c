/* -------------------------------------------------------------------------- *
 *
 *   Copyright 2017 Deter-Project - All Rights Reserved
 *
 *    Apache 2.0 License
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

int do_getNetInfo(void);
int tryBoot(void);

struct NetInfo netinfo;

/*
struct WTFTest wtf = {
  .collector = "walrus", 
  .test = "preboot", 
  .participant = "deterboot",
  .counter = 0
};
*/

int main(void)
{
  lwip_socket_init();

  do_getNetInfo();

  //WTFok(&wtf, "starting boot process");
  int response = tryBoot();
  if(response == BIBOOTWHAT_TYPE_WAIT)
  {
    do { sleep(5); } while(tryBoot() == BIBOOTWHAT_TYPE_WAIT);
  }

  printf("boot process finished\n");
}

int do_getNetInfo(void)
{
  int err = getNetInfo(&netinfo);
  if(err) 
  {
    //WTFerror(&wtf, "getNetInfo failed %d", err);
    printf("getNetInfo failed %d\n", err);
    return TEST_ERROR;
  }

  //wtf.participant = netinfo.host;

  char* buf = malloc(strlen("walrus.") + strlen(netinfo.domain));
  sprintf(buf, "walrus.%s", netinfo.domain);
  //wtf.collector = buf;

  //WTFok(&wtf, "getNetInfo finished");
  return TEST_OK;
}

int doMFSBoot(const char *path) 
{
  if(strcmp(path, "http://192.168.252.1/linux-mfs") != 0)
  {
    //WTFerror(&wtf, "unexpected mfs path: %s\n", path);
    printf("unexpected mfs path: %s\n", path);
    return TEST_ERROR;
  }
  //WTFok(&wtf, "jumping into mfs now ...");
  return bootMFS(path);
}

int doChainBoot(const char *disk, int partition)
{
  //WTFok(&wtf, "chainbooting into %s:%d ...", disk, partition);
  return chainBoot(disk, partition);
}



int tryBoot(void)
{
  struct BootWhatResponse br;
  int err = bootWhat(&netinfo, &br);

  if(err != BOOTWHAT_OK)
  {
    //WTFerror(&wtf, "boot-what comms failure %d", err);
    printf("boot-what comms failure %d\n", err);
    return TEST_ERROR;
  }

  if(br.info.opcode != BIOPCODE_REPLY)
  {
    //WTFerror(&wtf, "unexpected opcode: %d", br.info.opcode);
    printf("unexpected opcode: %d\n", br.info.opcode);
    return TEST_ERROR;
  }

  switch(br.what->type) {
    case BIBOOTWHAT_TYPE_MFS: 
      doMFSBoot(br.what->what.mfs); 
      break;

    case BIBOOTWHAT_TYPE_PART: 
      doChainBoot("hd0", br.what->what.partition); 
      break;

    case BIBOOTWHAT_TYPE_WAIT: 
      //WTFok(&wtf, "entering wait period");
      printf("\rwaiting ... ");
      return BIBOOTWHAT_TYPE_WAIT;

      // *** NOT IMPLEMENTED *** //
    case BIBOOTWHAT_TYPE_SYSID:
    case BIBOOTWHAT_TYPE_MB:
    case BIBOOTWHAT_TYPE_REBOOT:
    case BIBOOTWHAT_TYPE_AUTO:
    case BIBOOTWHAT_TYPE_RESTART:
    case BIBOOTWHAT_TYPE_DISKPART:
      //WTFerror(&wtf, "unexpected bootwhat type: %d", br.what->type);
      printf("unexpected bootwhat type: %d\n", br.what->type);
      return TEST_ERROR;
  }


  if(err)
  {
    printf("booting mfs failed %d\n", err);
    //WTFerror(&wtf, "booting mfs failed %d", err);
  }

  return TEST_OK;
}
