#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>
#include <WiFi.h>
#include <time.h>

// ================= WIFI =================
const char* WIFI_SSID = "DucDuy";
const char* WIFI_PASS = "@19112004@";
const long GMT_OFFSET_SEC = 7 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// ================= BAO THUC =================
int baoThucGio = 23;
int baoThucPhut = 10;
bool baoThucBat = true;

const uint32_t BAO_THUC_THOI_GIAN_MS = 20000;
const uint32_t BAO_THUC_CHU_KY_NHAY_MS = 500;

// ================= PIN =================
#define LCD_SDA             21
#define LCD_SCL             19

// UART 2 chieu MAIN <-> PHU
#define UART_RX_FROM_MAIN   22
#define UART_TX_TO_MAIN     23

#define TM1637_CLK          4
#define TM1637_DIO          15

// ===== CAM BIEN =====
#define TOUCH_PIN           5
#define LIGHT_PIN           27

// ===== DOI 2 DONG NAY NEU CAM BIEN BI NGUOC =====
#define TOUCH_ACTIVE_LEVEL  HIGH
#define DARK_ACTIVE_LEVEL   LOW

// ================= THAM SO =================
const uint32_t HOLD_MSG_MS = 1800;
const uint32_t CHONG_DOI_CAM_BIEN_MS = 120;
const uint32_t CHU_KY_DOC_CAM_BIEN_MS = 80;

// ================= OBJECT =================
LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial mainSerial(2);
TM1637Display display(TM1637_CLK, TM1637_DIO);

// ================= MUTEX =================
SemaphoreHandle_t mutexLCD;
SemaphoreHandle_t mutexState;
SemaphoreHandle_t mutexTM;
SemaphoreHandle_t mutexUART;

// ================= STATE =================
struct LCDState {
  String dong1;
  String dong2;
  bool dangGiuThongBao;
  unsigned long mocBatDauGiu;
  uint32_t thoiGianGiuMs;

  bool baoThucDangKeu;
  unsigned long mocBatDauBaoThuc;
  unsigned long mocNhapNhayCuoi;
  bool tm1637DangBat;

  int ngayDaBao;
  int thangDaBao;
  int namDaBao;

  bool chamCu;
  bool troiToCu;
  unsigned long mocDocCamBienCuoi;
};

LCDState st;

// ================= HELPER =================
void khoaState() {
  xSemaphoreTake(mutexState, portMAX_DELAY);
}

void moKhoaState() {
  xSemaphoreGive(mutexState);
}

void inDong16(String s) {
  while (s.length() < 16) s += " ";
  lcd.print(s.substring(0, 16));
}

void hienLCD(String dong1, String dong2) {
  xSemaphoreTake(mutexLCD, portMAX_DELAY);

  lcd.setCursor(0, 0);
  inDong16(dong1);

  lcd.setCursor(0, 1);
  inDong16(dong2);

  xSemaphoreGive(mutexLCD);
}

void hienMacDinh() {
  khoaState();
  st.dong1 = "NHA CUA DUY";
  st.dong2 = "WELCOME";
  st.dangGiuThongBao = false;
  moKhoaState();

  hienLCD("NHA CUA DUY", "WELCOME");
}

void hienThongBaoTam(String dong1, String dong2, uint32_t holdMs) {
  khoaState();
  st.dong1 = dong1;
  st.dong2 = dong2;
  st.dangGiuThongBao = true;
  st.mocBatDauGiu = millis();
  st.thoiGianGiuMs = holdMs;
  moKhoaState();

  hienLCD(dong1, dong2);
}

void guiVeMain(const String &msg) {
  xSemaphoreTake(mutexUART, portMAX_DELAY);
  mainSerial.println(msg);
  xSemaphoreGive(mutexUART);

  Serial.print("GUI MAIN: ");
  Serial.println(msg);
}

bool dangCham() {
  return digitalRead(TOUCH_PIN) == TOUCH_ACTIVE_LEVEL;
}

bool dangToi() {
  return digitalRead(LIGHT_PIN) == DARK_ACTIVE_LEVEL;
}

