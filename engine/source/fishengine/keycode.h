#ifndef KEYCODE_H
#define KEYCODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KeyCode {
    // Not assigned (never returned as the result of a keystroke)
    KeyCodeNone = 0,
    // The backspace key
    KeyCodeBackspace = 8,
    // The forward delete key
    KeyCodeDelete = 127,
    // The tab key
    KeyCodeTab = 9,
    // The Clear key
    KeyCodeClear = 12,
    // Return key
    KeyCodeReturn = 13,
    // Pause on PC machines
    KeyCodePause = 19,
    // Escape key
    KeyCodeEscape = 27,
    // Space key
    KeyCodeSpace = 32,

    // Numeric keypad 0
    KeyCodeKeypad0 = 256,
    // Numeric keypad 1
    KeyCodeKeypad1 = 257,
    // Numeric keypad 2
    KeyCodeKeypad2 = 258,
    // Numeric keypad 3
    KeyCodeKeypad3 = 259,
    // Numeric keypad 4
    KeyCodeKeypad4 = 260,
    // Numeric keypad 5
    KeyCodeKeypad5 = 261,
    // Numeric keypad 6
    KeyCodeKeypad6 = 262,
    // Numeric keypad 7
    KeyCodeKeypad7 = 263,
    // Numeric keypad 8
    KeyCodeKeypad8 = 264,
    // Numeric keypad 9
    KeyCodeKeypad9 = 265,
    // Numeric keypad '.'
    KeyCodeKeypadPeriod = 266,
    // Numeric keypad '/'
    KeyCodeKeypadDivide = 267,
    // Numeric keypad '*'
    KeyCodeKeypadMultiply = 268,
    // Numeric keypad '-'
    KeyCodeKeypadMinus = 269,
    // Numeric keypad '+'
    KeyCodeKeypadPlus = 270,
    // Numeric keypad enter
    KeyCodeKeypadEnter = 271,
    // Numeric keypad '='
    KeyCodeKeypadEquals = 272,

    // Up arrow key
    KeyCodeUpArrow = 273,
    // Down arrow key
    KeyCodeDownArrow = 274,
    // Right arrow key
    KeyCodeRightArrow = 275,
    // Left arrow key
    KeyCodeLeftArrow = 276,
    // Insert key key
    KeyCodeInsert = 277,
    // Home key
    KeyCodeHome = 278,
    // End key
    KeyCodeEnd = 279,
    // Page up
    KeyCodePageUp = 280,
    // Page down
    KeyCodePageDown = 281,

    // F1 function key
    KeyCodeF1 = 282,
    // F2 function key
    KeyCodeF2 = 283,
    // F3 function key
    KeyCodeF3 = 284,
    // F4 function key
    KeyCodeF4 = 285,
    // F5 function key
    KeyCodeF5 = 286,
    // F6 function key
    KeyCodeF6 = 287,
    // F7 function key
    KeyCodeF7 = 288,
    // F8 function key
    KeyCodeF8 = 289,
    // F9 function key
    KeyCodeF9 = 290,
    // F10 function key
    KeyCodeF10 = 291,
    // F11 function key
    KeyCodeF11 = 292,
    // F12 function key
    KeyCodeF12 = 293,
    // F13 function key
    KeyCodeF13 = 294,
    // F14 function key
    KeyCodeF14 = 295,
    // F15 function key
    KeyCodeF15 = 296,

    // The '0' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha0 = 48,
    // The '1' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha1 = 49,
    // The '2' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha2 = 50,
    // The '3' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha3 = 51,
    // The '4' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha4 = 52,
    // The '5' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha5 = 53,
    // The '6' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha6 = 54,
    // The '7' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha7 = 55,
    // The '8' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha8 = 56,
    // The '9' key on the top of the alphanumeric keyboard.
    KeyCodeAlpha9 = 57,

    // Exclamation mark key '!'
    KeyCodeExclaim = 33,
    // Double quote key '"'
    KeyCodeDoubleQuote = 34,
    // Hash key '#'
    KeyCodeHash = 35,
    // Dollar sign key '$'
    KeyCodeDollar = 36,
    // Ampersand key '&'
    KeyCodeAmpersand = 38,
    // Quote key '
    KeyCodeQuote = 39,
    // Left Parenthesis key '('
    KeyCodeLeftParen = 40,
    // Right Parenthesis key ')'
    KeyCodeRightParen = 41,
    // Asterisk key '*'
    KeyCodeAsterisk = 42,
    // Plus key '+'
    KeyCodePlus = 43,
    // Comma ',' key
    KeyCodeComma = 44,

    // Minus '-' key
    KeyCodeMinus = 45,
    // Period '.' key
    KeyCodePeriod = 46,
    // Slash '/' key
    KeyCodeSlash = 47,

    // Colon ':' key
    KeyCodeColon = 58,
    // Semicolon ';' key
    KeyCodeSemicolon = 59,
    // Less than '<' key
    KeyCodeLess = 60,
    // Equals '=' key
    KeyCodeEquals = 61,
    // Greater than '>' key
    KeyCodeGreater = 62,
    // Question mark '?' key
    KeyCodeQuestion = 63,
    // At key '@'
    KeyCodeAt = 64,

    // Left square bracket key '['
    KeyCodeLeftBracket = 91,
    // Backslash key '\'
    KeyCodeBackslash = 92,
    // Right square bracket key ']'
    KeyCodeRightBracket = 93,
    // Caret key '^'
    KeyCodeCaret = 94,
    // Underscore '_' key
    KeyCodeUnderscore = 95,
    // Back quote key '`'
    KeyCodeBackQuote = 96,

    // 'a' key
    KeyCodeA = 97,
    // 'b' key
    KeyCodeB = 98,
    // 'c' key
    KeyCodeC = 99,
    // 'd' key
    KeyCodeD = 100,
    // 'e' key
    KeyCodeE = 101,
    // 'f' key
    KeyCodeF = 102,
    // 'g' key
    KeyCodeG = 103,
    // 'h' key
    KeyCodeH = 104,
    // 'i' key
    KeyCodeI = 105,
    // 'j' key
    KeyCodeJ = 106,
    // 'k' key
    KeyCodeK = 107,
    // 'l' key
    KeyCodeL = 108,
    // 'm' key
    KeyCodeM = 109,
    // 'n' key
    KeyCodeN = 110,
    // 'o' key
    KeyCodeO = 111,
    // 'p' key
    KeyCodeP = 112,
    // 'q' key
    KeyCodeQ = 113,
    // 'r' key
    KeyCodeR = 114,
    // 's' key
    KeyCodeS = 115,
    // 't' key
    KeyCodeT = 116,
    // 'u' key
    KeyCodeU = 117,
    // 'v' key
    KeyCodeV = 118,
    // 'w' key
    KeyCodeW = 119,
    // 'x' key
    KeyCodeX = 120,
    // 'y' key
    KeyCodeY = 121,
    // 'z' key
    KeyCodeZ = 122,

    // Numlock key
    KeyCodeNumlock = 300,
    // Capslock key
    KeyCodeCapsLock = 301,
    // Scroll lock key
    KeyCodeScrollLock = 302,
    // Right shift key
    KeyCodeRightShift = 303,
    // Left shift key
    KeyCodeLeftShift = 304,
    // Right Control key
    KeyCodeRightControl = 305,
    // Left Control key
    KeyCodeLeftControl = 306,
    // Right Alt key
    KeyCodeRightAlt = 307,
    // Left Alt key
    KeyCodeLeftAlt = 308,

    // Left Command key
    KeyCodeLeftCommand = 310,
    // Left Command key
    KeyCodeLeftApple = 310,
    // Left Windows key
    KeyCodeLeftWindows = 311,
    // Right Command key
    KeyCodeRightCommand = 309,
    // Right Command key
    KeyCodeRightApple = 309,
    // Right Windows key
    KeyCodeRightWindows = 312,
    // Alt Gr key
    KeyCodeAltGr = 313,

    // Help key
    KeyCodeHelp = 315,
    // Print key
    KeyCodePrint = 316,
    // Sys Req key
    KeyCodeSysReq = 317,
    // Break key
    KeyCodeBreak = 318,
    // Menu key
    KeyCodeMenu = 319,

    // First (primary) mouse button
    KeyCodeMouse0 = 323,
    // Second (secondary) mouse button
    KeyCodeMouse1 = 324,
    // Third mouse button
    KeyCodeMouse2 = 325,
    // Fourth mouse button
    KeyCodeMouse3 = 326,
    // Fifth mouse button
    KeyCodeMouse4 = 327,
    // Sixth mouse button
    KeyCodeMouse5 = 328,
    // Seventh mouse button
    KeyCodeMouse6 = 329,

    _KeyCodeCount,

    KeyCodeMouseLeftButton = KeyCodeMouse0,
    KeyCodeMouseRightButton = KeyCodeMouse1,
    KeyCodeMouseMiddleButton = KeyCodeMouse2,
} KeyCode;

KeyCode KeyCodeFromGLFWKey(int glfwKey);

#ifdef __cplusplus
}
#endif

#endif /* KEYCODE_H */