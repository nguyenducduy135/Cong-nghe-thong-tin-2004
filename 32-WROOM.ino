#define BLYNK_TEMPLATE_ID "TMPL6IfWaF4Iu"
#define BLYNK_TEMPLATE_NAME "Smart Home"
#define BLYNK_AUTH_TOKEN "GPZErf4sCmC9RPkCgleB8YRHSaV7R9SX"

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include <Stepper.h>

// ================= WIFI =================
char ssid[] = "DucDuy";
char pass[] = "@19112004@";

BlynkTimer timer;

// ================= PIN =================
#define BUZZER_PIN          25
#define RELAY_PIN           26
#define SERVO_PIN           4
#define NEOPIXEL_PIN        2

#define DHT_PIN             17
#define DHT_TYPE            DHT11

#define MQ2_PIN             34
#define RAIN_PIN            32

#define STEPPER_IN1         12
#define STEPPER_IN2         14
#define STEPPER_IN3         16
#define STEPPER_IN4         27

#define RFID_SS             5
#define RFID_RST            13
#define RFID_SCK            18
#define RFID_MISO           19
#define RFID_MOSI           23

#define UART_TX_TO_LCD      22
#define UART_RX_FROM_SLAVE  21
#define UART_BAUD_SLAVE     9600

// ================= THAM SO =================
const int nguongGas = 500;

const int GOC_MO_CUA   = 100;
const int GOC_DONG_CUA = 0;

const uint32_t THOI_GIAN_MO_CUA_MS   = 3000;
const uint32_t CHU_KY_NHAY_GAS_MS    = 250;
const uint32_t CHU_KY_GUI_DHT_LCD_MS = 4000;

const int BUOC_SERVO     = 50;
const int DELAY_SERVO_MS = 20;

const uint32_t KHOA_THE_LAP_MS = 1500;

// ================= BAO THUC TU APP =================
int alarmHour = 23;
int alarmMinute = 0;
bool alarmEnable = false;

// ================= UID THE HOP LE =================
const byte theHopLe1[] = {0x2C, 0x2F, 0x16, 0x05};
const byte theHopLe2[] = {0x46, 0x04, 0xA0, 0x5B};

const byte doDaiTheHopLe1 = sizeof(theHopLe1);
const byte doDaiTheHopLe2 = sizeof(theHopLe2);

// ================= OBJECT =================
MFRC522 rfid(RFID_SS, RFID_RST);
Servo servoCua;
Adafruit_NeoPixel led(8, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
DHT dht(DHT_PIN, DHT_TYPE);
Stepper gianPhoi(2048, STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4);
HardwareSerial lcdSerial(2);
HardwareSerial slaveSerial(1);

// ================= MUTEX =================
SemaphoreHandle_t mutexState;
SemaphoreHandle_t mutexSPI;
SemaphoreHandle_t mutexNeo;
SemaphoreHandle_t mutexUART;

// ================= BIEN THEM =================
unsigned long mocGuiDHTLenLCD = 0;

// ================= STATE =================
struct SystemState {
  float nhietDo;
  float doAm;
  int giaTriGas;

  bool coMua;
  bool muaCu;
  bool canhBaoGas;
  bool canhBaoGasCu;

  bool relayDangBat;
  bool cuaDangMo;

  bool denDoDangBat;
  bool denTrangBat;
  bool denAppBat;

  bool buzzerBat;
  bool buzzerAppBat;

  bool dangMoCuaBangThe;
  unsigned long mocMoCua;

  int soLanSaiThe;

  bool giuDenTrangSauThe;
  bool cuaMoDoGas;
  bool cuaAppBat;

  bool servoDangChay;
  String uidCuoi;
  unsigned long mocKhoaRFID;

  String lcdDong1Cuoi;
  String lcdDong2Cuoi;
  unsigned long mocGuiLCDCuoi;

  bool troiToTuCamBienPhu;
  bool chamTuCamBienPhu;
};

SystemState st;

// ================= PROTOTYPE =================
void khoaState();
void moKhoaState();

void guiLCD(const String &dong1, const String &dong2, bool force = false);
void guiDHTLenLCD();

void datBuzzer(bool bat);
void capNhatDenUuTien();

void tatNeoPixelRaw();
void neoPixelTrangRaw();
void neoPixelDoRaw();

void tatNeoPixel();
void neoPixelTrang();
void neoPixelDo();

void batRelay();
void tatRelay();

void moCuaCham();
void dongCuaCham();

void keoGianPhoi();
void dayGianPhoi();

void beepBlock(int lan, int batMs, int tatMs);
void canhBaoTheSai3Lan();

String docUIDThe();
bool laTheHopLe();
void khoiPhucDenTheoTrangThai();

void guiDuLieuLenBlynk();
void guiCanhBaoGasEvent();
void guiCanhBaoRFIDEvent();
void guiBaoThucSangESPPhu();

void xuLyLenhTuESPPhu(String line);
void xuLyTouchTuESPPhu(bool dangCham);
void xuLyAnhSangTuESPPhu(bool troiTo);

void taskDocCamBien(void *pvParameters);
void taskLogic(void *pvParameters);
void taskRFID(void *pvParameters);
void taskGasAlarm(void *pvParameters);
void taskSerial(void *pvParameters);
void taskNhanUARTPhu(void *pvParameters);

// ================= BLYNK =================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V6, V10, V11, V13, V14, V15, V16, V17);
  guiBaoThucSangESPPhu();
}

