#include "iotKernel.h"

IotKernel iot_kernel("device","0.0.1");

void setup() {
  iot_kernel.init();
}

void loop() {
 iot_kernel.loop();
}