#include <unistd.h>

int main(int argc, char *argv[])
{
  // This particular construction creates a BasicBlock that starts with a
  // (currently) unrecorded Instruction. This ensures that the BasicBlockStore
  // creation works correctly in the case that the first recorded Instruction
  // is not the first Instruction in the BasicBlock.
  switch (argc) {
    case 1:
      sleep(0);
      break;
    default:
      break;
  }

  return 0;
}

