
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <EEPROM.h>

/*
* LED STRIPE PROPERTIES
*/
#define PIN 7              //The signal pin connected with Arduino
#define LED_COUNT 100      //The amount of leds in the matrix
#define LINE_LED_COUNT 10  //The amount of leds per line in the matrix

/*
* PREDEFINED LED COLORS
*/
#define OFF 0x000000  //color black - leds are off
// #define OFFCOL(color) (0xFFFFFF - color) //?Contrasting color to facilitate code detection
#define OFFCOL(color) 0x000000

/*
* EEPROM save properties
*/
#define CODE_SIZE 7
#define SAVE_SIZE 10
#define SLOT_LIMIT 102

#define BIT(n) (1 << (n))

// LED stripe is an object of the Adafruit_Neopixel class
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);

/*
* FUNCTION DECLARATION
*/

void clearLEDs();
void resetLedStrip();

void testLedStrip();

bool inputParser(uint32_t *color, uint8_t aruco[], uint8_t *size, uint8_t *brightness);
void switchColor(uint32_t *color, String userInput);

void applyAruco(uint8_t arCode[], uint8_t size, uint32_t color);

void loadFromEEPROM(uint8_t aruco[], uint8_t *size, uint8_t *brightness, uint32_t *color);
void saveToEEPROM(uint8_t aruco[], uint8_t size, uint8_t brightness, uint32_t color);

int addBorder(uint8_t arcode[], int size, int withBorder[]);

//* setup code
void setup() {
    leds.begin();  // Call this to start up the LED strip.
    clearLEDs();   // This function, defined bellow, turns all LEDs off...
    leds.show();   // ...but the LEDs don't actually update until you call this.
    // Open serial communications and wait for port to open:
    Serial.begin(9600);

    uint32_t colorOnDisplay = 0;
    uint8_t brightnessOnDiplay = 0;
    uint8_t arucoOnDisplay[9] = {0};
    uint8_t arucoCodeSize = 0;

    loadFromEEPROM(arucoOnDisplay, &arucoCodeSize, &brightnessOnDiplay, &colorOnDisplay);
    resetLedStrip();
    leds.setBrightness(brightnessOnDiplay);
    applyAruco(arucoOnDisplay, arucoCodeSize, colorOnDisplay);

    while (!Serial)
        continue;

    // send port connection message:
    Serial.println("\n\nColAruco connection to serial port online");
    Serial.println();
}

// main code
void loop() {
    static uint32_t colorOnDisplay = 0;
    static uint8_t brightnessOnDiplay = 0;
    static uint8_t arucoOnDisplay[6] = {0};
    static uint8_t arucoCodeSize = 0;

    static bool initialized = false;
    if (!initialized) {
        loadFromEEPROM(arucoOnDisplay, &arucoCodeSize, &brightnessOnDiplay, &colorOnDisplay);
        initialized = true;
    }

    if (inputParser(&colorOnDisplay, arucoOnDisplay, &arucoCodeSize, &brightnessOnDiplay)) {
        Serial.print("Brightness value: ");
        Serial.println(brightnessOnDiplay, DEC);
        Serial.print("Color on display: ");
        Serial.println(colorOnDisplay, HEX);
        Serial.print("Aruco code size : ");
        Serial.println(arucoCodeSize);
        Serial.print("Aruco code on display (bytes): ");
        for (short i = 0; i < arucoCodeSize; i++) {
            Serial.print(arucoOnDisplay[i]);
            Serial.print(" ");
        }
        Serial.println();
        Serial.flush();

        resetLedStrip();
        leds.setBrightness(brightnessOnDiplay);
        applyAruco(arucoOnDisplay, arucoCodeSize, colorOnDisplay);
    }
}

/**
 * @brief handles input via serial port
 * 
 * @param color ptr to current color value to update
 * @param aruco ptr to current code array to update
 * @param size ptr to current size of current code array to update
 * @param brightness ptr to current brightness value to update
 * @return true if any values were updated else false
 */
