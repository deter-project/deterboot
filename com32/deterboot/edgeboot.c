/* -------------------------------------------------------------------------- *
 *
 *   Copyright 2017 Deter-Project - All Rights Reserved
 *
 *   Apache 2.0 License
 *
 *   This program implements the edge-clink boot mechanisim
 *
 * ----------------------------------------------------------------------- */

#include <syslinux/pxe.h>
#include <lwip/sockets.h>
#include <dhcp.h>
#include <net.h>
#include "deterboot.h"

struct NetInfo netinfo;

int getBootPart(int *part);

int main(void) {
  lwip_socket_init();

  int part=0;
  int err = getBootPart(&part);
  if(err) {
    printf("error getting partition info\n");
    exit(1);
  }

  printf("booting partition %d\n", part);
  
  chainBoot("hd0", part);

}

int getBootPart(int *part) {
  //get dhcp data from syslinux
  void *dhcpdata = NULL;
  size_t dhcplen = 0;
  int err = pxe_get_cached_info(
      PXENV_PACKET_TYPE_DHCP_ACK, 
      &dhcpdata, 
      &dhcplen);

  if(err) {
    printf("Failed to get network information\n");
    return err;
  }

  //unpack dhcp options
  struct dhcp_packet *pkt = (struct dhcp_packet*)dhcpdata;
  struct dhcp_option opts[256];
  opts[147].data = NULL;
  dhcp_unpack_packet(pkt, dhcplen, opts);

  //no option sent
  if(opts[147].data == NULL) return -1;

  //grab the boot partition option
  *part = ntohl(*((int*)opts[147].data));
  
  return 0;
}
