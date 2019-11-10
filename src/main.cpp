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
#define WINNING_FX_TIME (2000)  //NOTICE: make sure the number isn't too big. User might start a new game before the effect ends.
#define NO_GAME_FX_TIME (500)
#define WINNING_SENSOR_PIN (7)   // winning switch pin in the RPi (GPIO12)
#define START_GAME_PIN (3)       // coin switch pin in the RPi (GPIO25)
#define LIMIT_SWITCH_2_PIN (9)   // limit switch r/l pin in the RPi (GPIO20)
#define LIMIT_SWITCH_1_PIN (10)  // limit switch f/b pin in the RPi (GPIO16)

Adafruit_NeoPixel strip(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

Button plus_btn(BLUE_BTN_PIN);
Button minus_btn(RED_BTN_PIN);
Button coin_btn(START_GAME_PIN);

Timer reset_timer;
Timer fx_update;

int8_t score = 0;
uint8_t last_score = 0;
uint8_t level[] = {0, 28, 48, 60, 64};  //levels 0 to 4
uint8_t start_point = 0;
unsigned long last_update = 0;
unsigned long fx_last_update = 0;
unsigned long fx_last_update2 = 0;
bool game = false;
bool limits_state;               // = pressed = at home = 0
bool prev_limits_state = false;  // = pressed = at home
int8_t increment = 1;
volatile int8_t i = 0;  //TODO: change i and j to normal names...
volatile int8_t j = 0;

void delay_millis(uint32_t ms) {
    uint32_t start_ms = millis();
    while (millis() - start_ms < ms)
        ;
}

void level_up(uint8_t led_num) {
    if (led_num == level[1]) start_point = 0;   //up from level 0 to 1
    if (led_num == level[2]) start_point = 28;  //up from level 1 to 2
    if (led_num == level[3]) start_point = 48;  //up from level 2 to 3
    if (led_num == level[4]) start_point = 60;  //...

    for (uint8_t i = start_point; i < led_num; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255));
        strip.show();
        delay_millis(50);
    }
}

void level_down(uint8_t wait, uint8_t led_num) {  //clear prev level's frame and do the opposite direction effect with red color
    uint8_t start_point;
    if (led_num == level[0]) start_point = 28;  //down from level 1 to 0
    if (led_num == level[1]) start_point = 48;  //down from level 2 to 1
    if (led_num == level[2]) start_point = 60;  //down from level 3 to 2
    if (led_num == level[3]) start_point = 64;  //...

    if (millis() - last_update > wait) {
        last_update = millis();
        for (int8_t i = start_point - 1; i >= led_num; i--) {
            strip.setPixelColor(i, strip.Color(255, 0, 0));
            strip.show();
        }
    }
    delay_millis(1000);
    for (int8_t i = start_point - 1; i >= led_num; i--) {
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
        strip.show();
    }
}

void no_game_fx() {  //explanation effect
    if (increment == -1) {
        for (i = level[j] - 1; i >= level[j - 1]; i--) {
            strip.setPixelColor(i, strip.Color(0, 0, 0));
            strip.show();
        }
    } else {
        if (j <= 2) {
            for (i = level[j]; i < level[j + 1]; i++) {
                strip.setPixelColor(i, strip.Color(0, 0, 255));
                strip.show();
            }
        }
        if (j == 3) {
            for (i = level[j]; i < level[j + 1]; i++) {
                strip.setPixelColor(i, strip.Color(255, 0, 255));
                strip.show();
            }
        }
    }
    j += increment;
    if (j <= 0 || j >= 4) increment = -increment;
}

void reset_game() {
    // strip.clear();
    // strip.show();
    score = 0;
    last_score = 4;
    digitalWrite(WINNING_SENSOR_PIN, LOW);
}

void winning_check() {
    if (score == 4) {
        digitalWrite(WINNING_SENSOR_PIN, HIGH);
        // Serial.println("YOU WON");
    } else
        digitalWrite(WINNING_SENSOR_PIN, LOW);
}

