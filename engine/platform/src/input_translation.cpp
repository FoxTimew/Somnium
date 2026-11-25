// Translates the pinned SDL event vocabulary at the collection boundary.
// Unknown controls remain neutral and repeated keyboard presses are discarded.
#include "input_translation.hpp"
#include <SDL3/SDL_mouse.h>

namespace somnium::platform::detail {
std::optional<core::InputEvent> translateInput(const SDL_Event& event, std::uint64_t step) {
    core::InputEvent result;
    result.step = step;
    using core::InputEventKind;
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (event.key.repeat)
            return {};
        result.kind = InputEventKind::Key;
        result.down = event.key.down;
        switch (event.key.scancode) {
        case SDL_SCANCODE_A:
            result.key = core::Key::A;
            break;
        case SDL_SCANCODE_B:
            result.key = core::Key::B;
            break;
        case SDL_SCANCODE_C:
            result.key = core::Key::C;
            break;
        case SDL_SCANCODE_D:
            result.key = core::Key::D;
            break;
        case SDL_SCANCODE_E:
            result.key = core::Key::E;
            break;
        case SDL_SCANCODE_F:
            result.key = core::Key::F;
            break;
        case SDL_SCANCODE_G:
            result.key = core::Key::G;
            break;
        case SDL_SCANCODE_H:
            result.key = core::Key::H;
            break;
        case SDL_SCANCODE_I:
            result.key = core::Key::I;
            break;
        case SDL_SCANCODE_J:
            result.key = core::Key::J;
            break;
        case SDL_SCANCODE_K:
            result.key = core::Key::K;
            break;
        case SDL_SCANCODE_L:
            result.key = core::Key::L;
            break;
        case SDL_SCANCODE_M:
            result.key = core::Key::M;
            break;
        case SDL_SCANCODE_N:
            result.key = core::Key::N;
            break;
        case SDL_SCANCODE_O:
            result.key = core::Key::O;
            break;
        case SDL_SCANCODE_P:
            result.key = core::Key::P;
            break;
        case SDL_SCANCODE_Q:
            result.key = core::Key::Q;
            break;
        case SDL_SCANCODE_R:
            result.key = core::Key::R;
            break;
        case SDL_SCANCODE_S:
            result.key = core::Key::S;
            break;
        case SDL_SCANCODE_T:
            result.key = core::Key::T;
            break;
        case SDL_SCANCODE_U:
            result.key = core::Key::U;
            break;
        case SDL_SCANCODE_V:
            result.key = core::Key::V;
            break;
        case SDL_SCANCODE_W:
            result.key = core::Key::W;
            break;
        case SDL_SCANCODE_X:
            result.key = core::Key::X;
            break;
        case SDL_SCANCODE_Y:
            result.key = core::Key::Y;
            break;
        case SDL_SCANCODE_Z:
            result.key = core::Key::Z;
            break;
        case SDL_SCANCODE_1:
            result.key = core::Key::Digit1;
            break;
        case SDL_SCANCODE_2:
            result.key = core::Key::Digit2;
            break;
        case SDL_SCANCODE_3:
            result.key = core::Key::Digit3;
            break;
        case SDL_SCANCODE_4:
            result.key = core::Key::Digit4;
            break;
        case SDL_SCANCODE_5:
            result.key = core::Key::Digit5;
            break;
        case SDL_SCANCODE_6:
            result.key = core::Key::Digit6;
            break;
        case SDL_SCANCODE_7:
            result.key = core::Key::Digit7;
            break;
        case SDL_SCANCODE_8:
            result.key = core::Key::Digit8;
            break;
        case SDL_SCANCODE_9:
            result.key = core::Key::Digit9;
            break;
        case SDL_SCANCODE_0:
            result.key = core::Key::Digit0;
            break;
        case SDL_SCANCODE_RETURN:
            result.key = core::Key::Return;
            break;
        case SDL_SCANCODE_ESCAPE:
            result.key = core::Key::Escape;
            break;
        case SDL_SCANCODE_BACKSPACE:
            result.key = core::Key::Backspace;
            break;
        case SDL_SCANCODE_TAB:
            result.key = core::Key::Tab;
            break;
        case SDL_SCANCODE_SPACE:
            result.key = core::Key::Space;
            break;
        case SDL_SCANCODE_MINUS:
            result.key = core::Key::Minus;
            break;
        case SDL_SCANCODE_EQUALS:
            result.key = core::Key::Equals;
            break;
        case SDL_SCANCODE_LEFTBRACKET:
            result.key = core::Key::Leftbracket;
            break;
        case SDL_SCANCODE_RIGHTBRACKET:
            result.key = core::Key::Rightbracket;
            break;
        case SDL_SCANCODE_BACKSLASH:
            result.key = core::Key::Backslash;
            break;
        case SDL_SCANCODE_NONUSHASH:
            result.key = core::Key::Nonushash;
            break;
        case SDL_SCANCODE_SEMICOLON:
            result.key = core::Key::Semicolon;
            break;
        case SDL_SCANCODE_APOSTROPHE:
            result.key = core::Key::Apostrophe;
            break;
        case SDL_SCANCODE_GRAVE:
            result.key = core::Key::Grave;
            break;
        case SDL_SCANCODE_COMMA:
            result.key = core::Key::Comma;
            break;
        case SDL_SCANCODE_PERIOD:
            result.key = core::Key::Period;
            break;
        case SDL_SCANCODE_SLASH:
            result.key = core::Key::Slash;
            break;
        case SDL_SCANCODE_CAPSLOCK:
            result.key = core::Key::CapsLock;
            break;
        case SDL_SCANCODE_F1:
            result.key = core::Key::F1;
            break;
        case SDL_SCANCODE_F2:
            result.key = core::Key::F2;
            break;
        case SDL_SCANCODE_F3:
            result.key = core::Key::F3;
            break;
        case SDL_SCANCODE_F4:
            result.key = core::Key::F4;
            break;
        case SDL_SCANCODE_F5:
            result.key = core::Key::F5;
            break;
        case SDL_SCANCODE_F6:
            result.key = core::Key::F6;
            break;
        case SDL_SCANCODE_F7:
            result.key = core::Key::F7;
            break;
        case SDL_SCANCODE_F8:
            result.key = core::Key::F8;
            break;
        case SDL_SCANCODE_F9:
            result.key = core::Key::F9;
            break;
        case SDL_SCANCODE_F10:
            result.key = core::Key::F10;
            break;
        case SDL_SCANCODE_F11:
            result.key = core::Key::F11;
            break;
        case SDL_SCANCODE_F12:
            result.key = core::Key::F12;
            break;
        case SDL_SCANCODE_PRINTSCREEN:
            result.key = core::Key::PrintScreen;
            break;
        case SDL_SCANCODE_SCROLLLOCK:
            result.key = core::Key::ScrollLock;
            break;
        case SDL_SCANCODE_PAUSE:
            result.key = core::Key::Pause;
            break;
        case SDL_SCANCODE_INSERT:
            result.key = core::Key::Insert;
            break;
        case SDL_SCANCODE_HOME:
            result.key = core::Key::Home;
            break;
        case SDL_SCANCODE_PAGEUP:
            result.key = core::Key::PageUp;
            break;
        case SDL_SCANCODE_DELETE:
            result.key = core::Key::Delete;
            break;
        case SDL_SCANCODE_END:
            result.key = core::Key::End;
            break;
        case SDL_SCANCODE_PAGEDOWN:
            result.key = core::Key::PageDown;
            break;
        case SDL_SCANCODE_RIGHT:
            result.key = core::Key::Right;
            break;
        case SDL_SCANCODE_LEFT:
            result.key = core::Key::Left;
            break;
        case SDL_SCANCODE_DOWN:
            result.key = core::Key::Down;
            break;
        case SDL_SCANCODE_UP:
            result.key = core::Key::Up;
            break;
        case SDL_SCANCODE_NUMLOCKCLEAR:
            result.key = core::Key::NumLockClear;
            break;
        case SDL_SCANCODE_KP_DIVIDE:
            result.key = core::Key::KpDivide;
            break;
        case SDL_SCANCODE_KP_MULTIPLY:
            result.key = core::Key::KpMultiply;
            break;
        case SDL_SCANCODE_KP_MINUS:
            result.key = core::Key::KpMinus;
            break;
        case SDL_SCANCODE_KP_PLUS:
            result.key = core::Key::KpPlus;
            break;
        case SDL_SCANCODE_KP_ENTER:
            result.key = core::Key::KpEnter;
            break;
        case SDL_SCANCODE_KP_1:
            result.key = core::Key::Kp1;
            break;
        case SDL_SCANCODE_KP_2:
            result.key = core::Key::Kp2;
            break;
        case SDL_SCANCODE_KP_3:
            result.key = core::Key::Kp3;
            break;
        case SDL_SCANCODE_KP_4:
            result.key = core::Key::Kp4;
            break;
        case SDL_SCANCODE_KP_5:
            result.key = core::Key::Kp5;
            break;
        case SDL_SCANCODE_KP_6:
            result.key = core::Key::Kp6;
            break;
        case SDL_SCANCODE_KP_7:
            result.key = core::Key::Kp7;
            break;
        case SDL_SCANCODE_KP_8:
            result.key = core::Key::Kp8;
            break;
        case SDL_SCANCODE_KP_9:
            result.key = core::Key::Kp9;
            break;
        case SDL_SCANCODE_KP_0:
            result.key = core::Key::Kp0;
            break;
        case SDL_SCANCODE_KP_PERIOD:
            result.key = core::Key::KpPeriod;
            break;
        case SDL_SCANCODE_NONUSBACKSLASH:
            result.key = core::Key::Nonusbackslash;
            break;
        case SDL_SCANCODE_APPLICATION:
            result.key = core::Key::Application;
            break;
        case SDL_SCANCODE_POWER:
            result.key = core::Key::Power;
            break;
        case SDL_SCANCODE_KP_EQUALS:
            result.key = core::Key::KpEquals;
            break;
        case SDL_SCANCODE_F13:
            result.key = core::Key::F13;
            break;
        case SDL_SCANCODE_F14:
            result.key = core::Key::F14;
            break;
        case SDL_SCANCODE_F15:
            result.key = core::Key::F15;
            break;
        case SDL_SCANCODE_F16:
            result.key = core::Key::F16;
            break;
        case SDL_SCANCODE_F17:
            result.key = core::Key::F17;
            break;
        case SDL_SCANCODE_F18:
            result.key = core::Key::F18;
            break;
        case SDL_SCANCODE_F19:
            result.key = core::Key::F19;
            break;
        case SDL_SCANCODE_F20:
            result.key = core::Key::F20;
            break;
        case SDL_SCANCODE_F21:
            result.key = core::Key::F21;
            break;
        case SDL_SCANCODE_F22:
            result.key = core::Key::F22;
            break;
        case SDL_SCANCODE_F23:
            result.key = core::Key::F23;
            break;
        case SDL_SCANCODE_F24:
            result.key = core::Key::F24;
            break;
        case SDL_SCANCODE_EXECUTE:
            result.key = core::Key::Execute;
            break;
        case SDL_SCANCODE_HELP:
            result.key = core::Key::Help;
            break;
        case SDL_SCANCODE_MENU:
            result.key = core::Key::Menu;
            break;
        case SDL_SCANCODE_SELECT:
            result.key = core::Key::Select;
            break;
        case SDL_SCANCODE_STOP:
            result.key = core::Key::Stop;
            break;
        case SDL_SCANCODE_AGAIN:
            result.key = core::Key::Again;
            break;
        case SDL_SCANCODE_UNDO:
            result.key = core::Key::Undo;
            break;
        case SDL_SCANCODE_CUT:
            result.key = core::Key::Cut;
            break;
        case SDL_SCANCODE_COPY:
            result.key = core::Key::Copy;
            break;
        case SDL_SCANCODE_PASTE:
            result.key = core::Key::Paste;
            break;
        case SDL_SCANCODE_FIND:
            result.key = core::Key::Find;
            break;
        case SDL_SCANCODE_MUTE:
            result.key = core::Key::Mute;
            break;
        case SDL_SCANCODE_VOLUMEUP:
            result.key = core::Key::Volumeup;
            break;
        case SDL_SCANCODE_VOLUMEDOWN:
            result.key = core::Key::Volumedown;
            break;
        case SDL_SCANCODE_KP_COMMA:
            result.key = core::Key::KpComma;
            break;
        case SDL_SCANCODE_KP_EQUALSAS400:
            result.key = core::Key::KpEqualsas400;
            break;
        case SDL_SCANCODE_INTERNATIONAL1:
            result.key = core::Key::International1;
            break;
        case SDL_SCANCODE_INTERNATIONAL2:
            result.key = core::Key::International2;
            break;
        case SDL_SCANCODE_INTERNATIONAL3:
            result.key = core::Key::International3;
            break;
        case SDL_SCANCODE_INTERNATIONAL4:
            result.key = core::Key::International4;
            break;
        case SDL_SCANCODE_INTERNATIONAL5:
            result.key = core::Key::International5;
            break;
        case SDL_SCANCODE_INTERNATIONAL6:
            result.key = core::Key::International6;
            break;
        case SDL_SCANCODE_INTERNATIONAL7:
            result.key = core::Key::International7;
            break;
        case SDL_SCANCODE_INTERNATIONAL8:
            result.key = core::Key::International8;
            break;
        case SDL_SCANCODE_INTERNATIONAL9:
            result.key = core::Key::International9;
            break;
        case SDL_SCANCODE_LANG1:
            result.key = core::Key::Lang1;
            break;
        case SDL_SCANCODE_LANG2:
            result.key = core::Key::Lang2;
            break;
        case SDL_SCANCODE_LANG3:
            result.key = core::Key::Lang3;
            break;
        case SDL_SCANCODE_LANG4:
            result.key = core::Key::Lang4;
            break;
        case SDL_SCANCODE_LANG5:
            result.key = core::Key::Lang5;
            break;
        case SDL_SCANCODE_LANG6:
            result.key = core::Key::Lang6;
            break;
        case SDL_SCANCODE_LANG7:
            result.key = core::Key::Lang7;
            break;
        case SDL_SCANCODE_LANG8:
            result.key = core::Key::Lang8;
            break;
        case SDL_SCANCODE_LANG9:
            result.key = core::Key::Lang9;
            break;
        case SDL_SCANCODE_ALTERASE:
            result.key = core::Key::Alterase;
            break;
        case SDL_SCANCODE_SYSREQ:
            result.key = core::Key::Sysreq;
            break;
        case SDL_SCANCODE_CANCEL:
            result.key = core::Key::Cancel;
            break;
        case SDL_SCANCODE_CLEAR:
            result.key = core::Key::Clear;
            break;
        case SDL_SCANCODE_PRIOR:
            result.key = core::Key::Prior;
            break;
        case SDL_SCANCODE_RETURN2:
            result.key = core::Key::Return2;
            break;
        case SDL_SCANCODE_SEPARATOR:
            result.key = core::Key::Separator;
            break;
        case SDL_SCANCODE_OUT:
            result.key = core::Key::Out;
            break;
        case SDL_SCANCODE_OPER:
            result.key = core::Key::Oper;
            break;
        case SDL_SCANCODE_CLEARAGAIN:
            result.key = core::Key::Clearagain;
            break;
        case SDL_SCANCODE_CRSEL:
            result.key = core::Key::Crsel;
            break;
        case SDL_SCANCODE_EXSEL:
            result.key = core::Key::Exsel;
            break;
        case SDL_SCANCODE_FRONT:
            result.key = core::Key::Front;
            break;
        case SDL_SCANCODE_KP_00:
            result.key = core::Key::Kp00;
            break;
        case SDL_SCANCODE_KP_000:
            result.key = core::Key::Kp000;
            break;
        case SDL_SCANCODE_THOUSANDSSEPARATOR:
            result.key = core::Key::Thousandsseparator;
            break;
        case SDL_SCANCODE_DECIMALSEPARATOR:
            result.key = core::Key::Decimalseparator;
            break;
        case SDL_SCANCODE_CURRENCYUNIT:
            result.key = core::Key::Currencyunit;
            break;
        case SDL_SCANCODE_CURRENCYSUBUNIT:
            result.key = core::Key::Currencysubunit;
            break;
        case SDL_SCANCODE_KP_LEFTPAREN:
            result.key = core::Key::KpLeftparen;
            break;
        case SDL_SCANCODE_KP_RIGHTPAREN:
            result.key = core::Key::KpRightparen;
            break;
        case SDL_SCANCODE_KP_LEFTBRACE:
            result.key = core::Key::KpLeftbrace;
            break;
        case SDL_SCANCODE_KP_RIGHTBRACE:
            result.key = core::Key::KpRightbrace;
            break;
        case SDL_SCANCODE_KP_TAB:
            result.key = core::Key::KpTab;
            break;
        case SDL_SCANCODE_KP_BACKSPACE:
            result.key = core::Key::KpBackspace;
            break;
        case SDL_SCANCODE_KP_A:
            result.key = core::Key::KpA;
            break;
        case SDL_SCANCODE_KP_B:
            result.key = core::Key::KpB;
            break;
        case SDL_SCANCODE_KP_C:
            result.key = core::Key::KpC;
            break;
        case SDL_SCANCODE_KP_D:
            result.key = core::Key::KpD;
            break;
        case SDL_SCANCODE_KP_E:
            result.key = core::Key::KpE;
            break;
        case SDL_SCANCODE_KP_F:
            result.key = core::Key::KpF;
            break;
        case SDL_SCANCODE_KP_XOR:
            result.key = core::Key::KpXor;
            break;
        case SDL_SCANCODE_KP_POWER:
            result.key = core::Key::KpPower;
            break;
        case SDL_SCANCODE_KP_PERCENT:
            result.key = core::Key::KpPercent;
            break;
        case SDL_SCANCODE_KP_LESS:
            result.key = core::Key::KpLess;
            break;
        case SDL_SCANCODE_KP_GREATER:
            result.key = core::Key::KpGreater;
            break;
        case SDL_SCANCODE_KP_AMPERSAND:
            result.key = core::Key::KpAmpersand;
            break;
        case SDL_SCANCODE_KP_DBLAMPERSAND:
            result.key = core::Key::KpDblampersand;
            break;
        case SDL_SCANCODE_KP_VERTICALBAR:
            result.key = core::Key::KpVerticalbar;
            break;
        case SDL_SCANCODE_KP_DBLVERTICALBAR:
            result.key = core::Key::KpDblverticalbar;
            break;
        case SDL_SCANCODE_KP_COLON:
            result.key = core::Key::KpColon;
            break;
        case SDL_SCANCODE_KP_HASH:
            result.key = core::Key::KpHash;
            break;
        case SDL_SCANCODE_KP_SPACE:
            result.key = core::Key::KpSpace;
            break;
        case SDL_SCANCODE_KP_AT:
            result.key = core::Key::KpAt;
            break;
        case SDL_SCANCODE_KP_EXCLAM:
            result.key = core::Key::KpExclam;
            break;
        case SDL_SCANCODE_KP_MEMSTORE:
            result.key = core::Key::KpMemstore;
            break;
        case SDL_SCANCODE_KP_MEMRECALL:
            result.key = core::Key::KpMemrecall;
            break;
        case SDL_SCANCODE_KP_MEMCLEAR:
            result.key = core::Key::KpMemclear;
            break;
        case SDL_SCANCODE_KP_MEMADD:
            result.key = core::Key::KpMemadd;
            break;
        case SDL_SCANCODE_KP_MEMSUBTRACT:
            result.key = core::Key::KpMemsubtract;
            break;
        case SDL_SCANCODE_KP_MEMMULTIPLY:
            result.key = core::Key::KpMemmultiply;
            break;
        case SDL_SCANCODE_KP_MEMDIVIDE:
            result.key = core::Key::KpMemdivide;
            break;
        case SDL_SCANCODE_KP_PLUSMINUS:
            result.key = core::Key::KpPlusminus;
            break;
        case SDL_SCANCODE_KP_CLEAR:
            result.key = core::Key::KpClear;
            break;
        case SDL_SCANCODE_KP_CLEARENTRY:
            result.key = core::Key::KpClearentry;
            break;
        case SDL_SCANCODE_KP_BINARY:
            result.key = core::Key::KpBinary;
            break;
        case SDL_SCANCODE_KP_OCTAL:
            result.key = core::Key::KpOctal;
            break;
        case SDL_SCANCODE_KP_DECIMAL:
            result.key = core::Key::KpDecimal;
            break;
        case SDL_SCANCODE_KP_HEXADECIMAL:
            result.key = core::Key::KpHexadecimal;
            break;
        case SDL_SCANCODE_LCTRL:
            result.key = core::Key::LeftControl;
            break;
        case SDL_SCANCODE_LSHIFT:
            result.key = core::Key::LeftShift;
            break;
        case SDL_SCANCODE_LALT:
            result.key = core::Key::LeftAlt;
            break;
        case SDL_SCANCODE_LGUI:
            result.key = core::Key::LeftGui;
            break;
        case SDL_SCANCODE_RCTRL:
            result.key = core::Key::RightControl;
            break;
        case SDL_SCANCODE_RSHIFT:
            result.key = core::Key::RightShift;
            break;
        case SDL_SCANCODE_RALT:
            result.key = core::Key::RightAlt;
            break;
        case SDL_SCANCODE_RGUI:
            result.key = core::Key::RightGui;
            break;
        case SDL_SCANCODE_MODE:
            result.key = core::Key::Mode;
            break;
        case SDL_SCANCODE_SLEEP:
            result.key = core::Key::Sleep;
            break;
        case SDL_SCANCODE_WAKE:
            result.key = core::Key::Wake;
            break;
        case SDL_SCANCODE_CHANNEL_INCREMENT:
            result.key = core::Key::ChannelIncrement;
            break;
        case SDL_SCANCODE_CHANNEL_DECREMENT:
            result.key = core::Key::ChannelDecrement;
            break;
        case SDL_SCANCODE_MEDIA_PLAY:
            result.key = core::Key::MediaPlay;
            break;
        case SDL_SCANCODE_MEDIA_PAUSE:
            result.key = core::Key::MediaPause;
            break;
        case SDL_SCANCODE_MEDIA_RECORD:
            result.key = core::Key::MediaRecord;
            break;
        case SDL_SCANCODE_MEDIA_FAST_FORWARD:
            result.key = core::Key::MediaFastForward;
            break;
        case SDL_SCANCODE_MEDIA_REWIND:
            result.key = core::Key::MediaRewind;
            break;
        case SDL_SCANCODE_MEDIA_NEXT_TRACK:
            result.key = core::Key::MediaNextTrack;
            break;
        case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK:
            result.key = core::Key::MediaPreviousTrack;
            break;
        case SDL_SCANCODE_MEDIA_STOP:
            result.key = core::Key::MediaStop;
            break;
        case SDL_SCANCODE_MEDIA_EJECT:
            result.key = core::Key::MediaEject;
            break;
        case SDL_SCANCODE_MEDIA_PLAY_PAUSE:
            result.key = core::Key::MediaPlayPause;
            break;
        case SDL_SCANCODE_MEDIA_SELECT:
            result.key = core::Key::MediaSelect;
            break;
        case SDL_SCANCODE_AC_NEW:
            result.key = core::Key::AcNew;
            break;
        case SDL_SCANCODE_AC_OPEN:
            result.key = core::Key::AcOpen;
            break;
        case SDL_SCANCODE_AC_CLOSE:
            result.key = core::Key::AcClose;
            break;
        case SDL_SCANCODE_AC_EXIT:
            result.key = core::Key::AcExit;
            break;
        case SDL_SCANCODE_AC_SAVE:
            result.key = core::Key::AcSave;
            break;
        case SDL_SCANCODE_AC_PRINT:
            result.key = core::Key::AcPrint;
            break;
        case SDL_SCANCODE_AC_PROPERTIES:
            result.key = core::Key::AcProperties;
            break;
        case SDL_SCANCODE_AC_SEARCH:
            result.key = core::Key::AcSearch;
            break;
        case SDL_SCANCODE_AC_HOME:
            result.key = core::Key::AcHome;
            break;
        case SDL_SCANCODE_AC_BACK:
            result.key = core::Key::AcBack;
            break;
        case SDL_SCANCODE_AC_FORWARD:
            result.key = core::Key::AcForward;
            break;
        case SDL_SCANCODE_AC_STOP:
            result.key = core::Key::AcStop;
            break;
        case SDL_SCANCODE_AC_REFRESH:
            result.key = core::Key::AcRefresh;
            break;
        case SDL_SCANCODE_AC_BOOKMARKS:
            result.key = core::Key::AcBookmarks;
            break;
        case SDL_SCANCODE_SOFTLEFT:
            result.key = core::Key::Softleft;
            break;
        case SDL_SCANCODE_SOFTRIGHT:
            result.key = core::Key::Softright;
            break;
        case SDL_SCANCODE_CALL:
            result.key = core::Key::Call;
            break;
        case SDL_SCANCODE_ENDCALL:
            result.key = core::Key::Endcall;
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        result.kind = InputEventKind::MouseButton;
        result.down = event.button.down;
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
            result.mouseButton = core::MouseButton::Left;
            break;
        case SDL_BUTTON_RIGHT:
            result.mouseButton = core::MouseButton::Right;
            break;
        case SDL_BUTTON_MIDDLE:
            result.mouseButton = core::MouseButton::Middle;
            break;
        case SDL_BUTTON_X1:
            result.mouseButton = core::MouseButton::Extra1;
            break;
        case SDL_BUTTON_X2:
            result.mouseButton = core::MouseButton::Extra2;
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        result.kind = InputEventKind::MouseMotion;
        result.x = event.motion.xrel;
        result.y = event.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        result.kind = InputEventKind::MouseWheel;
        result.x = event.wheel.x;
        result.y = event.wheel.y;
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            result.x = -result.x;
            result.y = -result.y;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        result.kind = InputEventKind::GamepadButton;
        result.device = event.gbutton.which;
        result.down = event.gbutton.down;
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            result.gamepadButton = core::GamepadButton::South;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            result.gamepadButton = core::GamepadButton::East;
            break;
        case SDL_GAMEPAD_BUTTON_WEST:
            result.gamepadButton = core::GamepadButton::West;
            break;
        case SDL_GAMEPAD_BUTTON_NORTH:
            result.gamepadButton = core::GamepadButton::North;
            break;
        case SDL_GAMEPAD_BUTTON_BACK:
            result.gamepadButton = core::GamepadButton::Back;
            break;
        case SDL_GAMEPAD_BUTTON_GUIDE:
            result.gamepadButton = core::GamepadButton::Guide;
            break;
        case SDL_GAMEPAD_BUTTON_START:
            result.gamepadButton = core::GamepadButton::Start;
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            result.gamepadButton = core::GamepadButton::LeftStick;
            break;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            result.gamepadButton = core::GamepadButton::RightStick;
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            result.gamepadButton = core::GamepadButton::LeftShoulder;
            break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            result.gamepadButton = core::GamepadButton::RightShoulder;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            result.gamepadButton = core::GamepadButton::Up;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            result.gamepadButton = core::GamepadButton::Down;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            result.gamepadButton = core::GamepadButton::Left;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            result.gamepadButton = core::GamepadButton::Right;
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        result.kind = InputEventKind::GamepadAxis;
        result.device = event.gaxis.which;
        result.value = event.gaxis.value;
        switch (event.gaxis.axis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            result.gamepadAxis = core::GamepadAxis::LeftX;
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            result.gamepadAxis = core::GamepadAxis::LeftY;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            result.gamepadAxis = core::GamepadAxis::RightX;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            result.gamepadAxis = core::GamepadAxis::RightY;
            break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            result.gamepadAxis = core::GamepadAxis::LeftTrigger;
            break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            result.gamepadAxis = core::GamepadAxis::RightTrigger;
            break;
        default:
            return {};
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        result.kind = event.type == SDL_EVENT_GAMEPAD_ADDED ? InputEventKind::GamepadConnected
                                                            : InputEventKind::GamepadDisconnected;
        result.device = event.gdevice.which;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        result.kind = InputEventKind::FocusLost;
        break;
    default:
        return {};
    }
    return result;
}
} // namespace somnium::platform::detail
