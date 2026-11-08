
#include <LCDWIKI_GUI.h>
#include <LCDWIKI_KBV.h>
#include <TouchScreen.h>
#include "club_logos.h"

LCDWIKI_KBV mylcd(ILI9486, A3, A2, A1, A0, A4);

#define BLACK       0x0000
#define WHITE       0xFFFF
#define BG          0x0841
#define PANEL       0x10A3
#define PANEL2      0x2128
#define MUTED       0x9CF3
#define LIME        0xB7E0
#define GREEN       0x07E0
#define RED         0xF986
#define GOLD        0xFEA0
#define BORDER      0x39C7

#define YP A1
#define XM A2
#define YM 7
#define XP 6

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

#define RAW_X_LEFT    922
#define RAW_X_RIGHT   194
#define RAW_Y_TOP     936
#define RAW_Y_BOTTOM  202

struct Club {
  const char* name;
  const char* shortName;
  int apiId;
  uint16_t color;
};

Club teams[] = {
  {"Arsenal",     "ARS", 57, 0xD800},
  {"Man City",    "MCI", 65, 0x041F},
  {"Liverpool",   "LIV", 64, 0xC000},
  {"Real Madrid", "RMA", 86, 0xFFFF},
  {"Barcelona",   "BAR", 81, 0xA145},
  {"Man United",  "MUN", 66, 0xC000}
};

const int TEAM_COUNT = 6;
int selectedTeam = -1;

struct TouchTarget {
  int x;
  int y;
};

TouchTarget teamTouchPoints[TEAM_COUNT] = {
  {290, 53},
  {309, 239},
  {182, 96},
  {180, 222},
  {276, 86},
  {67, 218}
};

const int TOUCH_MAX_DISTANCE = 65;

const int BACK_TOUCH_X = 442;
const int BACK_TOUCH_Y = 28;
const int BACK_TOUCH_RADIUS_X = 38;
const int BACK_TOUCH_RADIUS_Y = 35;

bool touchWasDown = false;

String lastHome = "";
String lastAway = "";
String lastDate = "";
String lastScore = "";

String nextHome = "";
String nextAway = "";
String nextDate = "";

String statusMessage = "Select a club";

bool hasLastMatch = false;
bool hasNextMatch = false;

char serialBuffer[220];
byte serialBufferIndex = 0;

void fillRect(int x, int y, int w, int h, uint16_t color) {
  mylcd.Fill_Rect(x, y, w, h, color);
}

void drawRect(int x, int y, int w, int h, uint16_t color) {
  mylcd.Set_Draw_color(color);
  mylcd.Draw_Rectangle(x, y, x + w - 1, y + h - 1);
}

void drawLine(int x1, int y1, int x2, int y2, uint16_t color) {
  mylcd.Set_Draw_color(color);
  mylcd.Draw_Line(x1, y1, x2, y2);
}

void drawText(const String &value, int x, int y, int size,
              uint16_t foreground, uint16_t background) {
  mylcd.Set_Text_colour(foreground);
  mylcd.Set_Text_Back_colour(background);
  mylcd.Set_Text_Size(size);
  mylcd.Print_String((uint8_t*)value.c_str(), x, y);
}

void drawClubLogo(int index, int x, int y) {
  const uint16_t* logo = nullptr;

  switch (index) {
    case 0: logo = logo_arsenal; break;
    case 1: logo = logo_manchester_city; break;
    case 2: logo = logo_liverpool; break;
    case 3: logo = logo_real_madrid; break;
    case 4: logo = logo_barcelona; break;
    case 5: logo = logo_manchester_united; break;
    default: return;
  }

  for (int py = 0; py < 16; py++) {
    for (int px = 0; px < 16; px++) {
      uint16_t color = pgm_read_word(&logo[py * 16 + px]);

      if (color != 0x18E3) {
        mylcd.Set_Draw_color(color);
        mylcd.Draw_Pixel(x + px, y + py);
      }
    }
  }
}

void drawHeader(const String &subtitle) {
  fillRect(0, 0, 480, 76, BG);

  drawText("FOOTBALL", 18, 13, 3, WHITE, BG);
  drawText("HUB", 178, 13, 3, LIME, BG);
  drawText(subtitle, 20, 49, 1, MUTED, BG);

  fillRect(424, 18, 10, 10, LIME);
  drawText("LIVE", 440, 18, 1, LIME, BG);

  drawLine(18, 75, 462, 75, BORDER);
}

