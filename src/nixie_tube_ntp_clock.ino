/*
NTP Clock. (c)2022 @logic_star All rights reserved.
https://www.marutsu.co.jp/pc/static/large_order/ntp_clock_20220521
Modified by @riraosan.github.io for ATOM Lite.
@riraosan_0901 have not confirmed that it works with anything other M5Stack.
*/

// #define IMAGE_FROM_SD //SDカードからイメージを読み込む場合は有効化要
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Button2.h>
#include <M5GFX.h>
#include <LGFX_8BIT_CVBS.h>
#include <image.h>

static LGFX_8BIT_CVBS display;

#define BUTTON_GPIO_NUM 16
static Button2 button;

#define TFCARD_CS_PIN 4
#define LGFX          LGFX_8BIT_CVBS
#define LGFX_ONLY
#define USE_DISPLAY
#define SDU_APP_NAME "Nixie Tube NTP Clock"
#define SDU_APP_PATH "/00_nixie_tube_ntp_clock.bin"

#include <M5StackUpdater.h>

#define NOT_USEATOM

static M5Canvas     sprites[2];
static int_fast16_t sprite_height;
static std::int32_t display_width;
static std::int32_t display_height;

// Wifi 関連定義
const char* ntpServer          = "ntp.nict.jp";
const long  gmtOffset_sec      = 9 * 3600;
const int   daylightOffset_sec = 0;

// 時間関連
struct tm timeinfo;
uint8_t   secLastReport = 0;

bool autoNtp(void) {
  display.fillScreen(TFT_BLACK);  // 画面初期化
  display.setTextSize(1);
  display.setCursor(5, 10);
  display.println("Connecting...");

  WiFi.begin("your_ssid", "your_password");

  display.println("");
  display.print(" ");
  display.setTextSize(4);
  display.setTextColor(TFT_GREEN);
  display.println("CONNECTED");  // WiFi接続成功表示
  delay(1000);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // NTPによる時刻取得

  if (!getLocalTime(&timeinfo)) {
    WiFi.disconnect(true);  // 時刻取得失敗でWiFiオフ
    WiFi.mode(WIFI_OFF);

    display.setTextColor(TFT_RED);
    display.setTextSize(2);
    display.printf(" Failed to obtain time");  // 時刻取得失敗表示

    return false;  // 時刻取得失敗でリターン
  }

  display.fillScreen(TFT_BLACK);  // 画面消去

  return true;  // 時刻取得成功でリターン
}

// 各イメージデータの配列名を定義
const unsigned char* image_data[17] = {image0, image1, image2, image3, image4,
                                       image5, image6, image7, image8, image9,
                                       sun, mon, tue, wed, thu, fri, sat};
// 各イメージデータのサイズを定義
const uint32_t image_size[17] = {sizeof image0, sizeof image1, sizeof image2, sizeof image3, sizeof image4,
                                 sizeof image5, sizeof image6, sizeof image7, sizeof image8, sizeof image9,
                                 sizeof sun, sizeof mon, sizeof tue, sizeof wed, sizeof thu, sizeof fri, sizeof sat};

// x, yの位置にnumberの数字または曜日を表示
void PutJpg(M5Canvas* sprite, uint16_t x, uint16_t y, uint16_t number) {
  sprite->drawJpg(image_data[number], image_size[number], x, y);
}

// x, yの位置からnumberの数値をdigit桁で表示。文字間隔はx_offset
void PutNum(M5Canvas* sprite, uint16_t x, uint16_t y, uint16_t x_offset, uint8_t digit, uint16_t number) {
  int temp = number;
  int loop;
  for (loop = digit; loop > 0; loop--) {
    PutJpg(sprite, x + x_offset * (digit - loop), y, temp / int(pow(10, (loop - 1))));
    temp = temp % int(pow(10, (loop - 1)));
  }
}

void setupSprite(void) {
  display_width  = display.width();
  display_height = display.height();

  uint32_t div = 2;
  for (;;) {
    sprite_height = (display_height + div - 1) / div;
    bool fail     = false;
    for (std::uint32_t i = 0; !fail && i < 2; ++i) {
      sprites[i].setColorDepth(display.getColorDepth());
      sprites[i].fillScreen(TFT_BLACK);  // M5Stackの画面初期化
      sprites[i].setFont(&fonts::Font4);
      sprites[i].setTextSize(1);
      sprites[i].setTextColor(TFT_WHITE, TFT_BLACK);
      sprites[i].setTextDatum(TL_DATUM);
      fail = !sprites[i].createSprite(320, sprite_height + 10);
    }
    if (!fail) break;
    for (std::uint32_t i = 0; i < 2; ++i) {
      sprites[i].deleteSprite();
    }
    ++div;
  }

  display.startWrite();
}

