#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

enum XBOX360_AXES {
    XBOX_BTN_DPAD_UP = 0,
    XBOX_BTN_DPAD_DOWN = 1,
    XBOX_BTN_DPAD_LEFT = 2,
    XBOX_BTN_DPAD_RIGHT = 3,
    XBOX_BTN_START = 4,
    XBOX_BTN_BACK = 5,
    XBOX_BTN_LEFT_STICK = 6,        // Left stick click
    XBOX_BTN_RIGHT_STICK = 7,       // Right stick click
    XBOX_BTN_LEFT_BUMPER = 8,       // Left bumper
    XBOX_BTN_RIGHT_BUMPER = 9,      // Right bumper
    XBOX_BTN_GUIDE = 10,            // Home/Guide button
    XBOX_BTN_A = 12,
    XBOX_BTN_B = 13,
    XBOX_BTN_X = 14,
    XBOX_BTN_Y = 15,

    // Triggers (0-1000)
    XBOX_AXIS_LEFT_TRIGGER = 16,    // Left trigger
    XBOX_AXIS_RIGHT_TRIGGER = 17,   // Right trigger

    // Split axes - separate controls for each direction (0-1000 each)
    XBOX_AXIS_LX_MINUS = 18,   // Left stick X negative (left)
    XBOX_AXIS_LX_PLUS = 19,    // Left stick X positive (right)
    XBOX_AXIS_LY_MINUS = 20,   // Left stick Y negative (down)
    XBOX_AXIS_LY_PLUS = 21,    // Left stick Y positive (up)
    XBOX_AXIS_RX_MINUS = 22,   // Right stick X negative (left)
    XBOX_AXIS_RX_PLUS = 23,    // Right stick X positive (right)
    XBOX_AXIS_RY_MINUS = 24,   // Right stick Y negative (down)
    XBOX_AXIS_RY_PLUS = 25,    // Right stick Y positive (up)

    // Rumble event codes (for event callback)
    XBOX_RUMBLE_LEFT = 100,    // Left motor (large) - value 0-255
    XBOX_RUMBLE_RIGHT = 101    // Right motor (small) - value 0-255
};

enum MOUSE_AXES {
    MOUSE_AXIS_BUTTON_LEFT = 1,
    MOUSE_AXIS_BUTTON_RIGHT = 2,
    MOUSE_AXIS_BUTTON_MIDDLE = 3,
    MOUSE_AXIS_BUTTON_BACK = 4,
    MOUSE_AXIS_BUTTON_FORWARD = 5,
    MOUSE_AXIS_X_MINUS = 6,      // X Left
    MOUSE_AXIS_X_PLUS = 7,       // X Right
    MOUSE_AXIS_Y_MINUS = 8,      // Y Up
    MOUSE_AXIS_Y_PLUS = 9,       // Y Down
    MOUSE_AXIS_WHEEL_MINUS = 10,  // Wheel Down
    MOUSE_AXIS_WHEEL_PLUS = 11,   // Wheel Up
    MOUSE_AXIS_H_WHEEL_MINUS = 12, // H-Wheel Left
    MOUSE_AXIS_H_WHEEL_PLUS = 13   // H-Wheel Right
};

// Gamepad axis bitmask flags (for HID descriptor configuration)
#define FLAG_MASK_GAMEPAD_AXIS_LX     0x01  // Bit 0: Left stick X
#define FLAG_MASK_GAMEPAD_AXIS_LY     0x02  // Bit 1: Left stick Y
#define FLAG_MASK_GAMEPAD_AXIS_LZ     0x04  // Bit 2: Left stick Z
#define FLAG_MASK_GAMEPAD_AXIS_RX     0x08  // Bit 3: Right stick X
#define FLAG_MASK_GAMEPAD_AXIS_RY     0x10  // Bit 4: Right stick Y
#define FLAG_MASK_GAMEPAD_AXIS_RZ     0x20  // Bit 5: Right stick Z
#define FLAG_MASK_GAMEPAD_AXIS_DIAL   0x40  // Bit 6: Dial
#define FLAG_MASK_GAMEPAD_AXIS_SLIDER 0x80  // Bit 7: Slider

