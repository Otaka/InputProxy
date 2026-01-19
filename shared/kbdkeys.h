#ifndef KBDKEYS_H
#define KBDKEYS_H

#include <stdint.h>

// TinyUSB HID key codes are defined in class/hid/hid.h
// We'll reference them here for convenience

// Key definition structure with code and string representation
struct KeyDef {
    unsigned short code;           // HID key code
    const char* name;       // String representation
};

// Macro to create a KeyDef from a HID_KEY constant
// Usage: K_KEY(HID_KEY_A) expands to {HID_KEY_A, "A"}
#define K_KEY(constant) { constant, #constant }

// Helper macro to extract just the key name without HID_KEY_ prefix
// Usage: K_KEY_N(HID_KEY_A) expands to {HID_KEY_A, "A"}
#define K_KEY_N(constant) { constant, #constant + 8 }  // Skip "HID_KEY_" (8 chars)

// Common HID Key Codes (from TinyUSB)
// Alphanumeric Keys
#define HID_KEY_NONE                        0x00
#define HID_KEY_A                           0x04
#define HID_KEY_B                           0x05
#define HID_KEY_C                           0x06
#define HID_KEY_D                           0x07
#define HID_KEY_E                           0x08
#define HID_KEY_F                           0x09
#define HID_KEY_G                           0x0A
#define HID_KEY_H                           0x0B
#define HID_KEY_I                           0x0C
#define HID_KEY_J                           0x0D
#define HID_KEY_K                           0x0E
#define HID_KEY_L                           0x0F
#define HID_KEY_M                           0x10
#define HID_KEY_N                           0x11
#define HID_KEY_O                           0x12
#define HID_KEY_P                           0x13
#define HID_KEY_Q                           0x14
#define HID_KEY_R                           0x15
#define HID_KEY_S                           0x16
#define HID_KEY_T                           0x17
#define HID_KEY_U                           0x18
#define HID_KEY_V                           0x19
#define HID_KEY_W                           0x1A
#define HID_KEY_X                           0x1B
#define HID_KEY_Y                           0x1C
#define HID_KEY_Z                           0x1D

// Number Keys
#define HID_KEY_1                           0x1E
#define HID_KEY_2                           0x1F
#define HID_KEY_3                           0x20
#define HID_KEY_4                           0x21
#define HID_KEY_5                           0x22
#define HID_KEY_6                           0x23
#define HID_KEY_7                           0x24
#define HID_KEY_8                           0x25
#define HID_KEY_9                           0x26
#define HID_KEY_0                           0x27

// Special Keys
#define HID_KEY_ENTER                       0x28
#define HID_KEY_ESCAPE                      0x29
#define HID_KEY_BACKSPACE                   0x2A
#define HID_KEY_TAB                         0x2B
#define HID_KEY_SPACE                       0x2C
#define HID_KEY_MINUS                       0x2D
#define HID_KEY_EQUAL                       0x2E
#define HID_KEY_BRACKET_LEFT                0x2F
#define HID_KEY_BRACKET_RIGHT               0x30
#define HID_KEY_BACKSLASH                   0x31
#define HID_KEY_EUROPE_1                    0x32
#define HID_KEY_SEMICOLON                   0x33
#define HID_KEY_APOSTROPHE                  0x34
#define HID_KEY_GRAVE                       0x35
#define HID_KEY_COMMA                       0x36
#define HID_KEY_PERIOD                      0x37
#define HID_KEY_SLASH                       0x38
#define HID_KEY_CAPS_LOCK                   0x39

// Function Keys
#define HID_KEY_F1                          0x3A
#define HID_KEY_F2                          0x3B
#define HID_KEY_F3                          0x3C
#define HID_KEY_F4                          0x3D
#define HID_KEY_F5                          0x3E
#define HID_KEY_F6                          0x3F
#define HID_KEY_F7                          0x40
#define HID_KEY_F8                          0x41
#define HID_KEY_F9                          0x42
#define HID_KEY_F10                         0x43
#define HID_KEY_F11                         0x44
#define HID_KEY_F12                         0x45

