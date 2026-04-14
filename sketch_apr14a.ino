// =====================================================
// PROJETO SENTINELA v4.6 – QUEDA + BLE + WIFI + TWILIO
// Revisão para operação mais segura em bateria
// ESP32-C3 SuperMini
// Autor: Augusto Cesar Prusch Machado – RA 44386
// =====================================================

#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

// =====================================================
// [CONFIGURAÇÕES DE HARDWARE]
// =====================================================
#define I2C_SDA 4
#define I2C_SCL 5
#define PINO_BUZZER 1
#define PINO_BOTAO_PANICO 7
#define PINO_LED_INTERNO 8

#define MPU_ADDR_1 0x68
#define MPU_ADDR_2 0x69
#define MPU_WHO_AM_I_REG 0x75

// =====================================================
// [CONFIGURAÇÕES DO BEACON iBeacon]
// =====================================================
const uint8_t BEACON_UUID[16] = {
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
};

// =====================================================
// [CONFIGURAÇÕES DE REDE WI-FI 2.4 GHz]
// =====================================================
#define WIFI_SSID ""
#define WIFI_SENHA ""
#define WIFI_TIMEOUT 15000UL
#define RECONNECT_INTERVAL 10000UL

// =====================================================
// [CONFIGURAÇÕES DO TWILIO / WHATSAPP]
// PREENCHA COM SEUS DADOS REAIS
// =====================================================
#define TWILIO_ACCOUNT_SID   ""
#define TWILIO_AUTH_TOKEN    ""
#define TWILIO_WHATSAPP_FROM ""
#define TWILIO_WHATSAPP_TO   ""

#define DEVICE_ID ""

// =====================================================
// [LIMIARES E TEMPOS]
// =====================================================
#define DELTA_ACEL_MIN 20.0f
#define DELTA_GIRO_MIN 3.5f
#define CONFIRMACAO_QUEDA_MS 150UL
#define COOLDOWN_QUEDA_MS 20000UL
#define WARMUP_MS 5000UL
#define INTERVALO_BUZZER 500UL
#define INTERVALO_SCAN_BLE 5000UL
#define INTERVALO_RESUMO_BLE 10000UL
#define INTERVALO_RESCAN_MPU 10000UL
#define DEBOUNCE_BOTAO_MS 60UL

#define RSSI_MUITO_PROXIMO -50
#define RSSI_PROXIMO -70
#define RSSI_MEDIO -85

#define NUM_LEITURAS_CALIB 20

// =====================================================
// [ESTADOS GLOBAIS]
// =====================================================
bool alertaAtivo = false;
bool buzzerEstado = false;
unsigned long ultimaTrocaBuzzer = 0;

bool quedaDetectada = false;
bool quedaEmConfirmacao = false;
unsigned long inicioConfirmacaoQueda = 0;
unsigned long ultimaQuedaDetectada = 0;

bool sistemaPronto = false;
unsigned long tempoInicioSistema = 0;

bool estadoBotaoAnterior = HIGH;
unsigned long ultimaMudancaBotao = 0;

bool wifiConectado = false;
unsigned long ultimaTentativaWiFi = 0;
bool notificacaoEnviada = false;

bool mpuDisponivel = false;
uint8_t mpuEndereco = MPU_ADDR_1;
unsigned long ultimaTentativaMPU = 0;

volatile bool beaconEncontrado = false;
String localizacaoAtual = "Desconhecida";
int rssiValor = -100;
int rssiMelhor = -100;
uint16_t beaconMajor = 0;
uint16_t beaconMinor = 0;
unsigned long ultimoScanBLE = 0;
unsigned long ultimoResumoBLE = 0;

// Offsets MPU
float axOffset = 0, ayOffset = 0, azOffset = 0;
float gxOffset = 0, gyOffset = 0, gzOffset = 0;

// Leituras anteriores
float axPrev = 0, ayPrev = 0, azPrev = 0;
float gxPrev = 0, gyPrev = 0, gzPrev = 0;

struct BeaconInfo {
  uint16_t major;
  uint16_t minor;
  int rssi;
  String local;
  unsigned long ultimoVisto;
};

BeaconInfo beaconsDetectados[10];
int totalBeacons = 0;