BLYNK_WRITE(V6) {
  int value = param.asInt();
  bool gasDangBaoDong = false;
  bool cuaMoDoGas = false;

  khoaState();
  gasDangBaoDong = st.canhBaoGas;
  cuaMoDoGas = st.cuaMoDoGas;
  moKhoaState();

  if (value == 1) {
    if (!gasDangBaoDong) {
      moCuaCham();

      khoaState();
      st.cuaAppBat = true;
      st.dangMoCuaBangThe = false;
      st.giuDenTrangSauThe = false;
      moKhoaState();

      guiLCD("MO CUA TU APP", "BLYNK", true);
    } else {
      guiLCD("GAS BAO DONG", "KHONG MO CUA", true);
      Blynk.virtualWrite(V6, 0);
    }
  } else {
    if (!cuaMoDoGas) {
      dongCuaCham();

      khoaState();
      st.cuaAppBat = false;
      st.dangMoCuaBangThe = false;
      moKhoaState();

      guiLCD("DONG CUA TU APP", "BLYNK", true);
    } else {
      Blynk.virtualWrite(V6, 1);
    }
  }
}

BLYNK_WRITE(V10) {
  int value = param.asInt();

  khoaState();
  st.denAppBat = (value == 1);
  st.giuDenTrangSauThe = false;
  moKhoaState();

  capNhatDenUuTien();
}

BLYNK_WRITE(V11) {
  int value = param.asInt();

  khoaState();
  st.buzzerAppBat = (value == 1);
  moKhoaState();
}

BLYNK_WRITE(V13) {
  int value = param.asInt();

  if (value == 1) {
    dayGianPhoi();
    guiLCD("APP", "DAY GIAN PHOI", true);
    Blynk.virtualWrite(V13, 0);
  }
}

BLYNK_WRITE(V14) {
  int value = param.asInt();

  if (value == 1) {
    keoGianPhoi();
    guiLCD("APP", "KEO GIAN PHOI", true);
    Blynk.virtualWrite(V14, 0);
  }
}

BLYNK_WRITE(V15) {
  alarmHour = constrain(param.asInt(), 0, 23);
  guiBaoThucSangESPPhu();
}

BLYNK_WRITE(V16) {
  alarmMinute = constrain(param.asInt(), 0, 59);
  guiBaoThucSangESPPhu();
}

BLYNK_WRITE(V17) {
  alarmEnable = (param.asInt() == 1);
  guiBaoThucSangESPPhu();
}

// ================= HELPER =================
void khoaState() {
  xSemaphoreTake(mutexState, portMAX_DELAY);
}

void moKhoaState() {
  xSemaphoreGive(mutexState);
}