void xuLyUART(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("NHAN MAIN: ");
  Serial.println(line);

  if (line.startsWith("SET_ALARM|")) {
    int p1 = line.indexOf('|');
    int p2 = line.indexOf('|', p1 + 1);
    int p3 = line.indexOf('|', p2 + 1);

    if (p1 != -1 && p2 != -1 && p3 != -1) {
      int gioMoi = constrain(line.substring(p1 + 1, p2).toInt(), 0, 23);
      int phutMoi = constrain(line.substring(p2 + 1, p3).toInt(), 0, 59);
      bool batMoi = (line.substring(p3 + 1).toInt() == 1);

      bool coThayDoiBaoThuc = (gioMoi != baoThucGio) || (phutMoi != baoThucPhut) || (batMoi != baoThucBat);

      baoThucGio = gioMoi;
      baoThucPhut = phutMoi;
      baoThucBat = batMoi;

      // QUAN TRONG: Khi doi gio bao thuc tu app, cho phep bao lai trong cung ngay.
      // Neu khong reset, sau khi da bao 1 lan trong ngay thi cac gio moi sau do se bi chan.
      if (coThayDoiBaoThuc) {
        khoaState();
        st.ngayDaBao = -1;
        st.thangDaBao = -1;
        st.namDaBao = -1;
        st.baoThucDangKeu = false;
        moKhoaState();
      }

      String dong2 = String(baoThucGio);
      dong2 += ":";
      if (baoThucPhut < 10) dong2 += "0";
      dong2 += String(baoThucPhut);
      dong2 += baoThucBat ? " ON" : " OFF";

      hienThongBaoTam("CAP NHAT BAO THUC", dong2, 2000);
    }
    return;
  }

  int tach = line.indexOf('|');
  if (tach == -1) {
    hienThongBaoTam(line, "", HOLD_MSG_MS);
    return;
  }

  String dong1 = line.substring(0, tach);
  String dong2 = line.substring(tach + 1);

  hienThongBaoTam(dong1, dong2, HOLD_MSG_MS);
}

// ================= WIFI + NTP =================
void ketNoiWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Dang ket noi WiFi");
  int dem = 0;

  while (WiFi.status() != WL_CONNECTED && dem < 40) {
    delay(500);
    Serial.print(".");
    dem++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAIL");
  }
}

void khoiTaoThoiGian() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
             "pool.ntp.org", "time.nist.gov", "time.google.com");

  struct tm timeinfo;
  int dem = 0;

  while (!getLocalTime(&timeinfo) && dem < 20) {
    Serial.println("Dang doi dong bo gio NTP...");
    delay(500);
    dem++;
  }

  if (getLocalTime(&timeinfo)) {
    Serial.println("Dong bo gio thanh cong");
  } else {
    Serial.println("Khong lay duoc gio NTP");
  }
}

// ================= TM1637 =================
void hienGioLenTM1637(int gio, int phut, bool hienDauHaiCham) {
  int value = gio * 100 + phut;

  xSemaphoreTake(mutexTM, portMAX_DELAY);
  display.showNumberDecEx(
    value,
    hienDauHaiCham ? 0b01000000 : 0b00000000,
    true
  );
  xSemaphoreGive(mutexTM);
}

void tatTM1637() {
  xSemaphoreTake(mutexTM, portMAX_DELAY);
  display.clear();
  xSemaphoreGive(mutexTM);
}

// ================= BAO THUC =================
bool daBaoHomNay(int nam, int thang, int ngay) {
  bool kq;
  khoaState();
  kq = (st.namDaBao == nam && st.thangDaBao == thang && st.ngayDaBao == ngay);
  moKhoaState();
  return kq;
}

void danhDauDaBaoHomNay(int nam, int thang, int ngay) {
  khoaState();
  st.namDaBao = nam;
  st.thangDaBao = thang;
  st.ngayDaBao = ngay;
  moKhoaState();
}