// =====================================================
// [UTILITÁRIOS]
// =====================================================
String urlEncode(const String& text) {
  String encoded = "";
  char buf[4];

  for (size_t i = 0; i < text.length(); i++) {
    uint8_t c = static_cast<uint8_t>(text[i]);

    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", c);
      encoded += buf;
    }
  }

  return encoded;
}

String avaliarQualidadeSinal(int rssi) {
  if (rssi > RSSI_MUITO_PROXIMO) return "EXCELENTE";
  if (rssi > RSSI_PROXIMO) return "BOM";
  if (rssi > RSSI_MEDIO) return "FRACO";
  return "MUITO FRACO";
}

void linhaSerial() {
  Serial.println(F("--------------------------------------------------"));
}

void piscarLED(uint8_t vezes, uint16_t tempoMs) {
  for (uint8_t i = 0; i < vezes; i++) {
    digitalWrite(PINO_LED_INTERNO, LOW);
    delay(tempoMs);
    digitalWrite(PINO_LED_INTERNO, HIGH);
    delay(tempoMs);
  }
}

String gerarTimestampSeguro() {
  time_t agora = time(nullptr);

  if (agora < 100000) {
    unsigned long s = millis() / 1000UL;
    unsigned long m = s / 60UL;
    unsigned long h = m / 60UL;

    char fallback[40];
    snprintf(fallback, sizeof(fallback), "Uptime %02lu:%02lu:%02lu",
             h % 100UL, m % 60UL, s % 60UL);
    return String(fallback);
  }

  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localtime(&agora));
  return String(timestamp);
}

// =====================================================
// [I2C – DIAGNÓSTICO]
// =====================================================
bool i2cExisteEndereco(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void scanI2C() {
  linhaSerial();
  Serial.println(F("SCAN I2C DE DIAGNÓSTICO"));
  linhaSerial();

  int encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t erro = Wire.endTransmission();

    if (erro == 0) {
      Serial.printf("I2C: dispositivo encontrado em 0x%02X\n", addr);
      encontrados++;
    }
  }

  if (encontrados == 0) {
    Serial.println(F("I2C: nenhum dispositivo encontrado."));
  } else {
    Serial.printf("I2C: total encontrado = %d\n", encontrados);
  }

  linhaSerial();
}

bool lerRegistrador8(uint8_t addr, uint8_t reg, uint8_t &valor) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  uint8_t lidos = Wire.requestFrom((int)addr, 1, true);
  if (lidos != 1) return false;

  valor = Wire.read();
  return true;
}

bool escreverRegistrador8(uint8_t addr, uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(valor);
  return (Wire.endTransmission(true) == 0);
}

// =====================================================
// [MPU6050 – DETECÇÃO / INICIALIZAÇÃO / LEITURA]
// =====================================================
bool detectarMPU() {
  uint8_t who = 0;

  if (i2cExisteEndereco(MPU_ADDR_1)) {
    if (lerRegistrador8(MPU_ADDR_1, MPU_WHO_AM_I_REG, who)) {
      Serial.printf("MPU: resposta em 0x68 | WHO_AM_I = 0x%02X\n", who);
      mpuEndereco = MPU_ADDR_1;
      return true;
    }
  }

  if (i2cExisteEndereco(MPU_ADDR_2)) {
    if (lerRegistrador8(MPU_ADDR_2, MPU_WHO_AM_I_REG, who)) {
      Serial.printf("MPU: resposta em 0x69 | WHO_AM_I = 0x%02X\n", who);
      mpuEndereco = MPU_ADDR_2;
      return true;
    }
  }

  return false;
}

bool mpuInit() {
  if (!detectarMPU()) {
    Serial.println(F("MPU: não encontrado no barramento I2C."));
    return false;
  }

  if (!escreverRegistrador8(mpuEndereco, 0x6B, 0x00)) return false; // wake up
  if (!escreverRegistrador8(mpuEndereco, 0x1A, 0x03)) return false; // filtro
  if (!escreverRegistrador8(mpuEndereco, 0x1B, 0x18)) return false; // gyro ±2000 dps
  if (!escreverRegistrador8(mpuEndereco, 0x1C, 0x10)) return false; // accel ±8g

  delay(100);

  Serial.printf("MPU: inicializado com sucesso em 0x%02X\n", mpuEndereco);
  Serial.println(F("MPU: Gyro = ±2000 dps | Accel = ±8g"));
  return true;
}