// Navigation Keys
#define HID_KEY_PRINT_SCREEN                0x46
#define HID_KEY_SCROLL_LOCK                 0x47
#define HID_KEY_PAUSE                       0x48
#define HID_KEY_INSERT                      0x49
#define HID_KEY_HOME                        0x4A
#define HID_KEY_PAGE_UP                     0x4B
#define HID_KEY_DELETE                      0x4C
#define HID_KEY_END                         0x4D
#define HID_KEY_PAGE_DOWN                   0x4E
#define HID_KEY_ARROW_RIGHT                 0x4F
#define HID_KEY_ARROW_LEFT                  0x50
#define HID_KEY_ARROW_DOWN                  0x51
#define HID_KEY_ARROW_UP                    0x52

// Numpad Keys
#define HID_KEY_NUM_LOCK                    0x53
#define HID_KEY_KEYPAD_DIVIDE               0x54
#define HID_KEY_KEYPAD_MULTIPLY             0x55
#define HID_KEY_KEYPAD_SUBTRACT             0x56
#define HID_KEY_KEYPAD_ADD                  0x57
#define HID_KEY_KEYPAD_ENTER                0x58
#define HID_KEY_KEYPAD_1                    0x59
#define HID_KEY_KEYPAD_2                    0x5A
#define HID_KEY_KEYPAD_3                    0x5B
#define HID_KEY_KEYPAD_4                    0x5C
#define HID_KEY_KEYPAD_5                    0x5D
#define HID_KEY_KEYPAD_6                    0x5E
#define HID_KEY_KEYPAD_7                    0x5F
#define HID_KEY_KEYPAD_8                    0x60
#define HID_KEY_KEYPAD_9                    0x61
#define HID_KEY_KEYPAD_0                    0x62
#define HID_KEY_KEYPAD_DECIMAL              0x63

// Extended Keys
#define HID_KEY_EUROPE_2                    0x64
#define HID_KEY_APPLICATION                 0x65
#define HID_KEY_POWER                       0x66
#define HID_KEY_KEYPAD_EQUAL                0x67

// Extended Function Keys
#define HID_KEY_F13                         0x68
#define HID_KEY_F14                         0x69
#define HID_KEY_F15                         0x6A
#define HID_KEY_F16                         0x6B
#define HID_KEY_F17                         0x6C
#define HID_KEY_F18                         0x6D
#define HID_KEY_F19                         0x6E
#define HID_KEY_F20                         0x6F
#define HID_KEY_F21                         0x70
#define HID_KEY_F22                         0x71
#define HID_KEY_F23                         0x72
#define HID_KEY_F24                         0x73

// System Keys
#define HID_KEY_EXECUTE                     0x74
#define HID_KEY_HELP                        0x75
#define HID_KEY_MENU                        0x76
#define HID_KEY_SELECT                      0x77
#define HID_KEY_STOP                        0x78
#define HID_KEY_AGAIN                       0x79
#define HID_KEY_UNDO                        0x7A
#define HID_KEY_CUT                         0x7B
#define HID_KEY_COPY                        0x7C
#define HID_KEY_PASTE                       0x7D
#define HID_KEY_FIND                        0x7E
// Note: These multimedia keys (0x7F-0x81) are keyboard usage page codes
// They work with setAxis() just like regular keys
#define HID_KEY_MUTE                        0x7F
#define HID_KEY_VOLUME_UP                   0x80
#define HID_KEY_VOLUME_DOWN                 0x81

// Locking Keys
#define HID_KEY_LOCKING_CAPS_LOCK           0x82
#define HID_KEY_LOCKING_NUM_LOCK            0x83
#define HID_KEY_LOCKING_SCROLL_LOCK         0x84

// Keypad Extended
#define HID_KEY_KEYPAD_COMMA                0x85
#define HID_KEY_KEYPAD_EQUAL_SIGN           0x86

