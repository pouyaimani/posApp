// #include "states.h"
// #include "eventloop.h"
// #include "magReader/magReader.h"
// #include "logger.h"
// #include "sys/sys.h"
// #include "mylvgl.h"
// #include "display.h"
// #include "ui/inbox.h"
// #include "ui/button.h"
// #include "utility/utility.h"
// #include "utility/alphabetic.h"
// #include "phrases/phrases.h"
// #include "font/myFont.h"
// #include "ui/infoPage.h"

// #define PASS_MAX_LEN   4
// #define AMOUNT_MAX_LEN 10

// static InputBox  inputBox;
// static Button    confirmBut;
// static Button    cancelBut;
// static lv_obj_t* title;
// static lv_obj_t* info;

// static InputMode_t inMode;
// static char*       input;
// static char*       amountStr;
// static char*       password;
// static char*       ipAddr;
// static uint8_t     idx   = 0;
// static uint8_t     maxIn = 0;

// typedef struct {
//     uint8_t octet[4];
//     uint8_t digits[4];     // how many digits user entered (0–3)
//     uint8_t currentOctet;  // 0–3
//     uint8_t cursorInOctet; // 0–2
// } IpInput;

// static IpInput ip;

// static bool ipValidateDigits(const char* in) {
//     if (!in)
//         return false;

//     int  len = 0;
//     char digits[12];

//     // Extract digits only (same logic as formatter)
//     for (int i = 0; in[i] && len < 12; i++) {
//         if (isDigit((unsigned char)in[i])) {
//             digits[len++] = in[i];
//         }
//     }

//     // Must be exactly 12 digits (4 full octets)
//     if (len != 12)
//         return false;

//     // Validate each octet
//     for (int oct = 0; oct < 4; oct++) {
//         int val = 0;

//         for (int j = 0; j < 3; j++) {
//             val = val * 10 + (digits[oct * 3 + j] - '0');
//         }

//         if (val > 255)
//             return false;
//     }

//     return true;
// }

// static bool amountValidate(const char* in) {
//     char digits[6];
//     int  len = 0;

//     if (!in) {
//         return false;
//     }
//     if (in[0] == 0) {
//         return false;
//     }
//     if (strlen(in) < AMOUNT_MIN_CNT) {
//         return false;
//     }
// }

// static void showMaxError(State* state) {
//     switch (inMode) {
//     case IN_MODE_AMOUNT:
//         GOTO_INFO(state, state, INFO_ERROR, phraseGetDef(PHRASE_ERROR),
//                   phraseGetDef(PHRASE_AMOUNT_EXCEED));
//         break;
//     case IN_MODE_PASSWORD:
//         break;
//     case IN_MODE_NUMBERS:
//         GOTO_INFO(state, state, INFO_ERROR, phraseGetDef(PHRASE_ERROR),
//                   phraseGetDef(PHRASE_INPUT_EXCEED));
//         break;
//     default:
//         break;
//     }
// }

// static void handleInput(State* state, KeypadEvent* ev) {
//     bool isPassword = inMode == IN_MODE_PASSWORD ? true : false;
//     bool isAmount   = inMode == IN_MODE_AMOUNT ? true : false;
//     bool isIp       = inMode == IN_MODE_IP ? true : false;
//     if (ev->key == KEY_CLEAR) {
//         deleteChar(input);
//         deleteChar(password);
//     } else {
//         if (idx >= maxIn) {
//             showMaxError(state);
//             return;
//         }
//         appendChar(input, INPUT_MAX_LEN, isPassword ? '*' : ev->keyStr);
//         if (isPassword) {
//             appendChar(password, INPUT_MAX_LEN, ev->keyStr);
//         }
//     }

//     if (isAmount) {
//         if (input[0] == '0') {
//             deleteChar(input);
//             return;
//         }
//         amountSeparator(input, amountStr, INPUT_MAX_LEN);
//         LV_SET_TEXT(inputBox.textBox, amountStr);
//     } else if (isIp) {
//         ipFormatLeftAligned(input, ipAddr);
//         lv_label_set_recolor(inputBox.textBox, true);
//         LV_SET_TEXT(inputBox.textBox, ipAddr);
//     } else if (inMode == IN_MODE_DATE) {
//         char buf[16];
//         dateFormat(input, buf);
//         LV_SET_TEXT(inputBox.textBox, buf);
//     } else if (inMode == IN_MODE_TIME) {
//         char buf[16];
//         timeFormat(input, buf);
//         LV_SET_TEXT(inputBox.textBox, buf);
//     } else {
//         LV_SET_TEXT(inputBox.textBox, input);
//     }
//     if (ev->key <= KEY_9 && idx < maxIn) {
//         idx++;
//     } else if (ev->key == KEY_CLEAR && idx > 0) {
//         idx--;
//     }
// }