int cardX(int index) {
  return (index % 2 == 0) ? 15 : 250;
}

int cardY(int index) {
  return 91 + (index / 2) * 70;
}

void drawTeamCard(int index) {
  int x = cardX(index);
  int y = cardY(index);

  fillRect(x, y, 215, 60, PANEL);
  drawRect(x, y, 215, 60, BORDER);
  fillRect(x, y, 5, 60, teams[index].color);

  fillRect(x + 15, y + 12, 38, 36, PANEL2);
  drawRect(x + 15, y + 12, 38, 36, teams[index].color);

  drawClubLogo(index, x + 22, y + 18);

  drawText(teams[index].name, x + 64, y + 22, 2, WHITE, PANEL);
  drawText(">", x + 190, y + 23, 2, MUTED, PANEL);
}

void drawSelectionScreen() {
  selectedTeam = -1;

  mylcd.Fill_Screen(BG);
  drawHeader("SELECT YOUR CLUB");

  drawText("CHOOSE A TEAM", 18, 80, 1, LIME, BG);

  for (int i = 0; i < TEAM_COUNT; i++) {
    drawTeamCard(i);
  }

  drawLine(18, 302, 462, 302, BORDER);
  drawText("YOUR FOOTBALL MATCH CENTER", 130, 306, 1, MUTED, BG);
}

void drawBackButton() {
  fillRect(15, 13, 85, 42, PANEL);
  drawRect(15, 13, 85, 42, BORDER);
  drawText("< BACK", 28, 28, 1, WHITE, PANEL);
}

String formatDate(String rawDate) {
  if (rawDate.length() < 16) {
    return rawDate;
  }

  String result = rawDate.substring(0, 10);
  result += "  ";
  result += rawDate.substring(11, 16);

  return result;
}

void drawTeamName(const String &name, int x, int y, uint16_t color) {
  String displayName = name;

  if (displayName.length() > 15) {
    displayName = displayName.substring(0, 14);
  }

  drawText(displayName, x, y, 2, color, PANEL);
}

void drawMatchCard(int y, const String &title, const String &home,
                   const String &away, const String &date,
                   const String &score, bool finished) {
  const int cardX = 18;
  const int cardW = 444;
  const int cardH = 102;

  fillRect(cardX, y, cardW, cardH, PANEL);
  drawRect(cardX, y, cardW, cardH, BORDER);
  fillRect(cardX, y, 5, cardH, LIME);

  drawText(title, 34, y + 10, 1, LIME, PANEL);

  if (home.length() == 0 || away.length() == 0) {
    drawText("Waiting for match data...", 35, y + 48, 1, MUTED, PANEL);
    return;
  }

  drawTeamName(home, 35, y + 42, WHITE);
  drawTeamName(away, 340, y + 42, WHITE);

  if (finished && score.length() > 0) {
    drawText(score, 205, y + 39, 3, WHITE, PANEL);
  } else {
    drawText("VS", 218, y + 44, 2, GOLD, PANEL);
  }

  drawText(formatDate(date), 35, y + 77, 1, MUTED, PANEL);

  if (finished) {
    drawText("FT", 420, y + 77, 1, LIME, PANEL);
  }
}

void drawDashboard() {
  if (selectedTeam < 0) return;

  mylcd.Fill_Screen(BG);
  drawBackButton();

  drawText(teams[selectedTeam].name, 145, 24, 2, WHITE, BG);
  drawText("MATCH CENTER", 170, 51, 1, MUTED, BG);

  drawLine(18, 70, 462, 70, BORDER);

  drawMatchCard(82, "LAST MATCH", lastHome, lastAway,
                lastDate, lastScore, true);

  drawMatchCard(194, "NEXT MATCH", nextHome, nextAway,
                nextDate, "", false);

  fillRect(0, 304, 480, 16, BG);
  drawText(statusMessage, 20, 306, 1, MUTED, BG);
}

