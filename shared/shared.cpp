#include "shared.h"

const char* HID_GAMEPAD_AXES_NAMES[52] = {
    // Hat switch / D-pad (0-3)
    "Hat Up", "Hat Down", "Hat Left", "Hat Right",

    // Buttons 1-32 (4-35)
    "Button 1", "Button 2", "Button 3", "Button 4",
    "Button 5", "Button 6", "Button 7", "Button 8",
    "Button 9", "Button 10", "Button 11", "Button 12",
    "Button 13", "Button 14", "Button 15", "Button 16",
    "Button 17", "Button 18", "Button 19", "Button 20",
    "Button 21", "Button 22", "Button 23", "Button 24",
    "Button 25", "Button 26", "Button 27", "Button 28",
    "Button 29", "Button 30", "Button 31", "Button 32",

    // Axes - split into negative and positive directions (36-51)
    "Stick LX-", "Stick LX+", "Stick LY-", "Stick LY+",
    "Stick LZ-", "Stick LZ+", "Stick RX-", "Stick RX+",
    "Stick RY-", "Stick RY+", "Stick RZ-", "Stick RZ+",
    "Dial-", "Dial+", "Slider-", "Slider+"
};

const char* XBOX360_AXES_NAMES[26] = {
    "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right",
    "Start", "Back", "Left Stick", "Right Stick",
    "Left Bumper", "Right Bumper", "Guide", "Reserved",
    "A", "B", "X", "Y",
    "Left Trigger", "Right Trigger",
    "LX-", "LX+", "LY-", "LY+",
    "RX-", "RX+", "RY-", "RY+"
};

const char* MOUSE_AXES_NAMES[15] = {
    "",                  // Index 0 (unused - axes start from 1)
    "Button Left",       // Index 1
    "Button Right",      // Index 2
    "Button Middle",     // Index 3
    "Button Back",       // Index 4
    "Button Forward",    // Index 5
    "X Axis Left",       // Index 6
    "X Axis Right",      // Index 7
    "Y Axis Up",         // Index 8
    "Y Axis Down",       // Index 9
    "Wheel Down",        // Index 10
    "Wheel Up",          // Index 11
    "H-Wheel Left",      // Index 12
    "H-Wheel Right"      // Index 13
};