void guiLCD(const String &dong1, const String &dong2, bool force) {
  unsigned long now = millis();
  bool boQua = false;

  khoaState();
  if (!force &&
      dong1 == st.lcdDong1Cuoi &&
      dong2 == st.lcdDong2Cuoi &&
      (now - st.mocGuiLCDCuoi < 400)) {
    boQua = true;
  } else {
    st.lcdDong1Cuoi = dong1;
    st.lcdDong2Cuoi = dong2;
    st.mocGuiLCDCuoi = now;
  }
  moKhoaState();

  if (boQua) return;

  xSemaphoreTake(mutexUART, portMAX_DELAY);
  lcdSerial.print(dong1);
  lcdSerial.print("|");
  lcdSerial.println(dong2);
  xSemaphoreGive(mutexUART);
}

void guiDHTLenLCD() {
  unsigned long now = millis();
  if (now - mocGuiDHTLenLCD < CHU_KY_GUI_DHT_LCD_MS) return;
  mocGuiDHTLenLCD = now;

  float nhiet, am;
  bool gas;
  bool dangMoBangThe;
  bool cuaMoDoGas;
  unsigned long mocMo;
  bool cuaAppBat;
  bool chamPhu;

  khoaState();
  nhiet = st.nhietDo;
  am = st.doAm;
  gas = st.canhBaoGas;
  dangMoBangThe = st.dangMoCuaBangThe;
  cuaMoDoGas = st.cuaMoDoGas;
  mocMo = st.mocMoCua;
  cuaAppBat = st.cuaAppBat;
  chamPhu = st.chamTuCamBienPhu;
  moKhoaState();

  if (gas) return;
  if (cuaMoDoGas) return;
  if (cuaAppBat) return;
  if (chamPhu) return;
  if (dangMoBangThe && (now - mocMo < THOI_GIAN_MO_CUA_MS + 1200)) return;

  guiLCD("NHIET:" + String(nhiet, 1) + "C",
         "DO AM:" + String(am, 1) + "%", true);
}

void datBuzzer(bool bat) {
  digitalWrite(BUZZER_PIN, bat ? HIGH : LOW);

  khoaState();
  st.buzzerBat = bat;
  moKhoaState();
}

void tatNeoPixelRaw() {
  for (int i = 0; i < 8; i++) led.setPixelColor(i, led.Color(0, 0, 0));
  led.show();
}

void neoPixelTrangRaw() {
  for (int i = 0; i < 8; i++) led.setPixelColor(i, led.Color(120, 120, 120));
  led.show();
}

void neoPixelDoRaw() {
  for (int i = 0; i < 8; i++) led.setPixelColor(i, led.Color(255, 0, 0));
  led.show();
}

void tatNeoPixel() {
  xSemaphoreTake(mutexNeo, portMAX_DELAY);
  tatNeoPixelRaw();
  xSemaphoreGive(mutexNeo);

  khoaState();
  st.denDoDangBat = false;
  st.denTrangBat = false;
  moKhoaState();
}

void neoPixelTrang() {
  xSemaphoreTake(mutexNeo, portMAX_DELAY);
  neoPixelTrangRaw();
  xSemaphoreGive(mutexNeo);

  khoaState();
  st.denTrangBat = true;
  st.denDoDangBat = false;
  moKhoaState();
}

void neoPixelDo() {
  xSemaphoreTake(mutexNeo, portMAX_DELAY);
  neoPixelDoRaw();
  xSemaphoreGive(mutexNeo);

  khoaState();
  st.denDoDangBat = true;
  st.denTrangBat = false;
  moKhoaState();
}

void capNhatDenUuTien() {
  bool gas, denApp, giuDen, troiTo;

  khoaState();
  gas = st.canhBaoGas;
  denApp = st.denAppBat;
  giuDen = st.giuDenTrangSauThe;
  troiTo = st.troiToTuCamBienPhu;
  moKhoaState();

  if (gas) return;

  if (denApp || giuDen || troiTo) neoPixelTrang();
  else tatNeoPixel();
}

void batRelay() {
  digitalWrite(RELAY_PIN, LOW);

  khoaState();
  st.relayDangBat = true;
  moKhoaState();
}

void tatRelay() {
  digitalWrite(RELAY_PIN, HIGH);

  khoaState();
  st.relayDangBat = false;
  moKhoaState();
}