void batBaoThuc() {
  khoaState();
  st.baoThucDangKeu = true;
  st.mocBatDauBaoThuc = millis();
  st.mocNhapNhayCuoi = 0;
  st.tm1637DangBat = true;
  moKhoaState();

  hienThongBaoTam("BAO THUC !!!", "TM1637 NHAP NHAY", BAO_THUC_THOI_GIAN_MS);
  guiVeMain("ALARM_ON");
  Serial.println("BAT BAO THUC");
}

void tatBaoThuc() {
  khoaState();
  st.baoThucDangKeu = false;
  st.tm1637DangBat = true;
  moKhoaState();

  Serial.println("TAT BAO THUC");
}

// ================= TASK NHAN UART =================
void taskNhanUART(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    while (mainSerial.available()) {
      String line = mainSerial.readStringUntil('\n');
      xuLyUART(line);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ================= TASK QUAN LY LCD =================
void taskQuanLyLCD(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    bool dangGiu;
    unsigned long moc;
    uint32_t hold;
    bool baoThucDangKeu;

    khoaState();
    dangGiu = st.dangGiuThongBao;
    moc = st.mocBatDauGiu;
    hold = st.thoiGianGiuMs;
    baoThucDangKeu = st.baoThucDangKeu;
    moKhoaState();

    if (dangGiu && millis() - moc >= hold) {
      if (!baoThucDangKeu) {
        hienMacDinh();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ================= TASK TM1637 + BAO THUC =================
void taskDongHoTM1637(void *pvParameters) {
  (void) pvParameters;

  bool cham = false;

  for (;;) {
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
      int gio = timeinfo.tm_hour;
      int phut = timeinfo.tm_min;
      int ngay = timeinfo.tm_mday;
      int thang = timeinfo.tm_mon + 1;
      int nam = timeinfo.tm_year + 1900;

      bool baoThucDangKeu;
      unsigned long mocBaoThuc;
      unsigned long mocNhayCuoi;
      bool tmDangBat;

      khoaState();
      baoThucDangKeu = st.baoThucDangKeu;
      mocBaoThuc = st.mocBatDauBaoThuc;
      mocNhayCuoi = st.mocNhapNhayCuoi;
      tmDangBat = st.tm1637DangBat;
      moKhoaState();

      if (baoThucBat &&
          !baoThucDangKeu &&
          gio == baoThucGio &&
          phut == baoThucPhut &&
          !daBaoHomNay(nam, thang, ngay)) {
        danhDauDaBaoHomNay(nam, thang, ngay);
        batBaoThuc();

        khoaState();
        baoThucDangKeu = st.baoThucDangKeu;
        mocBaoThuc = st.mocBatDauBaoThuc;
        mocNhayCuoi = st.mocNhapNhayCuoi;
        tmDangBat = st.tm1637DangBat;
        moKhoaState();
      }

      if (baoThucDangKeu) {
        unsigned long nowMs = millis();

        if (nowMs - mocBaoThuc >= BAO_THUC_THOI_GIAN_MS) {
          tatBaoThuc();
          hienMacDinh();
          hienGioLenTM1637(gio, phut, true);
        } else {
          if (nowMs - mocNhayCuoi >= BAO_THUC_CHU_KY_NHAY_MS) {
            tmDangBat = !tmDangBat;

            khoaState();
            st.tm1637DangBat = tmDangBat;
            st.mocNhapNhayCuoi = nowMs;
            moKhoaState();
          }

          if (tmDangBat) hienGioLenTM1637(gio, phut, true);
          else tatTM1637();
        }
      } else {
        cham = !cham;
        hienGioLenTM1637(gio, phut, cham);
      }
    } else {
      xSemaphoreTake(mutexTM, portMAX_DELAY);
      uint8_t dash[] = {0x40, 0x40, 0x40, 0x40};
      display.setSegments(dash);
      xSemaphoreGive(mutexTM);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ================= TASK CAM BIEN =================
void taskCamBien(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    unsigned long now = millis();
    bool choDoc = false;
    bool chamCu;
    bool troiToCu;

    khoaState();
    if (now - st.mocDocCamBienCuoi >= CHU_KY_DOC_CAM_BIEN_MS) {
      st.mocDocCamBienCuoi = now;
      choDoc = true;
    }
    chamCu = st.chamCu;
    troiToCu = st.troiToCu;
    moKhoaState();

    if (!choDoc) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    bool chamMoi = dangCham();
    bool troiToMoi = dangToi();

    if (chamMoi != chamCu) {
      vTaskDelay(pdMS_TO_TICKS(CHONG_DOI_CAM_BIEN_MS));
      bool chamXacNhan = dangCham();

      if (chamXacNhan == chamMoi) {
        khoaState();
        st.chamCu = chamXacNhan;
        moKhoaState();

        Serial.print("TOUCH raw=");
        Serial.println(chamXacNhan ? "ACTIVE" : "INACTIVE");

        if (chamXacNhan) {
          guiVeMain("TOUCH_ON");
          hienThongBaoTam("CAM BIEN CHAM", "DANG CHAM", 1200);
        } else {
          guiVeMain("TOUCH_OFF");
          hienThongBaoTam("CAM BIEN CHAM", "DA THA TAY", 1200);
        }
      }
    }

    if (troiToMoi != troiToCu) {
      vTaskDelay(pdMS_TO_TICKS(CHONG_DOI_CAM_BIEN_MS));
      bool troiToXacNhan = dangToi();

      if (troiToXacNhan == troiToMoi) {
        khoaState();
        st.troiToCu = troiToXacNhan;
        moKhoaState();

        Serial.print("LIGHT raw=");
        Serial.println(troiToXacNhan ? "DARK" : "BRIGHT");

        if (troiToXacNhan) {
          guiVeMain("LIGHT_BRIGHT");
          hienThongBaoTam("TROI SANG", "GUI VE MAIN", 1200);
        } else {
          guiVeMain("LIGHT_DARK");
          hienThongBaoTam("TROI TOI", "GUI VE MAIN", 1200);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ================= TASK KIEM TRA WIFI/NTP =================
void taskDongBoLaiGio(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      ketNoiWiFi();
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      khoiTaoThoiGian();
    }

    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  mutexLCD = xSemaphoreCreateMutex();
  mutexState = xSemaphoreCreateMutex();
  mutexTM = xSemaphoreCreateMutex();
  mutexUART = xSemaphoreCreateMutex();

  pinMode(TOUCH_PIN, INPUT);
  pinMode(LIGHT_PIN, INPUT);

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  mainSerial.begin(9600, SERIAL_8N1, UART_RX_FROM_MAIN, UART_TX_TO_MAIN);

  display.setBrightness(7, true);
  display.clear();

  khoaState();
  st.dong1 = "NHA CUA DUY";
  st.dong2 = "WELCOME";
  st.dangGiuThongBao = false;
  st.mocBatDauGiu = 0;
  st.thoiGianGiuMs = 2500;
  st.baoThucDangKeu = false;
  st.mocBatDauBaoThuc = 0;
  st.mocNhapNhayCuoi = 0;
  st.tm1637DangBat = true;
  st.ngayDaBao = -1;
  st.thangDaBao = -1;
  st.namDaBao = -1;
  st.chamCu = dangCham();
  st.troiToCu = dangToi();
  st.mocDocCamBienCuoi = 0;
  moKhoaState();

  hienLCD("NHA CUA DUY", "KHOI DONG...");

  ketNoiWiFi();
  khoiTaoThoiGian();

  hienMacDinh();

  if (dangToi()) guiVeMain("LIGHT_DARK");
  else guiVeMain("LIGHT_BRIGHT");

  Serial.println("ESP32 PHU SAN SANG");

  xTaskCreatePinnedToCore(taskNhanUART,     "taskNhanUART",     4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskQuanLyLCD,    "taskQuanLyLCD",    4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskDongHoTM1637, "taskDongHoTM1637", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskCamBien,      "taskCamBien",      4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskDongBoLaiGio, "taskDongBoLaiGio", 4096, NULL, 1, NULL, 1);
}

// ================= LOOP =================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}