bool mpuReadRaw(int16_t &rAx, int16_t &rAy, int16_t &rAz,
                int16_t &rGx, int16_t &rGy, int16_t &rGz) {
  Wire.beginTransmission(mpuEndereco);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;

  uint8_t recebidos = Wire.requestFrom((int)mpuEndereco, 14, true);
  if (recebidos != 14) return false;

  rAx = (Wire.read() << 8) | Wire.read();
  rAy = (Wire.read() << 8) | Wire.read();
  rAz = (Wire.read() << 8) | Wire.read();

  (void)Wire.read(); // temperatura high
  (void)Wire.read(); // temperatura low

  rGx = (Wire.read() << 8) | Wire.read();
  rGy = (Wire.read() << 8) | Wire.read();
  rGz = (Wire.read() << 8) | Wire.read();

  return true;
}

bool calibrarSensor() {
  if (!mpuDisponivel) return false;

  Serial.println(F("MPU: iniciando calibração..."));

  float sumAx = 0, sumAy = 0, sumAz = 0;
  float sumGx = 0, sumGy = 0, sumGz = 0;
  int amostrasValidas = 0;

  for (int i = 0; i < NUM_LEITURAS_CALIB; i++) {
    int16_t rAx, rAy, rAz, rGx, rGy, rGz;

    if (mpuReadRaw(rAx, rAy, rAz, rGx, rGy, rGz)) {
      sumAx += (rAx / 4096.0f) * 8.0f * 9.81f;
      sumAy += (rAy / 4096.0f) * 8.0f * 9.81f;
      sumAz += (rAz / 4096.0f) * 8.0f * 9.81f;

      sumGx += (rGx / 16.4f);
      sumGy += (rGy / 16.4f);
      sumGz += (rGz / 16.4f);

      amostrasValidas++;
    }

    delay(15);
  }

  if (amostrasValidas < 5) {
    Serial.println(F("MPU: falha na calibração, poucas amostras válidas."));
    return false;
  }

  axOffset = sumAx / amostrasValidas;
  ayOffset = sumAy / amostrasValidas;
  azOffset = sumAz / amostrasValidas;

  gxOffset = sumGx / amostrasValidas;
  gyOffset = sumGy / amostrasValidas;
  gzOffset = sumGz / amostrasValidas;

  axPrev = ayPrev = azPrev = 0;
  gxPrev = gyPrev = gzPrev = 0;

  Serial.println(F("MPU: calibração concluída com sucesso."));
  Serial.printf("MPU offsets | A(%.2f, %.2f, %.2f) | G(%.2f, %.2f, %.2f)\n",
                axOffset, ayOffset, azOffset, gxOffset, gyOffset, gzOffset);
  return true;
}

bool mpuRead(float* ax, float* ay, float* az, float* gx, float* gy, float* gz) {
  if (!mpuDisponivel) return false;

  int16_t rAx, rAy, rAz, rGx, rGy, rGz;
  if (!mpuReadRaw(rAx, rAy, rAz, rGx, rGy, rGz)) return false;

  *ax = (rAx / 4096.0f) * 8.0f * 9.81f - axOffset;
  *ay = (rAy / 4096.0f) * 8.0f * 9.81f - ayOffset;
  *az = (rAz / 4096.0f) * 8.0f * 9.81f - azOffset;

  *gx = (rGx / 16.4f) - gxOffset;
  *gy = (rGy / 16.4f) - gyOffset;
  *gz = (rGz / 16.4f) - gzOffset;

  return true;
}

void tentarRecuperarMPU() {
  if (mpuDisponivel) return;
  if (millis() - ultimaTentativaMPU < INTERVALO_RESCAN_MPU) return;

  ultimaTentativaMPU = millis();

  linhaSerial();
  Serial.println(F("MPU: tentativa automática de recuperação..."));

  if (mpuInit()) {
    mpuDisponivel = true;
    if (calibrarSensor()) {
      Serial.println(F("MPU: recuperação concluída com sucesso."));
    } else {
      Serial.println(F("MPU: encontrado, mas calibração falhou."));
      mpuDisponivel = false;
    }
  } else {
    Serial.println(F("MPU: ainda indisponível."));
  }

  linhaSerial();
}