void moCuaCham() {
  khoaState();
  if (st.servoDangChay || st.cuaDangMo) {
    moKhoaState();
    return;
  }
  st.servoDangChay = true;
  moKhoaState();

  for (int goc = GOC_DONG_CUA; goc <= GOC_MO_CUA; goc += BUOC_SERVO) {
    servoCua.write(goc);
    vTaskDelay(pdMS_TO_TICKS(DELAY_SERVO_MS));
  }
  servoCua.write(GOC_MO_CUA);

  khoaState();
  st.cuaDangMo = true;
  st.servoDangChay = false;
  moKhoaState();

  batRelay();
}

void dongCuaCham() {
  khoaState();
  if (st.servoDangChay || !st.cuaDangMo) {
    moKhoaState();
    return;
  }
  st.servoDangChay = true;
  moKhoaState();

  for (int goc = GOC_MO_CUA; goc >= GOC_DONG_CUA; goc -= BUOC_SERVO) {
    servoCua.write(goc);
    vTaskDelay(pdMS_TO_TICKS(DELAY_SERVO_MS));
  }
  servoCua.write(GOC_DONG_CUA);

  khoaState();
  st.cuaDangMo = false;
  st.servoDangChay = false;
  moKhoaState();

  tatRelay();
}

void keoGianPhoi() {
  gianPhoi.setSpeed(10);
  gianPhoi.step(1024);
}

void dayGianPhoi() {
  gianPhoi.setSpeed(10);
  gianPhoi.step(-1024);
}

void beepBlock(int lan, int batMs, int tatMs) {
  for (int i = 0; i < lan; i++) {
    datBuzzer(true);
    vTaskDelay(pdMS_TO_TICKS(batMs));
    datBuzzer(false);
    vTaskDelay(pdMS_TO_TICKS(tatMs));
  }
}

void canhBaoTheSai3Lan() {
  guiLCD("SAI THE 3 LAN", "CANH BAO !!!", true);

  for (int i = 0; i < 10; i++) {
    datBuzzer(true);
    neoPixelDo();
    vTaskDelay(pdMS_TO_TICKS(200));
    datBuzzer(false);
    tatNeoPixel();
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  capNhatDenUuTien();
}

String docUIDThe() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toLowerCase();
  return uid;
}

bool laTheHopLe() {
  bool trungThe1 = true;
  bool trungThe2 = true;

  if (rfid.uid.size == doDaiTheHopLe1) {
    for (byte i = 0; i < doDaiTheHopLe1; i++) {
      if (rfid.uid.uidByte[i] != theHopLe1[i]) {
        trungThe1 = false;
        break;
      }
    }
  } else trungThe1 = false;

  if (rfid.uid.size == doDaiTheHopLe2) {
    for (byte i = 0; i < doDaiTheHopLe2; i++) {
      if (rfid.uid.uidByte[i] != theHopLe2[i]) {
        trungThe2 = false;
        break;
      }
    }
  } else trungThe2 = false;

  return (trungThe1 || trungThe2);
}

void khoiPhucDenTheoTrangThai() {
  capNhatDenUuTien();
}

void guiCanhBaoGasEvent() {
  int gasValue;
  float nhiet, am;

  khoaState();
  gasValue = st.giaTriGas;
  nhiet = st.nhietDo;
  am = st.doAm;
  moKhoaState();

  String thongBao = "CANH BAO GAS! Gas=" + String(gasValue) +
                    ", Nhiet do=" + String(nhiet, 1) +
                    "C, Do am=" + String(am, 1) + "%";
  Blynk.logEvent("gas_alert", thongBao);
}

void guiCanhBaoRFIDEvent() {
  Blynk.logEvent("rfid_alert", "Canh bao: Quet the sai 3 lan lien tiep!");
}

void guiBaoThucSangESPPhu() {
  String lenh = "SET_ALARM|" + String(alarmHour) + "|" + String(alarmMinute) + "|" + String(alarmEnable ? 1 : 0);

  xSemaphoreTake(mutexUART, portMAX_DELAY);
  // Gui chung duong UART TX22 sang ESP phu (ESP phu RX22)
  // ESP phu se nhan lenh SET_ALARM va cap nhat gio bao thuc
  lcdSerial.println(lenh);
  xSemaphoreGive(mutexUART);

  String dong2 = String(alarmHour);
  dong2 += ":";
  if (alarmMinute < 10) dong2 += "0";
  dong2 += String(alarmMinute);
  dong2 += alarmEnable ? " ON" : " OFF";

  guiLCD("CAP NHAT BAO THUC", dong2, true);
}