// International & Language Keys
#define HID_KEY_KANJI1                      0x87
#define HID_KEY_KANJI2                      0x88
#define HID_KEY_KANJI3                      0x89
#define HID_KEY_KANJI4                      0x8A
#define HID_KEY_KANJI5                      0x8B
#define HID_KEY_KANJI6                      0x8C
#define HID_KEY_KANJI7                      0x8D
#define HID_KEY_KANJI8                      0x8E
#define HID_KEY_KANJI9                      0x8F
#define HID_KEY_LANG1                       0x90
#define HID_KEY_LANG2                       0x91
#define HID_KEY_LANG3                       0x92
#define HID_KEY_LANG4                       0x93
#define HID_KEY_LANG5                       0x94
#define HID_KEY_LANG6                       0x95
#define HID_KEY_LANG7                       0x96
#define HID_KEY_LANG8                       0x97
#define HID_KEY_LANG9                       0x98

// Additional System Keys
#define HID_KEY_ALTERNATE_ERASE             0x99
#define HID_KEY_SYSREQ_ATTENTION            0x9A
#define HID_KEY_CANCEL                      0x9B
#define HID_KEY_CLEAR                       0x9C
#define HID_KEY_PRIOR                       0x9D
#define HID_KEY_RETURN                      0x9E
#define HID_KEY_SEPARATOR                   0x9F
#define HID_KEY_OUT                         0xA0
#define HID_KEY_OPER                        0xA1
#define HID_KEY_CLEAR_AGAIN                 0xA2
#define HID_KEY_CRSEL_PROPS                 0xA3
#define HID_KEY_EXSEL                       0xA4

// Advanced Keypad Keys
#define HID_KEY_KEYPAD_00                   0xB0
#define HID_KEY_KEYPAD_000                  0xB1
#define HID_KEY_THOUSANDS_SEPARATOR         0xB2
#define HID_KEY_DECIMAL_SEPARATOR           0xB3
#define HID_KEY_CURRENCY_UNIT               0xB4
#define HID_KEY_CURRENCY_SUBUNIT            0xB5
#define HID_KEY_KEYPAD_LEFT_PARENTHESIS     0xB6
#define HID_KEY_KEYPAD_RIGHT_PARENTHESIS    0xB7
#define HID_KEY_KEYPAD_LEFT_BRACE           0xB8
#define HID_KEY_KEYPAD_RIGHT_BRACE          0xB9
#define HID_KEY_KEYPAD_TAB                  0xBA
#define HID_KEY_KEYPAD_BACKSPACE            0xBB
#define HID_KEY_KEYPAD_A                    0xBC
#define HID_KEY_KEYPAD_B                    0xBD
#define HID_KEY_KEYPAD_C                    0xBE
#define HID_KEY_KEYPAD_D                    0xBF
#define HID_KEY_KEYPAD_E                    0xC0
#define HID_KEY_KEYPAD_F                    0xC1
#define HID_KEY_KEYPAD_XOR                  0xC2
#define HID_KEY_KEYPAD_CARET                0xC3
#define HID_KEY_KEYPAD_PERCENT              0xC4
#define HID_KEY_KEYPAD_LESS_THAN            0xC5
#define HID_KEY_KEYPAD_GREATER_THAN         0xC6
#define HID_KEY_KEYPAD_AMPERSAND            0xC7
#define HID_KEY_KEYPAD_DOUBLE_AMPERSAND     0xC8
#define HID_KEY_KEYPAD_VERTICAL_BAR         0xC9
#define HID_KEY_KEYPAD_DOUBLE_VERTICAL_BAR  0xCA
#define HID_KEY_KEYPAD_COLON                0xCB
#define HID_KEY_KEYPAD_HASH                 0xCC
#define HID_KEY_KEYPAD_SPACE                0xCD
#define HID_KEY_KEYPAD_AT                   0xCE
#define HID_KEY_KEYPAD_EXCLAMATION          0xCF
#define HID_KEY_KEYPAD_MEMORY_STORE         0xD0
#define HID_KEY_KEYPAD_MEMORY_RECALL        0xD1
#define HID_KEY_KEYPAD_MEMORY_CLEAR         0xD2
#define HID_KEY_KEYPAD_MEMORY_ADD           0xD3
#define HID_KEY_KEYPAD_MEMORY_SUBTRACT      0xD4
#define HID_KEY_KEYPAD_MEMORY_MULTIPLY      0xD5
#define HID_KEY_KEYPAD_MEMORY_DIVIDE        0xD6
#define HID_KEY_KEYPAD_PLUS_MINUS           0xD7
#define HID_KEY_KEYPAD_CLEAR                0xD8
#define HID_KEY_KEYPAD_CLEAR_ENTRY          0xD9
#define HID_KEY_KEYPAD_BINARY               0xDA
#define HID_KEY_KEYPAD_OCTAL                0xDB
#define HID_KEY_KEYPAD_DECIMAL_2            0xDC
#define HID_KEY_KEYPAD_HEXADECIMAL          0xDD