// =====================================================
// [BLE – BEACON]
// =====================================================
void atualizarListaBeacons(uint16_t major, uint16_t minor, int rssi, const String& local) {
  for (int i = 0; i < totalBeacons; i++) {
    if (beaconsDetectados[i].major == major && beaconsDetectados[i].minor == minor) {
      beaconsDetectados[i].rssi = rssi;
      beaconsDetectados[i].local = local;
      beaconsDetectados[i].ultimoVisto = millis();

      if (rssi > rssiMelhor) {
        rssiMelhor = rssi;
        localizacaoAtual = local;
      }
      return;
    }
  }

  if (totalBeacons < 10) {
    beaconsDetectados[totalBeacons] = {major, minor, rssi, local, millis()};
    totalBeacons++;

    if (rssi > rssiMelhor) {
      rssiMelhor = rssi;
      localizacaoAtual = local;
    }
  }
}

String resolverLocal(uint16_t major, uint16_t minor) {
  if (major == 1 && minor == 1) return "Sala de Estar";
  if (major == 1 && minor == 2) return "Quarto Principal";
  if (major == 2 && minor == 1) return "Cozinha";
  if (major == 2 && minor == 2) return "Banheiro";
  if (major == 3 && minor == 1) return "Corredor";
  if (major == 3 && minor == 2) return "Varanda";
  return "Área Desconhecida";
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (!advertisedDevice.haveManufacturerData()) return;

    String md = advertisedDevice.getManufacturerData();
    if (md.length() < 25) return;

    uint8_t b0 = (uint8_t)md[0];
    uint8_t b1 = (uint8_t)md[1];
    uint8_t b2 = (uint8_t)md[2];
    uint8_t b3 = (uint8_t)md[3];

    if (b0 != 0x4C || b1 != 0x00 || b2 != 0x02 || b3 != 0x15) return;

    bool match = true;
    for (int i = 0; i < 16; i++) {
      if ((uint8_t)md[4 + i] != BEACON_UUID[i]) {
        match = false;
        break;
      }
    }
    if (!match) return;

    uint16_t major = ((uint8_t)md[20] << 8) | (uint8_t)md[21];
    uint16_t minor = ((uint8_t)md[22] << 8) | (uint8_t)md[23];
    int rssi = advertisedDevice.getRSSI();

    String local = resolverLocal(major, minor);

    beaconEncontrado = true;
    rssiValor = rssi;
    beaconMajor = major;
    beaconMinor = minor;

    atualizarListaBeacons(major, minor, rssi, local);

    Serial.printf("BLE: Beacon válido | Local=%s | Major=%u | Minor=%u | RSSI=%d dBm\n",
                  local.c_str(), major, minor, rssi);
  }
};

void initBLEScanner() {
  linhaSerial();
  Serial.println(F("BLE: inicializando scanner..."));

  BLEDevice::init("SENTINELA");
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);

  Serial.println(F("BLE: scanner pronto."));
  linhaSerial();
}

void scanBLEUnico(const char* motivo) {
  Serial.printf("BLE: scan único iniciado (%s)...\n", motivo);
  beaconEncontrado = false;

  BLEDevice::getScan()->start(2, false);
  BLEDevice::getScan()->clearResults();

  if (beaconEncontrado) {
    Serial.printf("BLE: melhor beacon [%s] | RSSI=%d | Major=%u | Minor=%u\n",
                  localizacaoAtual.c_str(), rssiMelhor, beaconMajor, beaconMinor);
  } else {
    Serial.println(F("BLE: nenhum beacon válido encontrado neste scan."));
  }
}

void scanBLEPeriodico() {
  if (millis() - ultimoScanBLE < INTERVALO_SCAN_BLE) return;

  Serial.println(F("BLE: iniciando scan periódico..."));
  beaconEncontrado = false;

  BLEDevice::getScan()->start(2, false);
  BLEDevice::getScan()->clearResults();

  ultimoScanBLE = millis();

  if (!beaconEncontrado) {
    Serial.println(F("BLE: scan finalizado sem beacon válido."));
  }
}

