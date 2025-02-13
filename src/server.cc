#include <iostream>
#include <signal.h>

#include "store.h"

void signal_handler(int sig_number) {
  std::cout << "Capture Ctrl+C" << std::endl;
  exit(0);
}

int main() {
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    std::cerr << "cannot register signal handler!" << std::endl;
    exit(-1);
  }

  LightningStore store("/tmp/lightning", 1024 * 1024 * 1024);
  store.Run();

  return 0;
}
