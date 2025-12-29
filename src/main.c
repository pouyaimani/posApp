#include "hal/dev/dev.h"
#include "display/display.h"

int main() {
    Display *disp = getDisplay();
    disp->init();
    Device *dev = getDevice();
    OOP_CALL(dev, init);
    while (1) {
        disp->update();
    }
    
}