enum HID_GAMEPAD_AXES {
    // Hat switch / D-pad (0-3)
    GAMEPAD_AXIS_HAT_UP = 0,
    GAMEPAD_AXIS_HAT_DOWN = 1,
    GAMEPAD_AXIS_HAT_LEFT = 2,
    GAMEPAD_AXIS_HAT_RIGHT = 3,

    // Buttons 1-32 (4-35)
    GAMEPAD_BTN_1 = 4,
    GAMEPAD_BTN_2 = 5,
    GAMEPAD_BTN_3 = 6,
    GAMEPAD_BTN_4 = 7,
    GAMEPAD_BTN_5 = 8,
    GAMEPAD_BTN_6 = 9,
    GAMEPAD_BTN_7 = 10,
    GAMEPAD_BTN_8 = 11,
    GAMEPAD_BTN_9 = 12,
    GAMEPAD_BTN_10 = 13,
    GAMEPAD_BTN_11 = 14,
    GAMEPAD_BTN_12 = 15,
    GAMEPAD_BTN_13 = 16,
    GAMEPAD_BTN_14 = 17,
    GAMEPAD_BTN_15 = 18,
    GAMEPAD_BTN_16 = 19,
    GAMEPAD_BTN_17 = 20,
    GAMEPAD_BTN_18 = 21,
    GAMEPAD_BTN_19 = 22,
    GAMEPAD_BTN_20 = 23,
    GAMEPAD_BTN_21 = 24,
    GAMEPAD_BTN_22 = 25,
    GAMEPAD_BTN_23 = 26,
    GAMEPAD_BTN_24 = 27,
    GAMEPAD_BTN_25 = 28,
    GAMEPAD_BTN_26 = 29,
    GAMEPAD_BTN_27 = 30,
    GAMEPAD_BTN_28 = 31,
    GAMEPAD_BTN_29 = 32,
    GAMEPAD_BTN_30 = 33,
    GAMEPAD_BTN_31 = 34,
    GAMEPAD_BTN_32 = 35,

    // Axes - split into negative and positive directions (36-51)
    GAMEPAD_AXIS_LX_MINUS = 36,    // Left stick X negative (left)
    GAMEPAD_AXIS_LX_PLUS = 37,     // Left stick X positive (right)
    GAMEPAD_AXIS_LY_MINUS = 38,    // Left stick Y negative (down)
    GAMEPAD_AXIS_LY_PLUS = 39,     // Left stick Y positive (up)
    GAMEPAD_AXIS_LZ_MINUS = 40,    // Left stick Z negative
    GAMEPAD_AXIS_LZ_PLUS = 41,     // Left stick Z positive
    GAMEPAD_AXIS_RX_MINUS = 42,    // Right stick X negative (left)
    GAMEPAD_AXIS_RX_PLUS = 43,     // Right stick X positive (right)
    GAMEPAD_AXIS_RY_MINUS = 44,    // Right stick Y negative (down)
    GAMEPAD_AXIS_RY_PLUS = 45,     // Right stick Y positive (up)
    GAMEPAD_AXIS_RZ_MINUS = 46,    // Right stick Z negative
    GAMEPAD_AXIS_RZ_PLUS = 47,     // Right stick Z positive
    GAMEPAD_AXIS_DIAL_MINUS = 48,  // Dial negative
    GAMEPAD_AXIS_DIAL_PLUS = 49,   // Dial positive
    GAMEPAD_AXIS_SLIDER_MINUS = 50,// Slider negative
    GAMEPAD_AXIS_SLIDER_PLUS = 51  // Slider positive
};