// static void handleAlpahb(State* state, KeypadEvent* ev) {
//     if (ev->key == KEY_CLEAR) {
//         deleteChar(input);
//     } else {
//         alphebatic()->addKey(input, INPUT_MAX_LEN, ev->key);
//     }
//     LV_SET_TEXT(inputBox.textBox, input);
// }

// STATE_DEF_HANDLE(Input, KeypadEvent) {
//     if (ev->key == KEY_ESC) {
//         if (state->prev)
//             SM_GOTO(state->prev);
//         else
//             LOG_WARN("Input state: previous state is not set.");
//         return;
//     } else if (ev->key == KEY_ENTER) {
//         if (state->next) {
//             if (inMode == IN_MODE_PASSWORD) {
//                 if (idx != maxIn) {
//                     return;
//                 }
//             } else if (inMode == IN_MODE_IP) {
//                 if (!ipValidateDigits(input)) {
//                     GOTO_INFO(state, state, INFO_ERROR,
//                               phraseGetDef(PHRASE_ERROR),
//                               phraseGetDef(PHRASE_INVALID_IP));
//                     return;
//                 }
//             } else if (inMode == IN_MODE_DATE) {
//                 if (!dateValidate(input)) {
//                     GOTO_INFO(state, state, INFO_ERROR,
//                               phraseGetDef(PHRASE_ERROR),
//                               phraseGetDef(PHRASE_INVALID_DATE));
//                     return;
//                 }
//             } else if (inMode == IN_MODE_TIME) {
//                 if (!timeValidate(input)) {
//                     GOTO_INFO(state, state, INFO_ERROR,
//                               phraseGetDef(PHRASE_ERROR),
//                               phraseGetDef(PHRASE_INVALID_TIME));
//                     return;
//                 }
//             } else if (inMode == IN_MODE_AMOUNT) {
//                 if (!amountValidate(input)) {
//                     GOTO_INFO(state, state, INFO_ERROR,
//                               phraseGetDef(PHRASE_ERROR),
//                               phraseGetDef(PHRASE_INVALID_AMOUNT));
//                     return;
//                 }
//             }
//             SM_GOTO(state->next);
//         } else
//             LOG_WARN("Input state: next state is not set.");
//     }

//     if (inMode == IN_MODE_ALPHAB) {
//         handleAlpahb(state, ev);
//         return;
//     }
//     if (ev->key > KEY_9 && ev->key != KEY_CLEAR)
//         return;

//     handleInput(state, ev);
// }

// static void createUi() {
//     title = lv_label_create(disp()->screen);
//     LV_SET_TEXT_FONT(title, FONT_20);
//     LV_SET_TEXT_COLOR(title, COLOR_TEXT_PRIMARY);
//     LV_SET_SIZE(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
//     LV_ALIGN(title, LV_ALIGN_CENTER, 0, -70);

//     info = lv_label_create(disp()->screen);
//     LV_SET_TEXT_FONT(info, FONT_16);
//     LV_SET_TEXT_COLOR(info, COLOR_TEXT_SECONDARY);
//     LV_SET_SIZE(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
//     LV_ALIGN(info, LV_ALIGN_CENTER, 0, -30);

//     ui_inBox_create(&inputBox, disp()->screen);
//     LV_ALIGN(inputBox.main, LV_ALIGN_CENTER, 0, 10);
//     ui_button_create(&confirmBut, disp()->screen);
//     ui_button_set_text(&confirmBut, phraseGetDef(PHRASE_CONFIRM));
//     ui_button_set_icon(&confirmBut, LV_SYMBOL_OK);
//     ui_button_set_color(&confirmBut, lv_color_hex(0x68DD40));
//     LV_ALIGN(ui_button_obj(&confirmBut), LV_ALIGN_BOTTOM_RIGHT, -5, -10);
//     ui_button_create(&cancelBut, disp()->screen);
//     ui_button_set_text(&cancelBut, phraseGetDef(PHRASE_CANCEL));
//     ui_button_set_icon(&cancelBut, LV_SYMBOL_CLOSE);
//     ui_button_set_color(&cancelBut, lv_color_hex(0xFF4E4E));
//     LV_ALIGN(ui_button_obj(&cancelBut), LV_ALIGN_BOTTOM_LEFT, 5, -10);