void update_score() {
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
            if (last_score == 1) level_down(50, level[0]);
            last_score = 0;
            break;
        case 1:
            if (last_score == 0) level_up(level[1]);        // if last_score was 0 make the blue effect because level is up
            if (last_score == 2) level_down(50, level[1]);  // if last_score was 2 make the red effect because level is down
            last_score = 1;
            break;
        case 2:
            if (last_score == 1) level_up(level[2]);
            if (last_score == 3) level_down(50, level[2]);
            last_score = 2;
            break;
        case 3:
            if (last_score == 2) level_up(level[3]);
            if (last_score == 4) level_down(50, level[3]);
            last_score = 3;
            break;
        case 4:
            winning_check();
            reset_timer.start();
            winning();
            break;
    }
}
void start_game() {
    switch (score) {
        case 0:
            if (last_score == 1) level_down(50, level[0]);
            break;
        case 1:
            for (uint8_t i = level[0]; i < level[1]; i++) {
                strip.setPixelColor(i, strip.Color(0, 0, 255));
                strip.show();
            }
            break;
        case 2:
            for (uint8_t i = level[0]; i < level[2]; i++) {
                strip.setPixelColor(i, strip.Color(0, 0, 255));
                strip.show();
            }
            break;
        case 3:
            for (uint8_t i = level[0]; i < level[3]; i++) {
                strip.setPixelColor(i, strip.Color(0, 0, 255));
                strip.show();
            }
            break;
    }
}

void check_for_game() {
    //FIXME: The current score level flickers when game starts and effect stops. Maybe it has to do with the functions order
    //FIXME: It shows fake winning once in a million time

    if (coin_btn.read() == 0) {
        game = true;
        strip.clear();
        strip.show();
    }

    if (!game && !fx_update.isRunning()) {  //TODO: add limit switches conditions just like down there V
        Serial.println("NO GAME...");
        fx_update.start();
    }

    if (game) {
        fx_update.stop();
        start_game();
    }

    limits_state = digitalRead(LIMIT_SWITCH_1_PIN) && digitalRead(LIMIT_SWITCH_2_PIN);

    //NOTICE: the arduino doesn't know if a game starts if someone plays manually. For that I need to add a condition down here.
    if (limits_state) {  // claw moved and GAME ON
        // Serial.println("GAME ON!!!");
        prev_limits_state = true;
    }

    if (!digitalRead(LIMIT_SWITCH_1_PIN) && !digitalRead(LIMIT_SWITCH_2_PIN) && prev_limits_state) {  // GAME OVER...
        strip.clear();
        strip.show();
        game = false;
        prev_limits_state = false;
    }
}

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    pinMode(LED_DATA_PIN, OUTPUT);
    pinMode(BLUE_BTN_PIN, INPUT);
    pinMode(RED_BTN_PIN, INPUT);
    pinMode(START_GAME_PIN, INPUT);
    pinMode(LIMIT_SWITCH_1_PIN, INPUT);  // When pressed = 0
    pinMode(LIMIT_SWITCH_2_PIN, INPUT);  // When depressed = 1
    pinMode(WINNING_SENSOR_PIN, OUTPUT);
    digitalWrite(WINNING_SENSOR_PIN, LOW);

    plus_btn.begin();
    minus_btn.begin();
    coin_btn.begin();

    reset_timer.setCallback(reset_game);
    reset_timer.setTimeout(WINNING_FX_TIME);

    fx_update.setCallback(no_game_fx);
    fx_update.setInterval(NO_GAME_FX_TIME);

    strip.begin();
    strip.show();  // Turn OFF all pixels
    strip.setBrightness(LED_BRIGHTNESS);

    Serial.println(F(
        "_______________________________\n"
        "\n"
        "   G e a r   M a c h i n e     \n"
        "_______________________________\n"
        "\n"
        "Made by KD Technology\n"
        "\n"));
}

void loop() {
    check_for_game();
    update_score();
    TimerManager::instance().update();
}
