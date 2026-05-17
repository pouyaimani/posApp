#include "states.h"
#include "eventloop.h"
#include "magReader/magReader.h"
#include "logger.h"
#include "sys/sys.h"
#include "mylvgl.h"
#include "display.h"
#include "ui/ui.h"
#include "utility/utility.h"
#include "utility/alphabetic.h"
#include "phrases/phrases.h"

#define PASS_MAX_LEN        4
#define AMOUNT_MAX_LEN      10

static InputBox inputBox;
static Button confirmBut;
static Button cancelBut;
static lv_obj_t *title;
static lv_obj_t *info;

static InputMode_t inMode;
static char *input;
static char *amountStr;
static char *password;
static uint8_t idx = 0;
static uint8_t maxIn = 0;

typedef struct {
    uint8_t octet[4];
    uint8_t digits[4];     // how many digits user entered (0–3)
    uint8_t currentOctet;  // 0–3
    uint8_t cursorInOctet; // 0–2
} IpInput;

static IpInput ip;

static bool ipValidateDigits(const char *in)
{
    if (!in) return false;

    int len = 0;
    char digits[12];

    // Extract digits only (same logic as formatter)
    for (int i = 0; in[i] && len < 12; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[len++] = in[i];
        }
    }

    // Must be exactly 12 digits (4 full octets)
    if (len != 12)
        return false;

    // Validate each octet
    for (int oct = 0; oct < 4; oct++) {
        int val = 0;

        for (int j = 0; j < 3; j++) {
            val = val * 10 + (digits[oct * 3 + j] - '0');
        }

        if (val > 255)
            return false;
    }

    return true;
}

static bool ipFormatLeftAligned(const char *in, char *out)
{
    char digits[12] = {0};
    int dcount = 0;

    // Extract digits only (max 12)
    for (int i = 0; in[i] && dcount < 12; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[dcount++] = in[i];
        }
    }

    int pos = 0;
    int di = 0;
    bool valid = true;

    for (int oct = 0; oct < 4; oct++) {

        // If not enough digits for this octet → fill with "---"
        if (di >= dcount) {
            out[pos++] = '-';
            out[pos++] = '-';
            out[pos++] = '-';
        } else {
            int octVal = 0;
            int start = di;

            // Read up to 3 digits
            int len = 0;
            while (di < dcount && len < 3) {
                octVal = octVal * 10 + (digits[di] - '0');
                di++;
                len++;
            }

            // Clamp to 255 if needed
            if (octVal > 255) {
                octVal = 255;
                valid = false;
            }

            // Write as 3-digit zero-padded number
            pos += sprintf(out + pos, "%03d", octVal);
        }

        if (oct < 3) {
            out[pos++] = '.';
        }
    }

    out[pos] = '\0';
    return valid;
}

static void timeFormat(const char *in, char *out)
{
    char digits[6] = {0};
    int dcount = 0;

    // Extract digits (max 6)
    for (int i = 0; in[i] && dcount < 6; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[dcount++] = in[i];
        }
    }

    int pos = 0;
    int di = 0;

    // HH
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = ':';

    // MM
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = ':';

    // SS
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos] = '\0';
}

static bool timeValidate(const char *in)
{
    char digits[6];
    int len = 0;

    for (int i = 0; in[i] && len < 6; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[len++] = in[i];
        }
    }

    if (len != 6) return false;

    int hh = (digits[0]-'0') * 10 + (digits[1]-'0');
    int mm = (digits[2]-'0') * 10 + (digits[3]-'0');
    int ss = (digits[4]-'0') * 10 + (digits[5]-'0');

    if (hh > 23) return false;
    if (mm > 59) return false;
    if (ss > 59) return false;

    return true;
}

static void dateFormat(const char *in, char *out)
{
    char digits[8] = {0};
    int dcount = 0;

    // Extract digits (max 8)
    for (int i = 0; in[i] && dcount < 8; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[dcount++] = in[i];
        }
    }

    int pos = 0;
    int di = 0;

    // YYYY
    for (int i = 0; i < 4; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = '/';

    // MM
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos++] = '/';

    // DD
    for (int i = 0; i < 2; i++) {
        out[pos++] = (di < dcount) ? digits[di++] : '-';
    }

    out[pos] = '\0';
}

