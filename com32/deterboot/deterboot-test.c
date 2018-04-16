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
#include <syslinux/reboot.h>
#include <dhcp.h>
#include <net.h>
#include <netinet/in.h>
#include <lwip/sockets.h>

#include "deterboot.h"
#include "testing.h"
#include "walrus.h"

int do_getNetInfo(void);
int tryBoot(void);
int do_wait(void);
int handleResponse(struct boot_what *what);

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
    //do { sleep(5); } while(tryBoot() == BIBOOTWHAT_TYPE_WAIT);
    int err = do_wait();
    if(err) {
      syslinux_reboot(0); 
    }
  }

  printf("boot process finished\n");
}

int do_wait(void)
{

  int sock, n;
  struct sockaddr_in name;
  struct boot_info reply;

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = htons(BISERVER_PORT);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0) {
    fprintf(stderr, "unable to open socket\n");
    return 1;
  }
  if(bind(sock, (struct sockaddr*)&name, sizeof(name))) {
    fprintf(stderr, "unable to listen on socket\n");
    return 1;
  }

  n = recvfrom(sock, &reply, sizeof(reply), 0, NULL, NULL);
  if(n < 0) {
    fprintf(stderr, "wait failed\n");
    return 1;
  }

  struct boot_what what;
  if(reply.opcode == BIOPCODE_REPLY)
  {
    what = *((struct boot_what*)reply.data);
  }

  return handleResponse(&what);
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

  return handleResponse(br.what);

  /*
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
  */

}

int handleResponse(struct boot_what *what)
{
  switch(what->type) {
    case BIBOOTWHAT_TYPE_MFS: 
      return doMFSBoot(what->what.mfs); 

    case BIBOOTWHAT_TYPE_PART: 
      return doChainBoot("hd0", what->what.partition); 

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
    default:
      //WTFerror(&wtf, "unexpected bootwhat type: %d", br.what->type);
      printf("unexpected bootwhat type: %d\n", what->type);
      return TEST_ERROR;
  }
}