void requestTeamData(int index) {
  if (index < 0 || index >= TEAM_COUNT) return;

  selectedTeam = index;

  lastHome = "";
  lastAway = "";
  lastDate = "";
  lastScore = "";

  nextHome = "";
  nextAway = "";
  nextDate = "";

  hasLastMatch = false;
  hasNextMatch = false;

  statusMessage = "Connecting to API...";
  drawDashboard();

  Serial.print("TEAM|");
  Serial.println(teams[index].apiId);
}

String fieldAt(const String &line, int index) {
  int start = 0;

  for (int i = 0; i < index; i++) {
    int separator = line.indexOf('|', start);

    if (separator < 0) return "";

    start = separator + 1;
  }

  int end = line.indexOf('|', start);

  if (end < 0) {
    end = line.length();
  }

  return line.substring(start, end);
}

void processIncomingLine(String line) {
  line.trim();

  if (line.startsWith("LAST|")) {
    lastHome = fieldAt(line, 1);
    lastAway = fieldAt(line, 2);
    lastDate = fieldAt(line, 3);

    String homeScore = fieldAt(line, 4);
    String awayScore = fieldAt(line, 5);

    if (homeScore == "-" || awayScore == "-") {
      lastScore = "";
    } else {
      lastScore = homeScore + " - " + awayScore;
    }

    hasLastMatch = true;
    statusMessage = "Last match received";

    drawDashboard();
    return;
  }

  if (line.startsWith("NEXT|")) {
    nextHome = fieldAt(line, 1);
    nextAway = fieldAt(line, 2);
    nextDate = fieldAt(line, 3);

    hasNextMatch = true;
    statusMessage = "Next match received";

    drawDashboard();
    return;
  }

  if (line.startsWith("ERROR|")) {
    statusMessage = line.substring(6);
    drawDashboard();
    return;
  }

  if (line == "END") {
    if (hasLastMatch || hasNextMatch) {
      statusMessage = "Data loaded successfully";
    } else {
      statusMessage = "No match data available";
    }

    drawDashboard();
  }
}

void readESP8266() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      serialBuffer[serialBufferIndex] = '\0';

      if (serialBufferIndex > 0) {
        processIncomingLine(String(serialBuffer));
      }

      serialBufferIndex = 0;
    } else {
      if (serialBufferIndex < sizeof(serialBuffer) - 1) {
        serialBuffer[serialBufferIndex++] = c;
      } else {
        serialBufferIndex = 0;
      }
    }
  }
}

int getTouchedTeamIndex(int x, int y) {
  int bestIndex = -1;
  long bestDistance = 2147483647L;

  for (int i = 0; i < TEAM_COUNT; i++) {
    long dx = x - teamTouchPoints[i].x;
    long dy = y - teamTouchPoints[i].y;
    long distance = dx * dx + dy * dy;

    if (distance < bestDistance) {
      bestDistance = distance;
      bestIndex = i;
    }
  }

  long maxDistanceSquared =
    (long)TOUCH_MAX_DISTANCE * TOUCH_MAX_DISTANCE;

  if (bestDistance > maxDistanceSquared) {
    return -1;
  }

  return bestIndex;
}

void handleTouch() {
  TSPoint p = ts.getPoint();

  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  digitalWrite(XM, HIGH);
  digitalWrite(YP, HIGH);

  if (p.z < 10 || p.z > 1000) {
    touchWasDown = false;
    return;
  }

  if (touchWasDown) return;
  touchWasDown = true;

  int x = map(p.x, RAW_X_LEFT, RAW_X_RIGHT, 0, 479);
  int y = map(p.y, RAW_Y_TOP, RAW_Y_BOTTOM, 0, 319);

  x = constrain(x, 0, 479);
  y = constrain(y, 0, 319);

  if (selectedTeam >= 0) {
    if (abs(x - BACK_TOUCH_X) <= BACK_TOUCH_RADIUS_X &&
        abs(y - BACK_TOUCH_Y) <= BACK_TOUCH_RADIUS_Y) {
      drawSelectionScreen();
    }

    return;
  }

  int teamIndex = getTouchedTeamIndex(x, y);

  if (teamIndex >= 0) {
    requestTeamData(teamIndex);
  }
}

void setup() {
  Serial.begin(9600);

  mylcd.Init_LCD();
  mylcd.Invert_Display(true);
  mylcd.Set_Rotation(1);

  drawSelectionScreen();
}

void loop() {
  readESP8266();
  handleTouch();
}