bool bA = false;
bool bB = false;
bool bC = false;

void handler(Button2& btn) {
  switch (btn.getType()) {
    case clickType::single_click:
      Serial.print("single ");
      bB = true;
      break;
    case clickType::double_click:
      Serial.print("double ");
      bC = true;
      break;
    case clickType::triple_click:
      Serial.print("triple ");
      break;
    case clickType::long_click:
      Serial.print("long ");
      bA = true;
      break;
    case clickType::empty:
      break;
    default:
      break;
  }

  Serial.print("click");
  Serial.print(" (");
  Serial.print(btn.getNumberOfClicks());
  Serial.println(")");
}

bool buttonAPressed(void) {
  bool temp = bA;
  bA        = false;

  return temp;
}

bool buttonBPressed(void) {
  bool temp = bB;
  bB        = false;

  return temp;
}

bool buttonCPressed(void) {
  bool temp = bC;
  bC        = false;

  return temp;
}

void ButtonUpdate() {
  button.loop();
}

void setupButton(void) {
  button.setClickHandler(handler);
  button.setDoubleClickHandler(handler);
  button.setTripleClickHandler(handler);
  button.setLongClickHandler(handler);
  button.begin(BUTTON_GPIO_NUM);

  SDUCfg.setSDUBtnA(&buttonAPressed);
  SDUCfg.setSDUBtnB(&buttonBPressed);
  SDUCfg.setSDUBtnC(&buttonCPressed);
  SDUCfg.setSDUBtnPoller(&ButtonUpdate);
}

void setup() {
  display.begin();

  setupButton();
  setSDUGfx(&display);
  checkSDUpdater(
      SD,            // filesystem (default=SD)
      MENU_BIN,      // path to binary (default=/menu.bin, empty string=rollback only)
      2000,          // wait delay, (default=0, will be forced to 2000 upon ESP.restart() )
      TFCARD_CS_PIN  // (usually default=4 but your mileage may vary)
  );

  setupSprite();

  // 時刻取得に失敗した場合は、動作停止
  if (!autoNtp()) {
    while (1) {
      ;
    }
  }
}

void loop() {
  getLocalTime(&timeinfo);

  ButtonUpdate();

  if (buttonAPressed()) {
    display.fillScreen(TFT_BLACK);

    updateFromFS(SD);
    ESP.restart();
  }

  // 毎日午前12時に時刻取得
  if ((timeinfo.tm_hour == 12) && (timeinfo.tm_min == 0) && (timeinfo.tm_sec == 0)) {
    ESP.restart();
  }

  if (secLastReport != timeinfo.tm_sec) {  // 秒が更新されたら、表示をupdate
    secLastReport = timeinfo.tm_sec;
    delay(10);

    // スプライト上
    sprites[0].setCursor(3, 16);
    sprites[0].printf("DATE");
    sprites[0].printf(" NTP Nixie Tube Clock");
    PutNum(&sprites[0], 3, 36, 52, 2, timeinfo.tm_mon + 1);  // 月の表示
    PutNum(&sprites[0], 110, 36, 52, 2, timeinfo.tm_mday);   // 日の表示
    PutJpg(&sprites[0], 216, 63, timeinfo.tm_wday + 10);     // 曜日の表示

    display.setPivot((display_width >> 1) - 7, (sprite_height >> 1) - 7);
    sprites[0].pushRotateZoomWithAA(&display, 0, 0.85, 0.85);

    // スプライト下
    sprites[1].setCursor(3, 16);
    sprites[1].printf("TIME");
    PutNum(&sprites[1], 3, 36, 52, 2, timeinfo.tm_hour);   // 時の表示
    PutNum(&sprites[1], 110, 36, 52, 2, timeinfo.tm_min);  // 分の表示
    PutNum(&sprites[1], 216, 36, 52, 2, timeinfo.tm_sec);  // 秒の表示

    display.setPivot((display_width >> 1) - 7, sprite_height + (sprite_height >> 1));
    sprites[1].pushRotateZoomWithAA(&display, 0, 0.85, 0.85);

    delay(100);  // 0.1秒ウェイト
  }
  display.display();
}
