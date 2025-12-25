#include "hal/dev/dev.h"

int main() {
    Device *dev = getDevice();
    dev->vtable->init(dev);
    while (1) {
        /* code */
    }
    
}