void guiDuLieuLenBlynk() {
  float nhiet, am;
  bool denApp, buzzerApp, cuaDangMo;
  int gasValue;

  khoaState();
  nhiet = st.nhietDo;
  am = st.doAm;
  denApp = st.denAppBat;
  buzzerApp = st.buzzerAppBat;
  cuaDangMo = st.cuaDangMo;
  gasValue = st.giaTriGas;
  moKhoaState();

  Blynk.virtualWrite(V0, nhiet);
  Blynk.virtualWrite(V1, am);
  Blynk.virtualWrite(V2, gasValue);
  Blynk.virtualWrite(V6, cuaDangMo ? 1 : 0);
  Blynk.virtualWrite(V10, denApp ? 1 : 0);
  Blynk.virtualWrite(V11, buzzerApp ? 1 : 0);
  // Khong ghi nguoc V15/V16/V17 lien tuc, de app giu gia tri nguoi dung dat
}

void xuLyTouchTuESPPhu(bool dangCham) {
  khoaState();
  st.chamTuCamBienPhu = dangCham;
  moKhoaState();

  if (dangCham) guiLCD("CANH BAO", "CO NGUOI CHAM", true);
  else guiLCD("CAM BIEN CHAM", "DA TAT", true);
}

void xuLyAnhSangTuESPPhu(bool troiTo) {
  khoaState();
  st.troiToTuCamBienPhu = troiTo;
  moKhoaState();

  if (troiTo) guiLCD("TROI TO", "BAT DEN", true);
  else guiLCD("TROI SANG", "TAT DEN", true);

  capNhatDenUuTien();
}

void xuLyLenhTuESPPhu(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("Nhan ESP PHU: ");
  Serial.println(line);

  if (line == "TOUCH_ON") {
    xuLyTouchTuESPPhu(true);
  } else if (line == "TOUCH_OFF") {
    xuLyTouchTuESPPhu(false);
  } else if (line == "LIGHT_DARK") {
    xuLyAnhSangTuESPPhu(true);
  } else if (line == "LIGHT_BRIGHT") {
    xuLyAnhSangTuESPPhu(false);
  } else if (line == "ALARM_ON") {
    guiLCD("BAO THUC", "COI KEU 20 LAN", true);
    beepBlock(20, 150, 150);
  }
}