void exibirResumoBeacons() {
  linhaSerial();
  Serial.println(F("RESUMO DOS BEACONS"));

  if (totalBeacons == 0) {
    Serial.println(F("BLE: nenhum beacon registrado ainda."));
    linhaSerial();
    return;
  }

  for (int i = 0; i < totalBeacons; i++) {
    if (millis() - beaconsDetectados[i].ultimoVisto > 15000) continue;

    Serial.printf("[%d] %s | M:%u | m:%u | RSSI:%d | %s\n",
                  i + 1,
                  beaconsDetectados[i].local.c_str(),
                  beaconsDetectados[i].major,
                  beaconsDetectados[i].minor,
                  beaconsDetectados[i].rssi,
                  avaliarQualidadeSinal(beaconsDetectados[i].rssi).c_str());
  }

  Serial.printf("BLE: local atual = %s | melhor RSSI = %d dBm\n",
                localizacaoAtual.c_str(), rssiMelhor);
  linhaSerial();
}

// =====================================================
// [WI-FI / NTP / TWILIO]
// =====================================================
void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConectado = true;
    return;
  }

  linhaSerial();
  Serial.println(F("WI-FI: iniciando conexão..."));
  Serial.printf("WI-FI: SSID alvo = %s\n", WIFI_SSID);
  Serial.println(F("WI-FI: ESP32-C3 opera em 2.4 GHz."));
  linhaSerial();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - inicio) < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConectado = true;
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    Serial.println(F("WI-FI: conexão estabelecida com sucesso."));
    Serial.print(F("WI-FI: IP = "));
    Serial.println(WiFi.localIP());
  } else {
    wifiConectado = false;
    Serial.println(F("WI-FI: falha ao conectar."));
    WiFi.disconnect(true, false);
  }
}

void verificarWiFi() {
  if (wifiConectado && WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WI-FI: conexão perdida."));
    wifiConectado = false;
    notificacaoEnviada = false;
  }

  if (!wifiConectado && millis() - ultimaTentativaWiFi >= RECONNECT_INTERVAL) {
    ultimaTentativaWiFi = millis();
    conectarWiFi();
  }
}

