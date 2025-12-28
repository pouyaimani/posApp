#include "hal/dev/dev.h"

int main() {
    Device *dev = getDevice();
    OOP_CALL(dev, init);
    // dev->vtable->init(dev);
    while (1) {
        /* code */
    }
    
}