void taskDocCamBien(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int gas = analogRead(MQ2_PIN);
    bool mua = (digitalRead(RAIN_PIN) == LOW);

    khoaState();
    if (!isnan(t)) st.nhietDo = t;
    if (!isnan(h)) st.doAm = h;
    st.giaTriGas = gas;
    st.coMua = mua;
    st.canhBaoGas = (gas > nguongGas);
    moKhoaState();

    guiDHTLenLCD();
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

void taskLogic(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    bool coMua, muaCu;
    bool gas, gasCu;
    unsigned long now = millis();

    khoaState();
    coMua = st.coMua;
    muaCu = st.muaCu;
    gas = st.canhBaoGas;
    gasCu = st.canhBaoGasCu;
    moKhoaState();

    if (coMua != muaCu) {
      if (coMua) {
        guiLCD("TROI MUA", "KEO GIAN PHOI", true);
        beepBlock(2, 120, 120);
        keoGianPhoi();
      } else {
        guiLCD("HET MUA", "DAY GIAN PHOI", true);
        beepBlock(2, 120, 120);
        dayGianPhoi();
      }

      khoaState();
      st.muaCu = coMua;
      moKhoaState();
    }

    if (gas && !gasCu) {
      guiLCD("CANH BAO GAS", "MO CUA KHAN CAP", true);

      if (Blynk.connected()) guiCanhBaoGasEvent();

      moCuaCham();

      khoaState();
      st.cuaMoDoGas = true;
      st.dangMoCuaBangThe = false;
      st.cuaAppBat = false;
      moKhoaState();
    }

    if (!gas && gasCu) {
      guiLCD("GAS AN TOAN", "DONG CUA", true);

      dongCuaCham();

      khoaState();
      st.cuaMoDoGas = false;
      st.dangMoCuaBangThe = false;
      moKhoaState();

      khoiPhucDenTheoTrangThai();
    }

    khoaState();
    st.canhBaoGasCu = gas;
    moKhoaState();

    bool dangMoBangThe;
    bool cuaMoDoGas;
    bool cuaAppBat;
    unsigned long mocMo;

    khoaState();
    dangMoBangThe = st.dangMoCuaBangThe;
    cuaMoDoGas = st.cuaMoDoGas;
    cuaAppBat = st.cuaAppBat;
    mocMo = st.mocMoCua;
    moKhoaState();

    if (!gas && !cuaMoDoGas && !cuaAppBat && dangMoBangThe && (now - mocMo >= THOI_GIAN_MO_CUA_MS)) {
      dongCuaCham();

      khoaState();
      st.dangMoCuaBangThe = false;
      st.giuDenTrangSauThe = false;
      moKhoaState();

      guiLCD("CUA DA DONG", "QUET THE TU", true);
      khoiPhucDenTheoTrangThai();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void taskRFID(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    bool coThe = false;
    bool theDung = false;
    bool gasDangBaoDong = false;
    bool cuaDangMo = false;
    String uid = "";
    unsigned long now = millis();

    khoaState();
    gasDangBaoDong = st.canhBaoGas;
    cuaDangMo = st.cuaDangMo;
    moKhoaState();

    xSemaphoreTake(mutexSPI, portMAX_DELAY);
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      coThe = true;
      uid = docUIDThe();
      theDung = laTheHopLe();
    }
    xSemaphoreGive(mutexSPI);

    if (coThe) {
      bool boQuaTheLap = false;

      khoaState();
      if (uid == st.uidCuoi && (now - st.mocKhoaRFID < KHOA_THE_LAP_MS)) {
        boQuaTheLap = true;
      } else {
        st.uidCuoi = uid;
        st.mocKhoaRFID = now;
      }
      moKhoaState();

      if (boQuaTheLap) {
        xSemaphoreTake(mutexSPI, portMAX_DELAY);
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        xSemaphoreGive(mutexSPI);
        vTaskDelay(pdMS_TO_TICKS(150));
        continue;
      }

      if (gasDangBaoDong) {
        guiLCD("GAS BAO DONG", "THE TAM KHOA", true);

        xSemaphoreTake(mutexSPI, portMAX_DELAY);
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        xSemaphoreGive(mutexSPI);

        vTaskDelay(pdMS_TO_TICKS(800));
        continue;
      }

      if (theDung) {
        beepBlock(1, 100, 80);

        khoaState();
        cuaDangMo = st.cuaDangMo;
        moKhoaState();

        if (!cuaDangMo) moCuaCham();

        khoaState();
        st.dangMoCuaBangThe = true;
        st.cuaAppBat = false;
        st.mocMoCua = millis();
        st.soLanSaiThe = 0;
        st.giuDenTrangSauThe = true;
        moKhoaState();

        capNhatDenUuTien();
        guiLCD("MOI VAO", "THE DUNG", true);
      } else {
        int soLanSai = 0;

        beepBlock(2, 100, 80);

        khoaState();
        st.soLanSaiThe++;
        soLanSai = st.soLanSaiThe;
        moKhoaState();

        if (soLanSai < 3) {
          guiLCD("XIN THU LAI", "LAN THU " + String(soLanSai), true);

          for (int i = 0; i < 3; i++) {
            neoPixelDo();
            vTaskDelay(pdMS_TO_TICKS(180));
            tatNeoPixel();
            vTaskDelay(pdMS_TO_TICKS(120));
          }

          khoiPhucDenTheoTrangThai();
        } else {
          guiLCD("SAI 3 LAN", "KICH BAO DONG", true);

          if (Blynk.connected()) {
            guiCanhBaoRFIDEvent();
          }

          canhBaoTheSai3Lan();

          khoaState();
          st.soLanSaiThe = 0;
          moKhoaState();
        }
      }

      xSemaphoreTake(mutexSPI, portMAX_DELAY);
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      xSemaphoreGive(mutexSPI);

      vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(80));
  }
}

void taskGasAlarm(void *pvParameters) {
  (void) pvParameters;
  bool pha = false;

  for (;;) {
    bool gas;
    bool buzzerApp;
    bool chamPhu;

    khoaState();
    gas = st.canhBaoGas;
    buzzerApp = st.buzzerAppBat;
    chamPhu = st.chamTuCamBienPhu;
    moKhoaState();

    if (gas) {
      pha = !pha;

      if (pha) {
        neoPixelDo();
        datBuzzer(true);
      } else {
        tatNeoPixel();
        datBuzzer(false);
      }

      vTaskDelay(pdMS_TO_TICKS(CHU_KY_NHAY_GAS_MS));
    } else if (chamPhu) {
      datBuzzer(true);
      capNhatDenUuTien();
      vTaskDelay(pdMS_TO_TICKS(50));
    } else {
      capNhatDenUuTien();
      datBuzzer(buzzerApp);
      vTaskDelay(pdMS_TO_TICKS(80));
    }
  }
}

void taskNhanUARTPhu(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    while (slaveSerial.available()) {
      String line = slaveSerial.readStringUntil('\n');
      xuLyLenhTuESPPhu(line);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void taskSerial(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    bool chamPhu, troiTo;

    khoaState();
    chamPhu = st.chamTuCamBienPhu;
    troiTo = st.troiToTuCamBienPhu;
    moKhoaState();

    Serial.println("===== UART PHU =====");
    Serial.print("Cham phu : "); Serial.println(chamPhu ? "CO" : "KHONG");
    Serial.print("Troi toi : "); Serial.println(troiTo ? "CO" : "KHONG");
    Serial.println("====================");

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RAIN_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);

  mutexState = xSemaphoreCreateMutex();
  mutexSPI   = xSemaphoreCreateMutex();
  mutexNeo   = xSemaphoreCreateMutex();
  mutexUART  = xSemaphoreCreateMutex();

  khoaState();
  st.nhietDo = 0;
  st.doAm = 0;
  st.giaTriGas = 0;
  st.coMua = false;
  st.muaCu = false;
  st.canhBaoGas = false;
  st.canhBaoGasCu = false;
  st.relayDangBat = false;
  st.cuaDangMo = false;
  st.denDoDangBat = false;
  st.denTrangBat = false;
  st.denAppBat = false;
  st.buzzerBat = false;
  st.buzzerAppBat = false;
  st.dangMoCuaBangThe = false;
  st.mocMoCua = 0;
  st.soLanSaiThe = 0;
  st.giuDenTrangSauThe = false;
  st.cuaMoDoGas = false;
  st.cuaAppBat = false;
  st.servoDangChay = false;
  st.uidCuoi = "";
  st.mocKhoaRFID = 0;
  st.lcdDong1Cuoi = "";
  st.lcdDong2Cuoi = "";
  st.mocGuiLCDCuoi = 0;
  st.troiToTuCamBienPhu = false;
  st.chamTuCamBienPhu = false;
  moKhoaState();

  lcdSerial.begin(9600, SERIAL_8N1, -1, UART_TX_TO_LCD);
  slaveSerial.begin(UART_BAUD_SLAVE, SERIAL_8N1, UART_RX_FROM_SLAVE, -1);

  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();

  servoCua.setPeriodHertz(50);
  servoCua.attach(SERVO_PIN, 500, 2400);
  servoCua.write(GOC_DONG_CUA);

  led.begin();
  tatNeoPixel();

  dht.begin();
  gianPhoi.setSpeed(10);

  guiLCD("NHA CUA DUY", "DANG KHOI DONG", true);

  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.connect(10000);
  }

  timer.setInterval(2000L, guiDuLieuLenBlynk);

  vTaskDelay(pdMS_TO_TICKS(1000));
  guiLCD("NHA CUA DUY", "WELCOME", true);

  guiBaoThucSangESPPhu();

  xTaskCreatePinnedToCore(taskDocCamBien,  "taskDocCamBien",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskLogic,       "taskLogic",       6144, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskRFID,        "taskRFID",        6144, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(taskGasAlarm,    "taskGasAlarm",    3072, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(taskNhanUARTPhu, "taskNhanUARTPhu", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskSerial,      "taskSerial",      4096, NULL, 1, NULL, 1);
}

// ================= LOOP =================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!Blynk.connected()) {
      Blynk.connect(1000);
    }
    Blynk.run();
    timer.run();
  }

  delay(10);
}