bool enviarWhatsApp(const String& tipoEvento, const String& local, int rssi) {
  if (!wifiConectado) {
    Serial.println(F("TWILIO: envio abortado, Wi-Fi indisponível."));
    return false;
  }

  String mensagem;
  if (tipoEvento == "fall") {
    mensagem = "🚨 *QUEDA DETECTADA*\n\n";
  } else if (tipoEvento == "sos") {
    mensagem = "🆘 *SOS MANUAL*\n\n";
  } else {
    mensagem = "⚠️ *ALERTA SENTINELA*\n\n";
  }

  mensagem += "📱 *Dispositivo:* " + String(DEVICE_ID) + "\n";
  mensagem += "📍 *Local:* " + local + "\n";
  mensagem += "📡 *Sinal:* " + String(rssi) + " dBm\n";
  mensagem += "⏰ *Horário:* " + gerarTimestampSeguro() + "\n";
  mensagem += "\n⚠️ *Verifique o idoso!*";

  String url = "https://api.twilio.com/2010-04-01/Accounts/" +
               String(TWILIO_ACCOUNT_SID) +
               "/Messages.json";

  String payload = "To=" + urlEncode(String(TWILIO_WHATSAPP_TO)) +
                   "&From=" + urlEncode(String(TWILIO_WHATSAPP_FROM)) +
                   "&Body=" + urlEncode(mensagem);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(15000);

  Serial.println(F("TWILIO: enviando mensagem WhatsApp..."));
  Serial.println(F("TWILIO: método = POST"));

  if (!http.begin(client, url)) {
    Serial.println(F("TWILIO: falha ao iniciar conexão HTTPS."));
    return false;
  }

  http.setAuthorization(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST(payload);
  String resposta = http.getString();

  Serial.printf("TWILIO: HTTP code = %d\n", httpCode);
  if (resposta.length() > 0) {
    Serial.println(F("TWILIO: resposta da API:"));
    Serial.println(resposta);
  }

  http.end();

  if (httpCode == 201 || httpCode == 200) {
    Serial.println(F("TWILIO: mensagem enviada com sucesso."));
    return true;
  }

  Serial.println(F("TWILIO: falha no envio da mensagem."));
  return false;
}

// =====================================================
// [BUZZER / ALERTA]
// =====================================================
void ativarBuzzerAlerta() {
  if (alertaAtivo) return;

  alertaAtivo = true;
  buzzerEstado = HIGH;
  digitalWrite(PINO_BUZZER, buzzerEstado);
  ultimaTrocaBuzzer = millis();

  Serial.println(F("BUZZER: alerta ativado."));
}

void desativarBuzzer() {
  alertaAtivo = false;
  buzzerEstado = LOW;
  digitalWrite(PINO_BUZZER, LOW);

  Serial.println(F("BUZZER: alerta desativado."));
}

void atualizarBuzzer() {
  if (!alertaAtivo) return;

  if (millis() - ultimaTrocaBuzzer >= INTERVALO_BUZZER) {
    buzzerEstado = !buzzerEstado;
    digitalWrite(PINO_BUZZER, buzzerEstado);
    ultimaTrocaBuzzer = millis();
  }
}

// =====================================================
// [BOTÃO DE PÂNICO]
// =====================================================
void processarBotaoPanico() {
  bool leitura = digitalRead(PINO_BOTAO_PANICO);

  if (leitura != estadoBotaoAnterior) {
    ultimaMudancaBotao = millis();
    estadoBotaoAnterior = leitura;
  }

  if ((millis() - ultimaMudancaBotao) < DEBOUNCE_BOTAO_MS) return;

  static bool estadoEstavelAnterior = HIGH;
  if (leitura == estadoEstavelAnterior) return;

  estadoEstavelAnterior = leitura;

  if (leitura == LOW) {
    Serial.println(F("BOTÃO: pressionado."));

    if (alertaAtivo) {
      desativarBuzzer();
      quedaDetectada = false;
      quedaEmConfirmacao = false;
      notificacaoEnviada = false;
      Serial.println(F("BOTÃO: alerta cancelado manualmente."));
    } else {
      Serial.println(F("BOTÃO: disparando SOS manual."));
      ativarBuzzerAlerta();

      scanBLEUnico("sos-manual");

      if (wifiConectado) {
        enviarWhatsApp("sos", localizacaoAtual, rssiMelhor);
      }
    }
  }
}

// =====================================================
// [DETECÇÃO DE QUEDA]
// =====================================================
bool verificarQuedaBruta(float ax, float ay, float az, float gx, float gy, float gz) {
  float deltaAx = fabs(ax - axPrev);
  float deltaAy = fabs(ay - ayPrev);
  float deltaAz = fabs(az - azPrev);

  float deltaGx = fabs(gx - gxPrev);
  float deltaGy = fabs(gy - gyPrev);
  float deltaGz = fabs(gz - gzPrev);

  float deltaAcc = sqrtf(deltaAx * deltaAx + deltaAy * deltaAy + deltaAz * deltaAz);
  float deltaGyro = sqrtf(deltaGx * deltaGx + deltaGy * deltaGy + deltaGz * deltaGz);

  axPrev = ax;
  ayPrev = ay;
  azPrev = az;
  gxPrev = gx;
  gyPrev = gy;
  gzPrev = gz;

  return (deltaAcc > DELTA_ACEL_MIN) && (deltaGyro > DELTA_GIRO_MIN);
}

void processarQueda() {
  if (!mpuDisponivel) return;
  if (millis() - ultimaQuedaDetectada < COOLDOWN_QUEDA_MS) return;

  float ax, ay, az, gx, gy, gz;
  if (!mpuRead(&ax, &ay, &az, &gx, &gy, &gz)) {
    Serial.println(F("MPU: falha de leitura durante operação."));
    mpuDisponivel = false;
    return;
  }

  if (verificarQuedaBruta(ax, ay, az, gx, gy, gz)) {
    if (!quedaEmConfirmacao) {
      quedaEmConfirmacao = true;
      inicioConfirmacaoQueda = millis();
      Serial.println(F("QUEDA: evento bruto detectado, iniciando confirmação."));
    }

    if (millis() - inicioConfirmacaoQueda >= CONFIRMACAO_QUEDA_MS) {
      Serial.println(F("QUEDA: confirmada."));
      Serial.printf("QUEDA: local atual = %s | RSSI = %d dBm\n",
                    localizacaoAtual.c_str(), rssiMelhor);

      ativarBuzzerAlerta();
      scanBLEUnico("queda-confirmada");

      if (wifiConectado && !notificacaoEnviada) {
        if (enviarWhatsApp("fall", localizacaoAtual, rssiMelhor)) {
          notificacaoEnviada = true;
        }
      }

      quedaDetectada = true;
      quedaEmConfirmacao = false;
      ultimaQuedaDetectada = millis();
    }
  } else {
    if (quedaEmConfirmacao) {
      quedaEmConfirmacao = false;
      Serial.println(F("QUEDA: confirmação cancelada, evento não sustentado."));
    }
  }
}

// =====================================================
// [SETUP]
// =====================================================
void setup() {
  pinMode(PINO_BUZZER, OUTPUT);
  digitalWrite(PINO_BUZZER, LOW);
  buzzerEstado = LOW;
  alertaAtivo = false;

  pinMode(PINO_BOTAO_PANICO, INPUT_PULLUP);
  estadoBotaoAnterior = HIGH;

  pinMode(PINO_LED_INTERNO, OUTPUT);
  digitalWrite(PINO_LED_INTERNO, HIGH);

  Serial.begin(115200);
  delay(300);

  linhaSerial();
  Serial.println(F("PROJETO SENTINELA v4.6"));
  Serial.println(F("ESP32-C3 SuperMini"));
  Serial.println(F("Modo revisado para operação por bateria + Twilio"));
  linhaSerial();

  Serial.printf("Hardware | Buzzer GPIO=%d | Botão GPIO=%d | SDA=%d | SCL=%d\n",
                PINO_BUZZER, PINO_BOTAO_PANICO, I2C_SDA, I2C_SCL);
  Serial.printf("Wi-Fi | SSID=%s | Banda=2.4 GHz\n", WIFI_SSID);
  Serial.println(F("Boot | Buzzer desligado no início."));
  linhaSerial();

  tempoInicioSistema = millis();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);
  scanI2C();

  Serial.println(F("MPU: inicialização no boot..."));
  if (mpuInit()) {
    mpuDisponivel = true;
    if (!calibrarSensor()) {
      Serial.println(F("MPU: presente, mas calibração falhou. Sistema seguirá sem queda automática."));
      mpuDisponivel = false;
    }
  } else {
    Serial.println(F("MPU: falha no boot. Sistema seguirá com botão, buzzer, BLE e Wi-Fi."));
    mpuDisponivel = false;
  }

  initBLEScanner();
  scanBLEUnico("boot");

  conectarWiFi();
  ultimaTentativaWiFi = millis();

  sistemaPronto = true;
  Serial.println(F("Sistema: inicialização concluída."));
  if (mpuDisponivel) {
    Serial.println(F("Sistema: modo completo ativo."));
  } else {
    Serial.println(F("Sistema: modo degradado ativo (sem MPU até recuperação)."));
  }

  piscarLED(3, 100);
}

// =====================================================
// [LOOP]
// =====================================================
void loop() {
  verificarWiFi();
  tentarRecuperarMPU();
  processarBotaoPanico();
  atualizarBuzzer();

  if (!sistemaPronto) {
    delay(100);
    return;
  }

  if (millis() - tempoInicioSistema < WARMUP_MS) {
    delay(100);
    return;
  }

  scanBLEPeriodico();
  processarQueda();

  static unsigned long ultimoStatus = 0;
  if (millis() - ultimoStatus >= 5000UL) {
    ultimoStatus = millis();

    Serial.printf("STATUS | WiFi=%s | MPU=%s | Buzzer=%s | Local=%s | RSSI=%d\n",
                  wifiConectado ? "OK" : "OFF",
                  mpuDisponivel ? "OK" : "FALHA",
                  alertaAtivo ? "ATIVO" : "OFF",
                  localizacaoAtual.c_str(),
                  rssiMelhor);
  }

  if (millis() - ultimoResumoBLE >= INTERVALO_RESUMO_BLE) {
    ultimoResumoBLE = millis();
    exibirResumoBeacons();
  }

  delay(50);
}
