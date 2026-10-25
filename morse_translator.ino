/*
  Morse Code Translator - Arduino UNO

  Wiring (from your diagram):
    Push button  -> D2 and GND  (uses internal pull-up, no resistor needed)
    Buzzer (+)   -> D8, Buzzer (-) -> GND
    Servo signal -> D9, Servo red -> 5V, Servo brown/black -> GND

  Input  (button):  short press = dot, long press = dash
                    short pause = end of letter, long pause = end of word
                    Decoded letters appear on the Serial Monitor.
  Output (serial):  type text in the Serial Monitor and press Enter.
                    The servo lowers the pen for a short time (dot) or a
                    long time (dash); the buzzer sounds along with it.

  Serial Monitor settings: 9600 baud, line ending "Newline" (or "Both NL & CR").
*/

#include <Servo.h>

// ---------------- Pins ----------------
const byte BUTTON_PIN = 2;
const byte BUZZER_PIN = 8;
const byte SERVO_PIN  = 9;

// ---------------- Servo angles (adjust to your arm) ----------------
const int PEN_UP_ANGLE   = 90;   // pen lifted off the paper
const int PEN_DOWN_ANGLE = 60;   // pen touching the paper

// ---------------- Button input timing (ms) ----------------
const unsigned long DEBOUNCE_MS   = 30;
const unsigned long DASH_MIN_MS   = 400;   // press >= this is a dash, else a dot
const unsigned long LETTER_GAP_MS = 800;   // pause >= this ends a letter
const unsigned long WORD_GAP_MS   = 2000;  // pause >= this ends a word

// ---------------- Output (servo + buzzer) timing ----------------
// Standard Morse ratios: dot = 1 unit, dash = 3 units,
// gap between symbols = 1 unit, between letters = 3, between words = 7.
const unsigned long UNIT_MS = 250;         // increase for slower writing

const int BUZZER_FREQ = 800;               // Hz

// ---------------- Morse tables ----------------
const char* LETTERS[26] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--.."
};
const char* DIGITS[10] = {
  "-----", ".----", "..---", "...--", "....-",
  ".....", "-....", "--...", "---..", "----."
};

Servo penServo;

// ---------------- Button state ----------------
const byte MAX_SYMBOLS = 6;
char symbolBuf[MAX_SYMBOLS + 1];
byte symbolCount = 0;

bool buttonDown = false;
unsigned long lastChangeTime = 0;
unsigned long pressStart = 0;
unsigned long releaseTime = 0;
bool waitingForWordGap = false;

// ------------------------------------------------------------
const char* morseFor(char c) {
  if (c >= 'A' && c <= 'Z') return LETTERS[c - 'A'];
  if (c >= '0' && c <= '9') return DIGITS[c - '0'];
  return NULL;
}

char decodeMorse(const char* code) {
  for (byte i = 0; i < 26; i++) if (strcmp(code, LETTERS[i]) == 0) return 'A' + i;
  for (byte i = 0; i < 10; i++) if (strcmp(code, DIGITS[i]) == 0) return '0' + i;
  return '?';
}

// ---------------- Output: draw one dot/dash ----------------
void markSymbol(unsigned long duration) {
  penServo.write(PEN_DOWN_ANGLE);
  tone(BUZZER_PIN, BUZZER_FREQ);
  delay(duration);
  penServo.write(PEN_UP_ANGLE);
  noTone(BUZZER_PIN);
}

void transmitText(String text) {
  text.toUpperCase();
  Serial.print("Writing: ");
  Serial.println(text);

  for (unsigned int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (c == ' ') {
      Serial.println("  [word gap]");
      delay(UNIT_MS * 4);               // 3 already waited after the letter -> 7 total
      continue;
    }

    const char* code = morseFor(c);
    if (code == NULL) continue;         // skip unsupported characters

    Serial.print("  ");
    Serial.print(c);
    Serial.print(" = ");
    Serial.println(code);

    for (byte k = 0; code[k] != '\0'; k++) {
      markSymbol(code[k] == '.' ? UNIT_MS : UNIT_MS * 3);
      delay(UNIT_MS);                   // gap between symbols
    }
    delay(UNIT_MS * 2);                 // 1 + 2 = 3 units between letters
  }
  Serial.println("Done writing.\n");
}

// ---------------- Input: finish a letter typed on the button ----------------
void flushLetter() {
  if (symbolCount == 0) return;
  symbolBuf[symbolCount] = '\0';
  Serial.print("  ->  ");
  Serial.println(decodeMorse(symbolBuf));
  symbolCount = 0;
  waitingForWordGap = true;
}

void handleButton() {
  unsigned long now = millis();
  bool down = (digitalRead(BUTTON_PIN) == LOW);

  // Pressed
  if (down && !buttonDown && now - lastChangeTime > DEBOUNCE_MS) {
    buttonDown = true;
    lastChangeTime = now;
    pressStart = now;
    waitingForWordGap = false;
    tone(BUZZER_PIN, BUZZER_FREQ);
  }
  // Released
  else if (!down && buttonDown && now - lastChangeTime > DEBOUNCE_MS) {
    buttonDown = false;
    lastChangeTime = now;
    releaseTime = now;
    noTone(BUZZER_PIN);

    char sym = (now - pressStart >= DASH_MIN_MS) ? '-' : '.';
    symbolBuf[symbolCount++] = sym;
    Serial.print(sym);

    if (symbolCount >= MAX_SYMBOLS) flushLetter();   // buffer full
  }

  // Pauses
  if (!buttonDown) {
    if (symbolCount > 0 && now - releaseTime >= LETTER_GAP_MS) {
      flushLetter();
    }
    if (waitingForWordGap && now - releaseTime >= WORD_GAP_MS) {
      Serial.println("  [space]");
      waitingForWordGap = false;
    }
  }
}

// ---------------- Setup / loop ----------------
void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  penServo.attach(SERVO_PIN);
  penServo.write(PEN_UP_ANGLE);

  Serial.begin(9600);
  Serial.println("Morse Translator ready.");
  Serial.println("Button: short = dot, long = dash.");
  Serial.println("Type text + Enter to have the servo write it in Morse.\n");
}

void loop() {
  handleButton();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      noTone(BUZZER_PIN);
      transmitText(line);
    }
  }
}