//     LV_SET_TEXT(inputBox.textBox, "");
// }

// static void setMode(InputMode_t mode) { inMode = mode; }

// static void setData(const char* dtitle, const char* dinfo) {
//     LV_SET_TEXT(title, dtitle);
//     LV_SET_TEXT(info, dinfo);
// }

// static void setMax(int val) { maxIn = val; }

// static void setInput(const char* in) {
//     snprintf(input, INPUT_MAX_LEN, "%s", in);
//     idx = strlen(input);
// }

// static void reset() {
//     clearStr(password);
//     clearStr(input);
//     idx = 0;
//     LV_SET_TEXT(inputBox.textBox, "");
//     LV_SET_TEXT(title, "");
//     LV_SET_TEXT(info, "");
//     maxIn = INPUT_MAX_LEN;
//     memset(&ip, 0, sizeof(IpInput));
// }

// STATE_DEF_ENTER(Input) {
//     LV_SHOW(inputBox.main);
//     LV_SHOW(ui_button_obj(&confirmBut));
//     LV_SHOW(ui_button_obj(&cancelBut));
//     LV_SHOW(title);
//     LV_SHOW(info);
//     if (inMode == IN_MODE_IP) {
//         char buf[24];
//         ipFormatLeftAligned(input, buf);
//         LV_SET_TEXT(inputBox.textBox, buf);
//     } else if (inMode == IN_MODE_AMOUNT) {
//         amountSeparator(input, amountStr, INPUT_MAX_LEN);
//         LV_SET_TEXT(inputBox.textBox, amountStr);
//     } else if (inMode == IN_MODE_DATE) {
//         char buf[16];
//         dateFormat(input, buf);
//         LV_SET_TEXT(inputBox.textBox, buf);
//     } else if (inMode == IN_MODE_TIME) {
//         char buf[16];
//         timeFormat(input, buf);
//         LV_SET_TEXT(inputBox.textBox, buf);
//     } else if (inMode == IN_MODE_NUMBERS) {
//         LV_SET_TEXT(inputBox.textBox, input);
//     }
// }

// STATE_DEF_EXIT(Input) {
//     Input* in = STATE_INPUT;
//     if (in->out) {
//         snprintf(in->out, maxIn + 1, "%s", in->input);
//     }
//     LV_HIDE(inputBox.main);
//     LV_HIDE(ui_button_obj(&confirmBut));
//     LV_HIDE(ui_button_obj(&cancelBut));
//     LV_HIDE(title);
//     LV_HIDE(info);
// }

// STATE_DEF_HANDLE(Input, TimeOutEvent) {}

// OOP_CTOR(Input, State* parent, const char* name) {
//     OOP_CALL_CTOR(State, self, parent, name);
//     self->base.vtable.enter         = STATE_ENTER(Input);
//     self->base.vtable.exit          = STATE_EXIT(Input);
//     self->base.vtable.handleKeypad  = STATE_HANDLE(Input, KeypadEvent);
//     self->base.vtable.handleTimeout = STATE_HANDLE(Input, TimeOutEvent);
//     self->setMode                   = setMode;
//     self->setData                   = setData;
//     self->setMax                    = setMax;
//     self->reset                     = reset;
//     self->setInput                  = setInput;
//     input                           = (char*)MEM_ALLOC(INPUT_MAX_LEN);
//     password                        = (char*)MEM_ALLOC(PASS_MAX_LEN + 1);
//     ipAddr                          = (char*)MEM_ALLOC(INPUT_MAX_LEN / 2);
//     self->input                     = input;
//     self->password                  = password;
//     self->ip                        = ipAddr;
//     amountStr                       = (char*)MEM_ALLOC(INPUT_MAX_LEN);
//     createUi();
// }