// Modifier Keys (0xE0 - 0xE7)
#define HID_KEY_CONTROL_LEFT                0xE0
#define HID_KEY_SHIFT_LEFT                  0xE1
#define HID_KEY_ALT_LEFT                    0xE2
#define HID_KEY_GUI_LEFT                    0xE3  // Windows/Command key
#define HID_KEY_CONTROL_RIGHT               0xE4
#define HID_KEY_SHIFT_RIGHT                 0xE5
#define HID_KEY_ALT_RIGHT                   0xE6
#define HID_KEY_GUI_RIGHT                   0xE7  // Windows/Command key

//------------------------------------------------------------------------------
// CONSUMER CONTROL CODES (Unified with setAxis interface)
//------------------------------------------------------------------------------
// These codes are from the Consumer Control usage page (NOT keyboard usage page)
// They work with setAxis() using a 500+ offset for automatic routing.
// The actual consumer usage codes are < 0x03FF, so adding 500 keeps them separate
// from regular keyboard codes (0-255).
//
// Usage: device.setAxis(HID_CONSUMER_VOLUME_INCREMENT, 1);  // Press
//        device.setAxis(HID_CONSUMER_VOLUME_INCREMENT, 0);  // Release
//
// Common Consumer Control codes (500 + raw usage code):
#define HID_CONSUMER_BRIGHTNESS_INCREMENT   (500 + 0x006F)  // 611 - Brightness Up
#define HID_CONSUMER_BRIGHTNESS_DECREMENT   (500 + 0x0070)  // 612 - Brightness Down
#define HID_CONSUMER_PLAY                   (500 + 0x00B0)  // 676 - Play
#define HID_CONSUMER_PAUSE                  (500 + 0x00B1)  // 677 - Pause
#define HID_CONSUMER_FAST_FORWARD           (500 + 0x00B3)  // 679 - Fast Forward
#define HID_CONSUMER_REWIND                 (500 + 0x00B4)  // 680 - Rewind
#define HID_CONSUMER_SCAN_NEXT_TRACK        (500 + 0x00B5)  // 681 - Next Track
#define HID_CONSUMER_SCAN_PREVIOUS_TRACK    (500 + 0x00B6)  // 682 - Previous Track
#define HID_CONSUMER_STOP                   (500 + 0x00B7)  // 683 - Stop
#define HID_CONSUMER_EJECT                  (500 + 0x00B8)  // 684 - Eject
#define HID_CONSUMER_PLAY_PAUSE             (500 + 0x00CD)  // 705 - Play/Pause toggle
#define HID_CONSUMER_MUTE                   (500 + 0x00E2)  // 726 - Mute
#define HID_CONSUMER_VOLUME_INCREMENT       (500 + 0x00E9)  // 733 - Volume Up
#define HID_CONSUMER_VOLUME_DECREMENT       (500 + 0x00EA)  // 734 - Volume Down
#define HID_CONSUMER_AL_EMAIL               (500 + 0x018A)  // 894 - Email Reader
#define HID_CONSUMER_AL_CALCULATOR          (500 + 0x0192)  // 902 - Calculator
#define HID_CONSUMER_AC_SEARCH              (500 + 0x0221)  // 1045 - Search
#define HID_CONSUMER_AC_HOME                (500 + 0x0223)  // 1047 - Home
#define HID_CONSUMER_AC_BACK                (500 + 0x0224)  // 1048 - Back
#define HID_CONSUMER_AC_FORWARD             (500 + 0x0225)  // 1049 - Forward
#define HID_CONSUMER_AC_STOP                (500 + 0x0226)  // 1050 - Stop
#define HID_CONSUMER_AC_REFRESH             (500 + 0x0227)  // 1051 - Refresh
#define HID_CONSUMER_AC_BOOKMARKS           (500 + 0x022A)  // 1054 - Bookmarks