static bool dateValidate(const char *in)
{
    char digits[8];
    int len = 0;

    for (int i = 0; in[i] && len < 8; i++) {
        if (isDigit((unsigned char)in[i])) {
            digits[len++] = in[i];
        }
    }

    if (len != 8) return false;

    int year = 0, month = 0, day = 0;

    // YYYY
    for (int i = 0; i < 4; i++)
        year = year * 10 + (digits[i] - '0');

    // MM
    for (int i = 4; i < 6; i++)
        month = month * 10 + (digits[i] - '0');

    // DD
    for (int i = 6; i < 8; i++)
        day = day * 10 + (digits[i] - '0');

    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;

    // Optional: better day validation per month
    int maxDay = 31;
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        maxDay = leap ? 29 : 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        maxDay = 30;
    }

    return day <= maxDay;
}

static void showMaxError(State *state) {
    switch (inMode) {
    case IN_MODE_AMOUNT:
        GOTO_INFO(state, state, phraseGetDef(PHRASE_ERROR),
                    phraseGetDef(PHRASE_AMOUNT_EXCEED));
        break;
    case IN_MODE_PASSWORD:
        break;
    case IN_MODE_NUMBERS:
        GOTO_INFO(state, state, phraseGetDef(PHRASE_ERROR),
                    phraseGetDef(PHRASE_INPUT_EXCEED));
        break;
    default:
        break;
    }
}

static void handleInput(State *state, KeypadEvent *ev)
{
    bool isPassword = inMode == IN_MODE_PASSWORD ? true : false;
    bool isAmount = inMode == IN_MODE_AMOUNT ? true : false;
    bool isIp = inMode == IN_MODE_IP ? true : false;
    if (ev->key == KEY_CLEAR) {
        deleteChar(input);
        deleteChar(password);
    } else {
        if (idx >= maxIn) {
            showMaxError(state);
            return;
        }
        appendChar(input, INPUT_MAX_LEN,
                   isPassword ? '*' : ev->keyStr);
        if (isPassword) {
            appendChar(password, INPUT_MAX_LEN, ev->keyStr);
        }
    }

    if (isAmount) {
        amountSeparator(input, amountStr, INPUT_MAX_LEN);
        LV_SET_TEXT(inputBox.textBox, amountStr);
    } else if (isIp) {
        char buf[24];
        ipFormatLeftAligned(input, buf);
        lv_label_set_recolor(inputBox.textBox, true);
        LV_SET_TEXT(inputBox.textBox, buf);
    } else if (inMode == IN_MODE_DATE) {
        char buf[16];
        dateFormat(input, buf);
        LV_SET_TEXT(inputBox.textBox, buf);
    } else if (inMode == IN_MODE_TIME) {
        char buf[16];
        timeFormat(input, buf);
        LV_SET_TEXT(inputBox.textBox, buf);
    } else {
        LV_SET_TEXT(inputBox.textBox, input);
    }
    if (ev->key <= KEY_9 && idx < maxIn) {
        idx++;
    } else if (ev->key == KEY_CLEAR && idx > 0) {
        idx--;
    }
}

static void handleAlpahb(State *state, KeypadEvent *ev)
{
    if (ev->key == KEY_CLEAR) {
        deleteChar(input);
    } else {
        alphebatic()->addKey(input, INPUT_MAX_LEN, ev->key);
    }
    LV_SET_TEXT(inputBox.textBox, input);
}

STATE_DEF_HANDLE(Input, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        if (state->prev)
            SM_GOTO(state->prev);
        else
            LOG_WARN("Input state: previous state is not set.");
        return;
    } else if (ev->key == KEY_ENTER) {
        if (state->next) {
            if (inMode == IN_MODE_PASSWORD) {
                if (idx != maxIn) {
                    return;
                }
            } else if (inMode == IN_MODE_IP) {
                if (!ipValidateDigits(input)) {
                    GOTO_INFO(state, state, phraseGetDef(PHRASE_ERROR),
                                phraseGetDef(PHRASE_INVALID_IP));
                    return;
                }
            } else if (inMode == IN_MODE_DATE) {
                if (!dateValidate(input)) {
                    GOTO_INFO(state, state, phraseGetDef(PHRASE_ERROR),
                                phraseGetDef(PHRASE_INVALID_DATE));
                    return;
                }
            } else if (inMode == IN_MODE_TIME) {
                if (!timeValidate(input)) {
                    GOTO_INFO(state, state, phraseGetDef(PHRASE_ERROR),
                                phraseGetDef(PHRASE_INVALID_TIME));
                    return;
                }
            }
            SM_GOTO(state->next);
        } else
            LOG_WARN("Input state: next state is not set.");
    } 

    if (inMode == IN_MODE_ALPHAB) {
        handleAlpahb(state, ev);
        return;
    }
    if (ev->key > KEY_9 && ev->key != KEY_CLEAR )
        return;

    handleInput(state, ev);
}