bool inputParser(uint32_t *color, uint8_t aruco[], uint8_t *size, uint8_t *brightness) {
    if (Serial.available() > 0) {
        String flag = Serial.readStringUntil(' ');

        if (flag.indexOf("test") >= 0) {
            testLedStrip();
            return 1;
        }
        if (flag.indexOf("save") >= 0) {
            saveToEEPROM(aruco, *size, *brightness, *color);
            Serial.println("Current settings saved to EEPROM storage.\n");
            return 1;
        }
        if (flag.indexOf("load") >= 0) {
            loadFromEEPROM(aruco, size, brightness, color);
            Serial.println("Previous settings loaded from EEPROM storage.\n");
            return 1;
        }
        if (flag.indexOf("code") >= 0) {
            *size = (uint8_t)Serial.parseInt(SKIP_WHITESPACE);  //testar com hexadecimal
            for (short i = 0; i < *size; i++)
                aruco[i] = (uint8_t)Serial.parseInt(SKIP_WHITESPACE);
            return 1;
        }
        if (flag.indexOf("br") >= 0) {
            *brightness = (uint8_t)Serial.parseInt(SKIP_WHITESPACE);
            return 1;
        }
        if (flag.indexOf("cl") >= 0) {
            switchColor(color, Serial.readStringUntil(' '));
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 
 * 
 * @param color 
 * @param userInput 
 */
void switchColor(uint32_t *color, String userInput) {
    char AuxArray[7] = {0};
    char *ptr;

    ptr = AuxArray;
    userInput.toCharArray(AuxArray, 7);
    *color = strtoul(AuxArray, &ptr, 16);
}

/**
 * @brief 
 * 
 * @param arCode 
 * @param size 
 * @param color 
 */
void applyAruco(uint8_t arCode[], uint8_t size, uint32_t color) {
    int code[10] = {0};

    size = addBorder(arCode, size, code);

    for (short line = 0; line < size; line++) {
        if (line % 2) {
            char offset = LINE_LED_COUNT - size;  //To offset odd lines and align the matrix properly to the left

            for (short col = size - 1; col >= 0; col--) {
                uint32_t tmpColor = OFFCOL(color);
                if (code[line] & BIT(col))
                    tmpColor = color;

                leds.setPixelColor(line * LINE_LED_COUNT + col + offset, tmpColor);
                leds.show();
            }
        } else
            for (short col = 0; col < size; col++) {
                uint32_t tmpColor = OFFCOL(color);
                if (code[line] & BIT((size - col - 1)))
                    tmpColor = color;

                leds.setPixelColor(line * LINE_LED_COUNT + col, tmpColor);
                leds.show();
            }
    }
}

/**
 * @brief clears the color on display, turning off the leds (color black)
 * 
 */
void clearLEDs() {
    for (byte i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, OFF);
        leds.show();
    }
}

/**
 * @brief resets led strip
 * 
 */
void resetLedStrip() {
    leds.begin();
    clearLEDs();
    leds.show();
}

void testLedStrip() {
    resetLedStrip();

    for (byte i = 0; i < LED_COUNT; i++) {
        for (byte i = 0; i < LED_COUNT; i++) {
            leds.setPixelColor(i, OFF);
            leds.show();
        }
    }
    delay(1);

    resetLedStrip();
}

/**
 * @brief Save display settings (code, brightness and color) as a preset to memory - only one preset may be saved at a time. Saving
 *        a new one will overwrite the previous preset.
 * 
 * @param aruco aruco code to store 
 * @param brightness brigtness value to store
 * @param color color value to store
 */
void saveToEEPROM(uint8_t aruco[], uint8_t size, uint8_t brightness, uint32_t color) {
    static uint8_t slot = 0;

    EEPROM.get(EEPROM.length() - 1, slot);
    slot++;

    if (slot > SLOT_LIMIT)
        slot = 0;

    EEPROM.put(slot * SAVE_SIZE, size);

    for (int i = 0; i < CODE_SIZE; i++)
        EEPROM.put(slot * SAVE_SIZE + i + 1, aruco[i]);

    EEPROM.put(slot * SAVE_SIZE + CODE_SIZE + 1, brightness);
    EEPROM.put(slot * SAVE_SIZE + CODE_SIZE + 2, color);

    EEPROM.put(EEPROM.length() - 1, slot);
}

/**
 * @brief loads the latest preset saved to Arduino EEPROM memory using saveToEEPROM() function
 * 
 * @param aruco one dimentional array (recomended minimum size of 8) to store the loaded aruco code (array of 8 bit int)
 * @param brightness variable to store the loaded brigtness value (8 bit int)
 * @param color variable to store the loaded color value (32 bit int)
 */
void loadFromEEPROM(uint8_t aruco[], uint8_t *size, uint8_t *brightness, uint32_t *color) {
    static uint8_t slot = 0;

    EEPROM.get(EEPROM.length() - 1, slot);

    EEPROM.get(slot * SAVE_SIZE, *size);

    for (int i = 0; i < CODE_SIZE; i++)
        EEPROM.get(slot * SAVE_SIZE + i + 1, aruco[i]);

    EEPROM.get(slot * SAVE_SIZE + CODE_SIZE + 1, *brightness);
    EEPROM.get(slot * SAVE_SIZE + CODE_SIZE + 2, *color);
}

/**
 * @brief pads aruco code with an appropriate border
 * 
 * @param arcode input code
 * @param size input code size
 * @param withBorder returned code with border
 * @return int size of code with border
 */
int addBorder(uint8_t arcode[], int size, int withBorder[]) {
    size += 4;

    //primeira e última filas preenchidas com 1 em todas as posições
    int borderVal = pow(2, size);
    withBorder[0] = borderVal;
    withBorder[size - 1] = borderVal;
    //segunda e penúltima filas preenchidas com 10..(0)..01
    withBorder[1] = BIT(size - 1) + 1;
    withBorder[size - 2] = BIT(size - 1) + 1;

    //restantes filas preenchidas com o código com borda de 1 e 0
    for (short i = 2; i < size - 2; i++)
        withBorder[i] += (BIT(size - 1) + (arcode[i - 2] << 2) + 1);

    return size;
}