enum KEYBOARD_AXES {
    KEY_UNUSED_0 = 0,
    KEY_UNUSED_1 = 1,
    KEY_UNUSED_2 = 2,
    KEY_UNUSED_3 = 3,
    KEY_UNUSED_4 = 4,
    KEY_A = 5,
    KEY_B = 6,
    KEY_C = 7,
    KEY_D = 8,
    KEY_E = 9,
    KEY_F = 10,
    KEY_G = 11,
    KEY_H = 12,
    KEY_I = 13,
    KEY_J = 14,
    KEY_K = 15,
    KEY_L = 16,
    KEY_M = 17,
    KEY_N = 18,
    KEY_O = 19,
    KEY_P = 20,
    KEY_Q = 21,
    KEY_R = 22,
    KEY_S = 23,
    KEY_T = 24,
    KEY_U = 25,
    KEY_V = 26,
    KEY_W = 27,
    KEY_X = 28,
    KEY_Y = 29,
    KEY_Z = 30,
    KEY_1 = 31,
    KEY_2 = 32,
    KEY_3 = 33,
    KEY_4 = 34,
    KEY_5 = 35,
    KEY_6 = 36,
    KEY_7 = 37,
    KEY_8 = 38,
    KEY_9 = 39,
    KEY_0 = 40,
    KEY_ENTER = 41,
    KEY_ESCAPE = 42,
    KEY_BACKSPACE = 43,
    KEY_TAB = 44,
    KEY_SPACE = 45,
    KEY_MINUS = 46,
    KEY_EQUAL = 47,
    KEY_BRACKET_LEFT = 48,
    KEY_BRACKET_RIGHT = 49,
    KEY_BACKSLASH = 50,
    KEY_EUROPE_1 = 51,
    KEY_SEMICOLON = 52,
    KEY_APOSTROPHE = 53,
    KEY_GRAVE = 54,
    KEY_COMMA = 55,
    KEY_PERIOD = 56,
    KEY_SLASH = 57,
    KEY_CAPS_LOCK = 58,
    KEY_F1 = 59,
    KEY_F2 = 60,
    KEY_F3 = 61,
    KEY_F4 = 62,
    KEY_F5 = 63,
    KEY_F6 = 64,
    KEY_F7 = 65,
    KEY_F8 = 66,
    KEY_F9 = 67,
    KEY_F10 = 68,
    KEY_F11 = 69,
    KEY_F12 = 70,
    KEY_PRINT_SCREEN = 71,
    KEY_SCROLL_LOCK = 72,
    KEY_PAUSE = 73,
    KEY_INSERT = 74,
    KEY_HOME = 75,
    KEY_PAGE_UP = 76,
    KEY_DELETE = 77,
    KEY_END = 78,
    KEY_PAGE_DOWN = 79,
    KEY_ARROW_RIGHT = 80,
    KEY_ARROW_LEFT = 81,
    KEY_ARROW_DOWN = 82,
    KEY_ARROW_UP = 83,
    KEY_NUM_LOCK = 84,
    KEY_KEYPAD_DIVIDE = 85,
    KEY_KEYPAD_MULTIPLY = 86,
    KEY_KEYPAD_SUBTRACT = 87,
    KEY_KEYPAD_ADD = 88,
    KEY_KEYPAD_ENTER = 89,
    KEY_KEYPAD_1 = 90,
    KEY_KEYPAD_2 = 91,
    KEY_KEYPAD_3 = 92,
    KEY_KEYPAD_4 = 93,
    KEY_KEYPAD_5 = 94,
    KEY_KEYPAD_6 = 95,
    KEY_KEYPAD_7 = 96,
    KEY_KEYPAD_8 = 97,
    KEY_KEYPAD_9 = 98,
    KEY_KEYPAD_0 = 99,
    KEY_KEYPAD_DECIMAL = 100,
    KEY_EUROPE_2 = 101,
    KEY_APPLICATION = 102,
    KEY_POWER = 103,
    KEY_KEYPAD_EQUAL = 104,
    KEY_F13 = 105,
    KEY_F14 = 106,
    KEY_F15 = 107,
    KEY_F16 = 108,
    KEY_F17 = 109,
    KEY_F18 = 110,
    KEY_F19 = 111,
    KEY_F20 = 112,
    KEY_F21 = 113,
    KEY_F22 = 114,
    KEY_F23 = 115,
    KEY_F24 = 116,
    KEY_EXECUTE = 117,
    KEY_HELP = 118,
    KEY_MENU = 119,
    KEY_SELECT = 120,
    KEY_STOP = 121,
    KEY_AGAIN = 122,
    KEY_UNDO = 123,
    KEY_CUT = 124,
    KEY_COPY = 125,
    KEY_PASTE = 126,
    KEY_FIND = 127,
    KEY_MUTE = 128,
    KEY_VOLUME_UP = 129,
    KEY_VOLUME_DOWN = 130,
    KEY_LOCKING_CAPS_LOCK = 131,
    KEY_LOCKING_NUM_LOCK = 132,
    KEY_LOCKING_SCROLL_LOCK = 133,
    KEY_KEYPAD_COMMA = 134,
    KEY_KEYPAD_EQUAL_SIGN = 135,
    KEY_KANJI1 = 136,
    KEY_KANJI2 = 137,
    KEY_KANJI3 = 138,
    KEY_KANJI4 = 139,
    KEY_KANJI5 = 140,
    KEY_KANJI6 = 141,
    KEY_KANJI7 = 142,
    KEY_KANJI8 = 143,
    KEY_KANJI9 = 144,
    KEY_LANG1 = 145,
    KEY_LANG2 = 146,
    KEY_LANG3 = 147,
    KEY_LANG4 = 148,
    KEY_LANG5 = 149,
    KEY_LANG6 = 150,
    KEY_LANG7 = 151,
    KEY_LANG8 = 152,
    KEY_LANG9 = 153,
    KEY_ALTERNATE_ERASE = 154,
    KEY_SYSREQ_ATTENTION = 155,
    KEY_CANCEL = 156,
    KEY_CLEAR = 157,
    KEY_PRIOR = 158,
    KEY_RETURN = 159,
    KEY_SEPARATOR = 160,
    KEY_OUT = 161,
    KEY_OPER = 162,
    KEY_CLEAR_AGAIN = 163,
    KEY_CRSEL_PROPS = 164,
    KEY_EXSEL = 165,
    KEY_UNUSED_165 = 166,
    KEY_UNUSED_166 = 167,
    KEY_UNUSED_167 = 168,
    KEY_UNUSED_168 = 169,
    KEY_UNUSED_169 = 170,
    KEY_UNUSED_170 = 171,
    KEY_UNUSED_171 = 172,
    KEY_UNUSED_172 = 173,
    KEY_UNUSED_173 = 174,
    KEY_UNUSED_174 = 175,
    KEY_UNUSED_175 = 176,
    KEY_KEYPAD_00 = 177,
    KEY_KEYPAD_000 = 178,
    KEY_THOUSANDS_SEPARATOR = 179,
    KEY_DECIMAL_SEPARATOR = 180,
    KEY_CURRENCY_UNIT = 181,
    KEY_CURRENCY_SUBUNIT = 182,
    KEY_KEYPAD_LEFT_PARENTHESIS = 183,
    KEY_KEYPAD_RIGHT_PARENTHESIS = 184,
    KEY_KEYPAD_LEFT_BRACE = 185,
    KEY_KEYPAD_RIGHT_BRACE = 186,
    KEY_KEYPAD_TAB = 187,
    KEY_KEYPAD_BACKSPACE = 188,
    KEY_KEYPAD_A = 189,
    KEY_KEYPAD_B = 190,
    KEY_KEYPAD_C = 191,
    KEY_KEYPAD_D = 192,
    KEY_KEYPAD_E = 193,
    KEY_KEYPAD_F = 194,
    KEY_KEYPAD_XOR = 195,
    KEY_KEYPAD_CARET = 196,
    KEY_KEYPAD_PERCENT = 197,
    KEY_KEYPAD_LESS_THAN = 198,
    KEY_KEYPAD_GREATER_THAN = 199,
    KEY_KEYPAD_AMPERSAND = 200,
    KEY_KEYPAD_DOUBLE_AMPERSAND = 201,
    KEY_KEYPAD_VERTICAL_BAR = 202,
    KEY_KEYPAD_DOUBLE_VERTICAL_BAR = 203,
    KEY_KEYPAD_COLON = 204,
    KEY_KEYPAD_HASH = 205,
    KEY_KEYPAD_SPACE = 206,
    KEY_KEYPAD_AT = 207,
    KEY_KEYPAD_EXCLAMATION = 208,
    KEY_KEYPAD_MEMORY_STORE = 209,
    KEY_KEYPAD_MEMORY_RECALL = 210,
    KEY_KEYPAD_MEMORY_CLEAR = 211,
    KEY_KEYPAD_MEMORY_ADD = 212,
    KEY_KEYPAD_MEMORY_SUBTRACT = 213,
    KEY_KEYPAD_MEMORY_MULTIPLY = 214,
    KEY_KEYPAD_MEMORY_DIVIDE = 215,
    KEY_KEYPAD_PLUS_MINUS = 216,
    KEY_KEYPAD_CLEAR = 217,
    KEY_KEYPAD_CLEAR_ENTRY = 218,
    KEY_KEYPAD_BINARY = 219,
    KEY_KEYPAD_OCTAL = 220,
    KEY_KEYPAD_DECIMAL_2 = 221,
    KEY_KEYPAD_HEXADECIMAL = 222,
    KEY_UNUSED_222 = 223,
    KEY_UNUSED_223 = 224,
    KEY_CONTROL_LEFT = 225,
    KEY_SHIFT_LEFT = 226,
    KEY_ALT_LEFT = 227,
    KEY_GUI_LEFT = 228,
    KEY_CONTROL_RIGHT = 229,
    KEY_SHIFT_RIGHT = 230,
    KEY_ALT_RIGHT = 231,
    KEY_GUI_RIGHT = 232,
    KEY_UNUSED_232 = 233,
    KEY_UNUSED_233 = 234,
    KEY_UNUSED_234 = 235,
    KEY_UNUSED_235 = 236,
    KEY_UNUSED_236 = 237,
    KEY_UNUSED_237 = 238,
    KEY_UNUSED_238 = 239,
    KEY_UNUSED_239 = 240,
    KEY_UNUSED_240 = 241,
    KEY_UNUSED_241 = 242,
    KEY_UNUSED_242 = 243,
    KEY_UNUSED_243 = 244,
    KEY_UNUSED_244 = 245,
    KEY_UNUSED_245 = 246,
    KEY_UNUSED_246 = 247,
    KEY_UNUSED_247 = 248,
    KEY_UNUSED_248 = 249,
    KEY_UNUSED_249 = 250,
    KEY_UNUSED_250 = 251,
    KEY_UNUSED_251 = 252,
    KEY_UNUSED_252 = 253,
    KEY_UNUSED_253 = 254,
    KEY_UNUSED_254 = 255,
    KEY_UNUSED_255 = 256
};

