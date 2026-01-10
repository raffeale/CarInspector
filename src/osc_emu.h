#include "esp32-hal-ledc.h"
//
const int oscPin = 5; // esp32s3 GPIO 8
const int freq = 20000000; // Frequency in Hz
//const int freq = 4000000; // Frequency in Hz
const int resolution = 2; // Resolution (8 bits = 0–255)
const int channel = 0; // PWM channel

void output20Mhz_osc1() {
    ledcSetClockSource(LEDC_USE_APB_CLK);
    ledcAttach(oscPin, freq, resolution);
    ledcWrite(oscPin, 2);

}

void stopOutput_osc1() {
    ledcDetach(oscPin);
    ledcWrite(oscPin, 0);
    
}