//------------------------------------------------------------------------------
// KEY ARRAYS AND EXAMPLES
//------------------------------------------------------------------------------

// Complete keyboard and consumer control key list for setAxis() reference
// All codes are compatible with TinyUsbKeyboardDevice::setAxis(int code, int value)
static const KeyDef KBD_KEYS_FULL_EXAMPLE[] = {
    // Special
    K_KEY_N(HID_KEY_NONE),
    
    // Letters (A-Z)
    K_KEY_N(HID_KEY_A), K_KEY_N(HID_KEY_B), K_KEY_N(HID_KEY_C), K_KEY_N(HID_KEY_D),
    K_KEY_N(HID_KEY_E), K_KEY_N(HID_KEY_F), K_KEY_N(HID_KEY_G), K_KEY_N(HID_KEY_H),
    K_KEY_N(HID_KEY_I), K_KEY_N(HID_KEY_J), K_KEY_N(HID_KEY_K), K_KEY_N(HID_KEY_L),
    K_KEY_N(HID_KEY_M), K_KEY_N(HID_KEY_N), K_KEY_N(HID_KEY_O), K_KEY_N(HID_KEY_P),
    K_KEY_N(HID_KEY_Q), K_KEY_N(HID_KEY_R), K_KEY_N(HID_KEY_S), K_KEY_N(HID_KEY_T),
    K_KEY_N(HID_KEY_U), K_KEY_N(HID_KEY_V), K_KEY_N(HID_KEY_W), K_KEY_N(HID_KEY_X),
    K_KEY_N(HID_KEY_Y), K_KEY_N(HID_KEY_Z),
    
    // Numbers (0-9)
    K_KEY_N(HID_KEY_1), K_KEY_N(HID_KEY_2), K_KEY_N(HID_KEY_3), K_KEY_N(HID_KEY_4),
    K_KEY_N(HID_KEY_5), K_KEY_N(HID_KEY_6), K_KEY_N(HID_KEY_7), K_KEY_N(HID_KEY_8),
    K_KEY_N(HID_KEY_9), K_KEY_N(HID_KEY_0),
    
    // Special Keys
    K_KEY_N(HID_KEY_ENTER), K_KEY_N(HID_KEY_ESCAPE), K_KEY_N(HID_KEY_BACKSPACE),
    K_KEY_N(HID_KEY_TAB), K_KEY_N(HID_KEY_SPACE), K_KEY_N(HID_KEY_MINUS),
    K_KEY_N(HID_KEY_EQUAL), K_KEY_N(HID_KEY_BRACKET_LEFT), K_KEY_N(HID_KEY_BRACKET_RIGHT),
    K_KEY_N(HID_KEY_BACKSLASH), K_KEY_N(HID_KEY_EUROPE_1), K_KEY_N(HID_KEY_SEMICOLON),
    K_KEY_N(HID_KEY_APOSTROPHE), K_KEY_N(HID_KEY_GRAVE), K_KEY_N(HID_KEY_COMMA),
    K_KEY_N(HID_KEY_PERIOD), K_KEY_N(HID_KEY_SLASH), K_KEY_N(HID_KEY_CAPS_LOCK),
    
    // Function Keys (F1-F12)
    K_KEY_N(HID_KEY_F1), K_KEY_N(HID_KEY_F2), K_KEY_N(HID_KEY_F3), K_KEY_N(HID_KEY_F4),
    K_KEY_N(HID_KEY_F5), K_KEY_N(HID_KEY_F6), K_KEY_N(HID_KEY_F7), K_KEY_N(HID_KEY_F8),
    K_KEY_N(HID_KEY_F9), K_KEY_N(HID_KEY_F10), K_KEY_N(HID_KEY_F11), K_KEY_N(HID_KEY_F12),
    
    // Navigation Keys
    K_KEY_N(HID_KEY_PRINT_SCREEN), K_KEY_N(HID_KEY_SCROLL_LOCK), K_KEY_N(HID_KEY_PAUSE),
    K_KEY_N(HID_KEY_INSERT), K_KEY_N(HID_KEY_HOME), K_KEY_N(HID_KEY_PAGE_UP),
    K_KEY_N(HID_KEY_DELETE), K_KEY_N(HID_KEY_END), K_KEY_N(HID_KEY_PAGE_DOWN),
    K_KEY_N(HID_KEY_ARROW_RIGHT), K_KEY_N(HID_KEY_ARROW_LEFT),
    K_KEY_N(HID_KEY_ARROW_DOWN), K_KEY_N(HID_KEY_ARROW_UP),
    
    // Numpad Keys
    K_KEY_N(HID_KEY_NUM_LOCK), K_KEY_N(HID_KEY_KEYPAD_DIVIDE), K_KEY_N(HID_KEY_KEYPAD_MULTIPLY),
    K_KEY_N(HID_KEY_KEYPAD_SUBTRACT), K_KEY_N(HID_KEY_KEYPAD_ADD), K_KEY_N(HID_KEY_KEYPAD_ENTER),
    K_KEY_N(HID_KEY_KEYPAD_1), K_KEY_N(HID_KEY_KEYPAD_2), K_KEY_N(HID_KEY_KEYPAD_3),
    K_KEY_N(HID_KEY_KEYPAD_4), K_KEY_N(HID_KEY_KEYPAD_5), K_KEY_N(HID_KEY_KEYPAD_6),
    K_KEY_N(HID_KEY_KEYPAD_7), K_KEY_N(HID_KEY_KEYPAD_8), K_KEY_N(HID_KEY_KEYPAD_9),
    K_KEY_N(HID_KEY_KEYPAD_0), K_KEY_N(HID_KEY_KEYPAD_DECIMAL),
    
    // Extended Keys
    K_KEY_N(HID_KEY_EUROPE_2), K_KEY_N(HID_KEY_APPLICATION),
    K_KEY_N(HID_KEY_POWER), K_KEY_N(HID_KEY_KEYPAD_EQUAL),
    
    // Extended Function Keys (F13-F24)
    K_KEY_N(HID_KEY_F13), K_KEY_N(HID_KEY_F14), K_KEY_N(HID_KEY_F15), K_KEY_N(HID_KEY_F16),
    K_KEY_N(HID_KEY_F17), K_KEY_N(HID_KEY_F18), K_KEY_N(HID_KEY_F19), K_KEY_N(HID_KEY_F20),
    K_KEY_N(HID_KEY_F21), K_KEY_N(HID_KEY_F22), K_KEY_N(HID_KEY_F23), K_KEY_N(HID_KEY_F24),
    
    // System Keys
    K_KEY_N(HID_KEY_EXECUTE), K_KEY_N(HID_KEY_HELP), K_KEY_N(HID_KEY_MENU),
    K_KEY_N(HID_KEY_SELECT), K_KEY_N(HID_KEY_STOP), K_KEY_N(HID_KEY_AGAIN),
    K_KEY_N(HID_KEY_UNDO), K_KEY_N(HID_KEY_CUT), K_KEY_N(HID_KEY_COPY),
    K_KEY_N(HID_KEY_PASTE), K_KEY_N(HID_KEY_FIND),
    
    // Keyboard Multimedia Keys (work with setAxis directly)
    K_KEY_N(HID_KEY_MUTE), K_KEY_N(HID_KEY_VOLUME_UP), K_KEY_N(HID_KEY_VOLUME_DOWN),
    
    // Locking Keys
    K_KEY_N(HID_KEY_LOCKING_CAPS_LOCK), K_KEY_N(HID_KEY_LOCKING_NUM_LOCK),
    K_KEY_N(HID_KEY_LOCKING_SCROLL_LOCK),
    
    // Keypad Extended
    K_KEY_N(HID_KEY_KEYPAD_COMMA), K_KEY_N(HID_KEY_KEYPAD_EQUAL_SIGN),
    
    // International & Language Keys
    K_KEY_N(HID_KEY_KANJI1), K_KEY_N(HID_KEY_KANJI2), K_KEY_N(HID_KEY_KANJI3),
    K_KEY_N(HID_KEY_KANJI4), K_KEY_N(HID_KEY_KANJI5), K_KEY_N(HID_KEY_KANJI6),
    K_KEY_N(HID_KEY_KANJI7), K_KEY_N(HID_KEY_KANJI8), K_KEY_N(HID_KEY_KANJI9),
    K_KEY_N(HID_KEY_LANG1), K_KEY_N(HID_KEY_LANG2), K_KEY_N(HID_KEY_LANG3),
    K_KEY_N(HID_KEY_LANG4), K_KEY_N(HID_KEY_LANG5), K_KEY_N(HID_KEY_LANG6),
    K_KEY_N(HID_KEY_LANG7), K_KEY_N(HID_KEY_LANG8), K_KEY_N(HID_KEY_LANG9),
    
    // Additional System Keys
    K_KEY_N(HID_KEY_ALTERNATE_ERASE), K_KEY_N(HID_KEY_SYSREQ_ATTENTION),
    K_KEY_N(HID_KEY_CANCEL), K_KEY_N(HID_KEY_CLEAR), K_KEY_N(HID_KEY_PRIOR),
    K_KEY_N(HID_KEY_RETURN), K_KEY_N(HID_KEY_SEPARATOR), K_KEY_N(HID_KEY_OUT),
    K_KEY_N(HID_KEY_OPER), K_KEY_N(HID_KEY_CLEAR_AGAIN),
    K_KEY_N(HID_KEY_CRSEL_PROPS), K_KEY_N(HID_KEY_EXSEL),
    
    // Advanced Keypad Keys
    K_KEY_N(HID_KEY_KEYPAD_00), K_KEY_N(HID_KEY_KEYPAD_000),
    K_KEY_N(HID_KEY_THOUSANDS_SEPARATOR), K_KEY_N(HID_KEY_DECIMAL_SEPARATOR),
    K_KEY_N(HID_KEY_CURRENCY_UNIT), K_KEY_N(HID_KEY_CURRENCY_SUBUNIT),
    K_KEY_N(HID_KEY_KEYPAD_LEFT_PARENTHESIS), K_KEY_N(HID_KEY_KEYPAD_RIGHT_PARENTHESIS),
    K_KEY_N(HID_KEY_KEYPAD_LEFT_BRACE), K_KEY_N(HID_KEY_KEYPAD_RIGHT_BRACE),
    K_KEY_N(HID_KEY_KEYPAD_TAB), K_KEY_N(HID_KEY_KEYPAD_BACKSPACE),
    K_KEY_N(HID_KEY_KEYPAD_A), K_KEY_N(HID_KEY_KEYPAD_B), K_KEY_N(HID_KEY_KEYPAD_C),
    K_KEY_N(HID_KEY_KEYPAD_D), K_KEY_N(HID_KEY_KEYPAD_E), K_KEY_N(HID_KEY_KEYPAD_F),
    K_KEY_N(HID_KEY_KEYPAD_XOR), K_KEY_N(HID_KEY_KEYPAD_CARET), K_KEY_N(HID_KEY_KEYPAD_PERCENT),
    K_KEY_N(HID_KEY_KEYPAD_LESS_THAN), K_KEY_N(HID_KEY_KEYPAD_GREATER_THAN),
    K_KEY_N(HID_KEY_KEYPAD_AMPERSAND), K_KEY_N(HID_KEY_KEYPAD_DOUBLE_AMPERSAND),
    K_KEY_N(HID_KEY_KEYPAD_VERTICAL_BAR), K_KEY_N(HID_KEY_KEYPAD_DOUBLE_VERTICAL_BAR),
    K_KEY_N(HID_KEY_KEYPAD_COLON), K_KEY_N(HID_KEY_KEYPAD_HASH), K_KEY_N(HID_KEY_KEYPAD_SPACE),
    K_KEY_N(HID_KEY_KEYPAD_AT), K_KEY_N(HID_KEY_KEYPAD_EXCLAMATION),
    K_KEY_N(HID_KEY_KEYPAD_MEMORY_STORE), K_KEY_N(HID_KEY_KEYPAD_MEMORY_RECALL),
    K_KEY_N(HID_KEY_KEYPAD_MEMORY_CLEAR), K_KEY_N(HID_KEY_KEYPAD_MEMORY_ADD),
    K_KEY_N(HID_KEY_KEYPAD_MEMORY_SUBTRACT), K_KEY_N(HID_KEY_KEYPAD_MEMORY_MULTIPLY),
    K_KEY_N(HID_KEY_KEYPAD_MEMORY_DIVIDE), K_KEY_N(HID_KEY_KEYPAD_PLUS_MINUS),
    K_KEY_N(HID_KEY_KEYPAD_CLEAR), K_KEY_N(HID_KEY_KEYPAD_CLEAR_ENTRY),
    K_KEY_N(HID_KEY_KEYPAD_BINARY), K_KEY_N(HID_KEY_KEYPAD_OCTAL),
    K_KEY_N(HID_KEY_KEYPAD_DECIMAL_2), K_KEY_N(HID_KEY_KEYPAD_HEXADECIMAL),
    
    // Modifier Keys
    K_KEY_N(HID_KEY_CONTROL_LEFT), K_KEY_N(HID_KEY_SHIFT_LEFT),
    K_KEY_N(HID_KEY_ALT_LEFT), K_KEY_N(HID_KEY_GUI_LEFT),
    K_KEY_N(HID_KEY_CONTROL_RIGHT), K_KEY_N(HID_KEY_SHIFT_RIGHT),
    K_KEY_N(HID_KEY_ALT_RIGHT), K_KEY_N(HID_KEY_GUI_RIGHT),
    
    // Consumer Control Keys (500+ offset, work with setAxis)
    K_KEY_N(HID_CONSUMER_BRIGHTNESS_INCREMENT), K_KEY_N(HID_CONSUMER_BRIGHTNESS_DECREMENT),
    K_KEY_N(HID_CONSUMER_PLAY), K_KEY_N(HID_CONSUMER_PAUSE),
    K_KEY_N(HID_CONSUMER_FAST_FORWARD), K_KEY_N(HID_CONSUMER_REWIND),
    K_KEY_N(HID_CONSUMER_SCAN_NEXT_TRACK), K_KEY_N(HID_CONSUMER_SCAN_PREVIOUS_TRACK),
    K_KEY_N(HID_CONSUMER_STOP), K_KEY_N(HID_CONSUMER_EJECT),
    K_KEY_N(HID_CONSUMER_PLAY_PAUSE), K_KEY_N(HID_CONSUMER_MUTE),
    K_KEY_N(HID_CONSUMER_VOLUME_INCREMENT), K_KEY_N(HID_CONSUMER_VOLUME_DECREMENT),
    K_KEY_N(HID_CONSUMER_AL_EMAIL), K_KEY_N(HID_CONSUMER_AL_CALCULATOR),
    K_KEY_N(HID_CONSUMER_AC_SEARCH), K_KEY_N(HID_CONSUMER_AC_HOME),
    K_KEY_N(HID_CONSUMER_AC_BACK), K_KEY_N(HID_CONSUMER_AC_FORWARD),
    K_KEY_N(HID_CONSUMER_AC_STOP), K_KEY_N(HID_CONSUMER_AC_REFRESH),
    K_KEY_N(HID_CONSUMER_AC_BOOKMARKS)
};
#endif // KBDKEYS_H
