/*
	Author: Yuval Kedar - KD Technology
	Instagram: https://www.instagram.com/kd_technology/
	Date: Oct 19
	Dev board: Arduino Uno
	
	"Gear Machine " a progress machine for Gigantic - Clawee.
	
	Arduino Uno communicates with RPi.
	A hamer (instead of a claw) suppose to hit buttons.
    There are two type of buttons: Red and blue.
    Red btn = -1
    Blue btn = +1

    On the machine's background there is a LED matrix that shows the user's progress.
    There are 4 levels (square frames) until one reaches the core and wins.

    The trick? The floor, which includes the buttons, is rotating.
*/

#include <Adafruit_NeoPixel.h>
#include <Button.h>  // https://github.com/madleech/Button
#include <timer.h>   // https://github.com/brunocalou/Timer
#include <timerManager.h>
#include "Arduino.h"

#define SERIAL_BAUDRATE (115200)
#define LED_DATA_PIN (6)
#define BLUE_BTN_PIN (5)
#define RED_BTN_PIN (4)
#define NUM_LEDS (64)
#define LED_BRIGHTNESS (200)
#define WINNING_FX_TIME (1500)  //NOTICE: make sure the number isn't too big. User might start a new game before the effect ends.
#define LEVEL_FX_TIME (50)

Adafruit_NeoPixel strip(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

Button plus_btn(BLUE_BTN_PIN);
Button minus_btn(RED_BTN_PIN);

Timer winning_timer;

int8_t score = 0;
uint8_t last_score = 0;
uint8_t level[] = {0, 28, 48, 60, 64};  //levels 0 to 4
uint32_t last_update;
// uint8_t led_counter = 0;

void level_up(uint8_t wait, uint8_t led_num) {
    uint8_t start_point;
    if (led_num == level[1]) start_point = 0;   //up from level 0 to 1
    if (led_num == level[2]) start_point = 28;  //up from level 1 to 2
    if (led_num == level[3]) start_point = 48;  //up from level 2 to 3
    if (led_num == level[4]) start_point = 60;  //...

    for (uint8_t i = start_point; i < led_num; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255));
        strip.show();
        delay(wait);
        //asdasf
    }
}

void level_down(uint8_t wait, uint8_t led_num) {  //clear prev level's frame and do the opposite direction effect with red color
    uint8_t start_point;
    if (led_num == level[0]) start_point = 27;  //down from level 1 to 0
    if (led_num == level[1]) start_point = 47;  //down from level 2 to 1
    if (led_num == level[2]) start_point = 59;  //down from level 3 to 2
    if (led_num == level[3]) start_point = 63;  //...

    for (uint8_t i = start_point; i >= led_num; i--) {
        strip.setPixelColor(i, strip.Color(255, 0, 0));
        strip.show();
        delay(wait);  //TODO: change it to timer
    }
    for (uint8_t i = start_point; i >= led_num; i--) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
        strip.show();
    }
}

void winning() {  // Rainbow cycle along whole strip.
    for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
        for (uint8_t i = 0; i < strip.numPixels(); i++) {  // For each pixel in strip...
            int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
            strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        strip.show();  // Update strip with new contents
    }
}

void reset_game() {
    strip.clear();
    strip.show();
    score = 0;
    last_score = 4;
}

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    pinMode(LED_DATA_PIN, OUTPUT);
    pinMode(BLUE_BTN_PIN, INPUT);
    pinMode(RED_BTN_PIN, INPUT);

    plus_btn.begin();
    minus_btn.begin();

    winning_timer.setCallback(reset_game);
    winning_timer.setTimeout(WINNING_FX_TIME);

    strip.begin();
    strip.show();  // Turn OFF all pixels
    strip.setBrightness(LED_BRIGHTNESS);

    Serial.println(F("Press 'p' to add score and 'm' to reduce score."));

    Serial.println(F(
        "_______________________________\n"
        "\n"
        "   G e a r   M a c h i n e     \n"
        "_______________________________\n"
        "\n"
        "Made by KD Technology\n"
        "\n"
        "Press 'p' to add score and 'm' to reduce score.\n"
        "\n"));
}

void loop() {
    if (plus_btn.pressed()) {
        score++;
        if (score >= 4) {
            score = 4;
        }
    }
    if (minus_btn.pressed()) {
        score--;
        if (score <= 0) {
            score = 0;
        }
    }

    switch (score) {
        case 0:
            if (last_score == 1) level_down(50, level[0]);  //FIXME: something is wrong here. probably gets below 0.
            // FIXME: I should make an independent function for it.
            last_score = 0;
            break;
        case 1:
            // if last_score was 0 make the blue effect because level is up
            if (last_score == 0) level_up(50, level[1]);
            // if last_score was 2 make the red effect because level is down
            if (last_score == 2) level_down(50, level[1]);
            last_score = 1;
            break;
        case 2:
            if (last_score == 1) level_up(50, level[2]);
            if (last_score == 3) level_down(50, level[2]);
            last_score = 2;
            break;
        case 3:
            if (last_score == 2) level_up(50, level[3]);
            if (last_score == 4) level_down(50, level[3]);
            last_score = 3;
            break;
        case 4:
            winning_timer.start();
            winning();
            break;
    }

    Serial.println(score);
    TimerManager::instance().update();
}