extern const char* HID_GAMEPAD_AXES_NAMES[52];
extern const char* XBOX360_AXES_NAMES[26];
extern const char* MOUSE_AXES_NAMES[15];
extern const char* KEYBOARD_AXES_NAMES[258];

#ifdef __cplusplus
#include <string>
#include <vector>
#include <map>

// A single axis entry: human-readable name and its integer index (from the enums above)
struct AxisEntry {
    std::string name;
    int index;
};

// Bidirectional name<->index table for a device type.
// For emulated devices it is built from the shared *_NAMES arrays.
// For real devices it is built dynamically from evdev capabilities.
class AxisTable {
public:
    void addEntry(const std::string& name, int index);

    // Returns -1 if name not found
    int getIndex(const std::string& name) const;

    // Returns empty string if index not found
    std::string getName(int index) const;

    bool hasName(const std::string& name) const;
    bool hasIndex(int index) const;

    const std::vector<AxisEntry>& getEntries() const { return entries; }

    // Build static tables from the shared *_NAMES arrays
    static AxisTable forHidGamepad();
    static AxisTable forXbox360();
    static AxisTable forMouse();
    static AxisTable forKeyboard();

private:
    std::vector<AxisEntry> entries;
    std::map<std::string, int> nameToIndex;
    std::map<int, std::string> indexToName;
};
#endif // __cplusplus

#endif // SHARED_UTILS_H