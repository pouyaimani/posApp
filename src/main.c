#include "hal/dev/dev.h"
#include "display/display.h"

int main() {
    Device *dev = getDevice();
    OOP_CALL(dev, init);
    Display *disp = getDisplay();
    disp->init();
    while (1) {
        disp->update();
    }
    
}