const char* KEYBOARD_AXES_NAMES[258] = {
    "KEY_UNUSED_0",      // 0: unused
    "KEY_UNUSED_1",      // 1: HID keycode 0x00
    "KEY_UNUSED_2",      // 2: HID keycode 0x01 (undefined)
    "KEY_UNUSED_3",      // 3: HID keycode 0x02 (undefined)
    "KEY_UNUSED_4",      // 4: HID keycode 0x03 (undefined)
    "KEY_A",             // 5: HID keycode 0x04
    "KEY_B",             // 6: HID keycode 0x05
    "KEY_C",             // 7: HID keycode 0x06
    "KEY_D",             // 8: HID keycode 0x07
    "KEY_E",             // 9: HID keycode 0x08
    "KEY_F",             // 10: HID keycode 0x09
    "KEY_G",             // 11: HID keycode 0x0A
    "KEY_H",             // 12: HID keycode 0x0B
    "KEY_I",             // 13: HID keycode 0x0C
    "KEY_J",             // 14: HID keycode 0x0D
    "KEY_K",             // 15: HID keycode 0x0E
    "KEY_L",             // 16: HID keycode 0x0F
    "KEY_M",             // 17: HID keycode 0x10
    "KEY_N",             // 18: HID keycode 0x11
    "KEY_O",             // 19: HID keycode 0x12
    "KEY_P",             // 20: HID keycode 0x13
    "KEY_Q",             // 21: HID keycode 0x14
    "KEY_R",             // 22: HID keycode 0x15
    "KEY_S",             // 23: HID keycode 0x16
    "KEY_T",             // 24: HID keycode 0x17
    "KEY_U",             // 25: HID keycode 0x18
    "KEY_V",             // 26: HID keycode 0x19
    "KEY_W",             // 27: HID keycode 0x1A
    "KEY_X",             // 28: HID keycode 0x1B
    "KEY_Y",             // 29: HID keycode 0x1C
    "KEY_Z",             // 30: HID keycode 0x1D
    "KEY_1",             // 31: HID keycode 0x1E
    "KEY_2",             // 32: HID keycode 0x1F
    "KEY_3",             // 33: HID keycode 0x20
    "KEY_4",             // 34: HID keycode 0x21
    "KEY_5",             // 35: HID keycode 0x22
    "KEY_6",             // 36: HID keycode 0x23
    "KEY_7",             // 37: HID keycode 0x24
    "KEY_8",             // 38: HID keycode 0x25
    "KEY_9",             // 39: HID keycode 0x26
    "KEY_0",             // 40: HID keycode 0x27
    "KEY_ENTER",         // 41: HID keycode 0x28
    "KEY_ESCAPE",        // 42: HID keycode 0x29
    "KEY_BACKSPACE",     // 43: HID keycode 0x2A
    "KEY_TAB",           // 44: HID keycode 0x2B
    "KEY_SPACE",         // 45: HID keycode 0x2C
    "KEY_MINUS",         // 46: HID keycode 0x2D
    "KEY_EQUAL",         // 47: HID keycode 0x2E
    "KEY_BRACKET_LEFT",  // 48: HID keycode 0x2F
    "KEY_BRACKET_RIGHT", // 49: HID keycode 0x30
    "KEY_BACKSLASH",     // 50: HID keycode 0x31
    "KEY_EUROPE_1",      // 51: HID keycode 0x32
    "KEY_SEMICOLON",     // 52: HID keycode 0x33
    "KEY_APOSTROPHE",    // 53: HID keycode 0x34
    "KEY_GRAVE",         // 54: HID keycode 0x35
    "KEY_COMMA",         // 55: HID keycode 0x36
    "KEY_PERIOD",        // 56: HID keycode 0x37
    "KEY_SLASH",         // 57: HID keycode 0x38
    "KEY_CAPS_LOCK",     // 58: HID keycode 0x39
    "KEY_F1",            // 59: HID keycode 0x3A
    "KEY_F2",            // 60: HID keycode 0x3B
    "KEY_F3",            // 61: HID keycode 0x3C
    "KEY_F4",            // 62: HID keycode 0x3D
    "KEY_F5",            // 63: HID keycode 0x3E
    "KEY_F6",            // 64: HID keycode 0x3F
    "KEY_F7",            // 65: HID keycode 0x40
    "KEY_F8",            // 66: HID keycode 0x41
    "KEY_F9",            // 67: HID keycode 0x42
    "KEY_F10",           // 68: HID keycode 0x43
    "KEY_F11",           // 69: HID keycode 0x44
    "KEY_F12",           // 70: HID keycode 0x45
    "KEY_PRINT_SCREEN",  // 71: HID keycode 0x46
    "KEY_SCROLL_LOCK",   // 72: HID keycode 0x47
    "KEY_PAUSE",         // 73: HID keycode 0x48
    "KEY_INSERT",        // 74: HID keycode 0x49
    "KEY_HOME",          // 75: HID keycode 0x4A
    "KEY_PAGE_UP",       // 76: HID keycode 0x4B
    "KEY_DELETE",        // 77: HID keycode 0x4C
    "KEY_END",           // 78: HID keycode 0x4D
    "KEY_PAGE_DOWN",     // 79: HID keycode 0x4E
    "KEY_ARROW_RIGHT",   // 80: HID keycode 0x4F
    "KEY_ARROW_LEFT",    // 81: HID keycode 0x50
    "KEY_ARROW_DOWN",    // 82: HID keycode 0x51
    "KEY_ARROW_UP",      // 83: HID keycode 0x52
    "KEY_NUM_LOCK",      // 84: HID keycode 0x53
    "KEY_KEYPAD_DIVIDE", // 85: HID keycode 0x54
    "KEY_KEYPAD_MULTIPLY",// 86: HID keycode 0x55
    "KEY_KEYPAD_SUBTRACT",// 87: HID keycode 0x56
    "KEY_KEYPAD_ADD",    // 88: HID keycode 0x57
    "KEY_KEYPAD_ENTER",  // 89: HID keycode 0x58
    "KEY_KEYPAD_1",      // 90: HID keycode 0x59
    "KEY_KEYPAD_2",      // 91: HID keycode 0x5A
    "KEY_KEYPAD_3",      // 92: HID keycode 0x5B
    "KEY_KEYPAD_4",      // 93: HID keycode 0x5C
    "KEY_KEYPAD_5",      // 94: HID keycode 0x5D
    "KEY_KEYPAD_6",      // 95: HID keycode 0x5E
    "KEY_KEYPAD_7",      // 96: HID keycode 0x5F
    "KEY_KEYPAD_8",      // 97: HID keycode 0x60
    "KEY_KEYPAD_9",      // 98: HID keycode 0x61
    "KEY_KEYPAD_0",      // 99: HID keycode 0x62
    "KEY_KEYPAD_DECIMAL",// 100: HID keycode 0x63
    "KEY_EUROPE_2",      // 101: HID keycode 0x64
    "KEY_APPLICATION",   // 102: HID keycode 0x65
    "KEY_POWER",         // 103: HID keycode 0x66
    "KEY_KEYPAD_EQUAL",  // 104: HID keycode 0x67
    "KEY_F13",           // 105: HID keycode 0x68
    "KEY_F14",           // 106: HID keycode 0x69
    "KEY_F15",           // 107: HID keycode 0x6A
    "KEY_F16",           // 108: HID keycode 0x6B
    "KEY_F17",           // 109: HID keycode 0x6C
    "KEY_F18",           // 110: HID keycode 0x6D
    "KEY_F19",           // 111: HID keycode 0x6E
    "KEY_F20",           // 112: HID keycode 0x6F
    "KEY_F21",           // 113: HID keycode 0x70
    "KEY_F22",           // 114: HID keycode 0x71
    "KEY_F23",           // 115: HID keycode 0x72
    "KEY_F24",           // 116: HID keycode 0x73
    "KEY_EXECUTE",       // 117: HID keycode 0x74
    "KEY_HELP",          // 118: HID keycode 0x75
    "KEY_MENU",          // 119: HID keycode 0x76
    "KEY_SELECT",        // 120: HID keycode 0x77
    "KEY_STOP",          // 121: HID keycode 0x78
    "KEY_AGAIN",         // 122: HID keycode 0x79
    "KEY_UNDO",          // 123: HID keycode 0x7A
    "KEY_CUT",           // 124: HID keycode 0x7B
    "KEY_COPY",          // 125: HID keycode 0x7C
    "KEY_PASTE",         // 126: HID keycode 0x7D
    "KEY_FIND",          // 127: HID keycode 0x7E
    "KEY_MUTE",          // 128: HID keycode 0x7F
    "KEY_VOLUME_UP",     // 129: HID keycode 0x80
    "KEY_VOLUME_DOWN",   // 130: HID keycode 0x81
    "KEY_LOCKING_CAPS_LOCK",   // 131: HID keycode 0x82
    "KEY_LOCKING_NUM_LOCK",    // 132: HID keycode 0x83
    "KEY_LOCKING_SCROLL_LOCK", // 133: HID keycode 0x84
    "KEY_KEYPAD_COMMA",        // 134: HID keycode 0x85
    "KEY_KEYPAD_EQUAL_SIGN",   // 135: HID keycode 0x86
    "KEY_KANJI1",        // 136: HID keycode 0x87
    "KEY_KANJI2",        // 137: HID keycode 0x88
    "KEY_KANJI3",        // 138: HID keycode 0x89
    "KEY_KANJI4",        // 139: HID keycode 0x8A
    "KEY_KANJI5",        // 140: HID keycode 0x8B
    "KEY_KANJI6",        // 141: HID keycode 0x8C
    "KEY_KANJI7",        // 142: HID keycode 0x8D
    "KEY_KANJI8",        // 143: HID keycode 0x8E
    "KEY_KANJI9",        // 144: HID keycode 0x8F
    "KEY_LANG1",         // 145: HID keycode 0x90
    "KEY_LANG2",         // 146: HID keycode 0x91
    "KEY_LANG3",         // 147: HID keycode 0x92
    "KEY_LANG4",         // 148: HID keycode 0x93
    "KEY_LANG5",         // 149: HID keycode 0x94
    "KEY_LANG6",         // 150: HID keycode 0x95
    "KEY_LANG7",         // 151: HID keycode 0x96
    "KEY_LANG8",         // 152: HID keycode 0x97
    "KEY_LANG9",         // 153: HID keycode 0x98
    "KEY_ALTERNATE_ERASE",  // 154: HID keycode 0x99
    "KEY_SYSREQ_ATTENTION", // 155: HID keycode 0x9A
    "KEY_CANCEL",        // 156: HID keycode 0x9B
    "KEY_CLEAR",         // 157: HID keycode 0x9C
    "KEY_PRIOR",         // 158: HID keycode 0x9D
    "KEY_RETURN",        // 159: HID keycode 0x9E
    "KEY_SEPARATOR",     // 160: HID keycode 0x9F
    "KEY_OUT",           // 161: HID keycode 0xA0
    "KEY_OPER",          // 162: HID keycode 0xA1
    "KEY_CLEAR_AGAIN",   // 163: HID keycode 0xA2
    "KEY_CRSEL_PROPS",   // 164: HID keycode 0xA3
    "KEY_EXSEL",         // 165: HID keycode 0xA4
    "KEY_UNUSED_165", "KEY_UNUSED_166", "KEY_UNUSED_167", "KEY_UNUSED_168", "KEY_UNUSED_169", "KEY_UNUSED_170", "KEY_UNUSED_171", // 166-172: HID keycodes 0xA5-0xAB (undefined)
    "KEY_UNUSED_172", "KEY_UNUSED_173", "KEY_UNUSED_174", "KEY_UNUSED_175", // 173-176: HID keycodes 0xAC-0xAF (undefined)
    "KEY_KEYPAD_00",     // 177: HID keycode 0xB0
    "KEY_KEYPAD_000",    // 178: HID keycode 0xB1
    "KEY_THOUSANDS_SEPARATOR", // 179: HID keycode 0xB2
    "KEY_DECIMAL_SEPARATOR",   // 180: HID keycode 0xB3
    "KEY_CURRENCY_UNIT",       // 181: HID keycode 0xB4
    "KEY_CURRENCY_SUBUNIT",    // 182: HID keycode 0xB5
    "KEY_KEYPAD_LEFT_PARENTHESIS",  // 183: HID keycode 0xB6
    "KEY_KEYPAD_RIGHT_PARENTHESIS", // 184: HID keycode 0xB7
    "KEY_KEYPAD_LEFT_BRACE",        // 185: HID keycode 0xB8
    "KEY_KEYPAD_RIGHT_BRACE",       // 186: HID keycode 0xB9
    "KEY_KEYPAD_TAB",               // 187: HID keycode 0xBA
    "KEY_KEYPAD_BACKSPACE",         // 188: HID keycode 0xBB
    "KEY_KEYPAD_A",                 // 189: HID keycode 0xBC
    "KEY_KEYPAD_B",                 // 190: HID keycode 0xBD
    "KEY_KEYPAD_C",                 // 191: HID keycode 0xBE
    "KEY_KEYPAD_D",                 // 192: HID keycode 0xBF
    "KEY_KEYPAD_E",                 // 193: HID keycode 0xC0
    "KEY_KEYPAD_F",                 // 194: HID keycode 0xC1
    "KEY_KEYPAD_XOR",               // 195: HID keycode 0xC2
    "KEY_KEYPAD_CARET",             // 196: HID keycode 0xC3
    "KEY_KEYPAD_PERCENT",           // 197: HID keycode 0xC4
    "KEY_KEYPAD_LESS_THAN",         // 198: HID keycode 0xC5
    "KEY_KEYPAD_GREATER_THAN",      // 199: HID keycode 0xC6
    "KEY_KEYPAD_AMPERSAND",         // 200: HID keycode 0xC7
    "KEY_KEYPAD_DOUBLE_AMPERSAND",  // 201: HID keycode 0xC8
    "KEY_KEYPAD_VERTICAL_BAR",      // 202: HID keycode 0xC9
    "KEY_KEYPAD_DOUBLE_VERTICAL_BAR", // 203: HID keycode 0xCA
    "KEY_KEYPAD_COLON",  // 204: HID keycode 0xCB
    "KEY_KEYPAD_HASH",   // 205: HID keycode 0xCC
    "KEY_KEYPAD_SPACE",  // 206: HID keycode 0xCD
    "KEY_KEYPAD_AT",     // 207: HID keycode 0xCE
    "KEY_KEYPAD_EXCLAMATION",     // 208: HID keycode 0xCF
    "KEY_KEYPAD_MEMORY_STORE",    // 209: HID keycode 0xD0
    "KEY_KEYPAD_MEMORY_RECALL",   // 210: HID keycode 0xD1
    "KEY_KEYPAD_MEMORY_CLEAR",    // 211: HID keycode 0xD2
    "KEY_KEYPAD_MEMORY_ADD",      // 212: HID keycode 0xD3
    "KEY_KEYPAD_MEMORY_SUBTRACT", // 213: HID keycode 0xD4
    "KEY_KEYPAD_MEMORY_MULTIPLY", // 214: HID keycode 0xD5
    "KEY_KEYPAD_MEMORY_DIVIDE",   // 215: HID keycode 0xD6
    "KEY_KEYPAD_PLUS_MINUS",      // 216: HID keycode 0xD7
    "KEY_KEYPAD_CLEAR",           // 217: HID keycode 0xD8
    "KEY_KEYPAD_CLEAR_ENTRY",     // 218: HID keycode 0xD9
    "KEY_KEYPAD_BINARY",          // 219: HID keycode 0xDA
    "KEY_KEYPAD_OCTAL",           // 220: HID keycode 0xDB
    "KEY_KEYPAD_DECIMAL_2",       // 221: HID keycode 0xDC
    "KEY_KEYPAD_HEXADECIMAL",     // 222: HID keycode 0xDD
    "KEY_UNUSED_222", "KEY_UNUSED_223", // 223-224: HID keycodes 0xDE-0xDF (undefined)
    "KEY_CONTROL_LEFT",           // 225: HID keycode 0xE0
    "KEY_SHIFT_LEFT",             // 226: HID keycode 0xE1
    "KEY_ALT_LEFT",               // 227: HID keycode 0xE2
    "KEY_GUI_LEFT",               // 228: HID keycode 0xE3
    "KEY_CONTROL_RIGHT",          // 229: HID keycode 0xE4
    "KEY_SHIFT_RIGHT",            // 230: HID keycode 0xE5
    "KEY_ALT_RIGHT",              // 231: HID keycode 0xE6
    "KEY_GUI_RIGHT",              // 232: HID keycode 0xE7
    "KEY_UNUSED_232", "KEY_UNUSED_233", "KEY_UNUSED_234", "KEY_UNUSED_235", "KEY_UNUSED_236", "KEY_UNUSED_237", "KEY_UNUSED_238", "KEY_UNUSED_239", // 233-240: HID keycodes 0xE8-0xEF (undefined)
    "KEY_UNUSED_240", "KEY_UNUSED_241", "KEY_UNUSED_242", "KEY_UNUSED_243", "KEY_UNUSED_244", "KEY_UNUSED_245", "KEY_UNUSED_246", "KEY_UNUSED_247", // 241-248: HID keycodes 0xF0-0xF7 (undefined)
    "KEY_UNUSED_248", "KEY_UNUSED_249", "KEY_UNUSED_250", "KEY_UNUSED_251", "KEY_UNUSED_252", "KEY_UNUSED_253", "KEY_UNUSED_254", "KEY_UNUSED_255"  // 249-256: HID keycodes 0xF8-0xFF (undefined)
};