static void createUi() {
    title = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(title, FONT_20);
    LV_SET_TEXT_COLOR(title, 0xFF4E4E);
    LV_SET_SIZE(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(title, LV_ALIGN_CENTER, 0, -70);

    info = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(info, FONT_16);
    LV_SET_TEXT_COLOR(info, 0x333333);
    LV_SET_SIZE(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    LV_ALIGN(info, LV_ALIGN_CENTER, 0, -30);
    
    uiInputBox(&inputBox, disp()->screen);
    LV_ALIGN(inputBox.main, LV_ALIGN_CENTER, 0, 10);
    uiButton(&confirmBut, disp()->screen, 0x68DD40, phraseGetDef(PHRASE_CONFIRM));
    LV_ALIGN(confirmBut.main, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    uiButton(&confirmBut, disp()->screen, 0xFF4E4E, phraseGetDef(PHRASE_CANCEL));
    LV_ALIGN(cancelBut.main, LV_ALIGN_BOTTOM_LEFT, 5, -10);

    LV_SET_TEXT(inputBox.textBox, "");
}

static void setMode(InputMode_t mode) {
    inMode = mode;
}

static void setData(const char *dtitle, const char *dinfo) {
    LV_SET_TEXT(title, dtitle);
    LV_SET_TEXT(info, dinfo);
}

static void setMax(int val) {
    maxIn = val;
}

static void setInput(const char *in) {
    snprintf(input,
        INPUT_MAX_LEN, "%s", in);
    idx = strlen(input);
}

static void reset() {
    clearStr(password);
    clearStr(input);
    idx = 0;
    LV_SET_TEXT(inputBox.textBox, "");
    LV_SET_TEXT(title, "");
    LV_SET_TEXT(info, "");
    maxIn = INPUT_MAX_LEN;
    memset(&ip, 0, sizeof(IpInput));
}

STATE_DEF_ENTER(Input) {
    LV_SHOW(inputBox.main);
    LV_SHOW(confirmBut.main);
    LV_SHOW(cancelBut.main);
    LV_SHOW(title);
    LV_SHOW(info);
    if (inMode == IN_MODE_IP) {
        char buf[24];
        ipFormatLeftAligned(input, buf);
        LV_SET_TEXT(inputBox.textBox, buf);
    } else if (inMode == IN_MODE_AMOUNT) {
        amountSeparator(input, amountStr, INPUT_MAX_LEN);
        LV_SET_TEXT(inputBox.textBox, amountStr);
    } else if (inMode == IN_MODE_DATE) {
        char buf[16];
        dateFormat(input, buf);
        LV_SET_TEXT(inputBox.textBox, buf);
    }else if (inMode == IN_MODE_TIME) {
        char buf[16];
        timeFormat(input, buf);
        LV_SET_TEXT(inputBox.textBox, buf);
    } else if (inMode == IN_MODE_NUMBERS) {
        LV_SET_TEXT(inputBox.textBox, input);
    }
}

STATE_DEF_EXIT(Input) {
    Input *in = STATE_INPUT;
    if (in->out) {
        snprintf(in->out, maxIn + 1, "%s", in->input);
    }
    LV_HIDE(inputBox.main);
    LV_HIDE(confirmBut.main);
    LV_HIDE(cancelBut.main);
    LV_HIDE(title);
    LV_HIDE(info);
}

STATE_DEF_HANDLE(Input, TimeOutEvent) {

}

OOP_CTOR(Input, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Input);
    self->base.vtable.exit = STATE_EXIT(Input);
    self->base.vtable.handleKeypad = STATE_HANDLE(Input, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Input, TimeOutEvent);
    self->setMode = setMode;
    self->setData = setData;
    self->setMax = setMax;
    self->reset = reset;
    self->setInput = setInput;
    input = (char*)MEM_ALLOC(INPUT_MAX_LEN);
    password = (char*)MEM_ALLOC(PASS_MAX_LEN + 1);
    self->input = input;
    self->password = password;
    amountStr = (char*)MEM_ALLOC(INPUT_MAX_LEN);
    createUi();
}