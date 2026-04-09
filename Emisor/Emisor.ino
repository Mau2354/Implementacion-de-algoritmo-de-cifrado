//Emisor

#include <WiFi.h>

const char* ssid = "TIGO-1F62";
const char* password = "4G79ED802616";

const char* serverIP = "192.168.0.10";
const int port = 1234;

WiFiClient client;

// ===== CONFIG =====
uint8_t nodeID = 1;

uint32_t P = 61;
uint32_t Q = 53;
uint32_t S = 12345;

uint8_t PSN = 0;
uint64_t currentKey;
bool sessionActive = false;

// ===== TABLA DE CLAVES =====
uint64_t keyTable[10];
int keyCount = 0;

// ===== MENSAJE =====
struct Message {
  uint8_t nodeID;
  uint8_t type;
  uint8_t psn;
  uint64_t key;
  uint8_t length;
  char payload[100];
};

// ===== FUNCIONES =====
uint64_t fs(uint32_t P, uint32_t S) {
  return (P ^ S) + (P * S);
}

uint64_t fg(uint64_t P0, uint32_t Q) {
  return (P0 * Q) ^ (P0 + Q);
}

uint32_t fm(uint32_t S, uint32_t Q) {
  return (S ^ Q) + (S << 1);
}

// ===== GENERAR CLAVE =====
void generateKey() {
  uint64_t P0 = fs(P, S);
  currentKey = fg(P0, Q);
  S = fm(S, Q);

  // guardar en tabla (circular)
  keyTable[keyCount % 10] = currentKey;
  keyCount++;
}

// ===== MOSTRAR TABLA =====
void printKeyTable() {
  Serial.println("\n🔑 Tabla de claves:");

  int total = min(keyCount, 10);

  for (int i = 0; i < total; i++) {
    Serial.print("Key ");
    Serial.print(i);
    Serial.print(": 0x");
    Serial.println((unsigned long long)keyTable[i], HEX);
  }

  Serial.println("--------------------");
}

// ===== CIFRADO =====
void op1(char* msg, uint64_t key, uint8_t length) {
  for (int i = 0; i < length; i++)
    msg[i] ^= (key >> (i % 8)) & 0xFF;
}

void op2(char* msg, uint8_t length) {
  for (int i = 0; i < length; i++)
    msg[i] += 1;
}

void op3(char* msg, uint8_t length) {
  for (int i = 0; i < length / 2; i++) {
    char t = msg[i];
    msg[i] = msg[length - i - 1];
    msg[length - i - 1] = t;
  }
}

void op4(char* msg, uint64_t key, uint8_t length) {
  for (int i = 0; i < length; i++) {
    msg[i] = msg[i] + ((key >> ((i + 3) % 8)) & 0x0F);
  }
}

void encrypt(char* msg, uint8_t length) {

  generateKey();

  switch (PSN % 4) {

    case 0:
      op1(msg, currentKey, length);
      op2(msg, length);
      break;

    case 1:
      op2(msg, length);
      op3(msg, length);
      break;

    case 2:
      op3(msg, length);
      op1(msg, currentKey, length);
      break;

    case 3:
      op4(msg, currentKey, length);
      break;
  }

  PSN = (msg[0] ^ currentKey) & 0x0F;
}

// Resetear llaves LCM
void resetKeys() {
  for (int i = 0; i < 10; i++) {
    keyTable[i] = 0;
  }
  keyCount = 0;
  currentKey = 0;
}
// ===== ENVIO =====
void sendMessage(uint8_t type, String text) {

  if (!client.connected()) {

    if (!sessionActive && type != 0) {
      Serial.println("Debes iniciar contacto /fcm");
      return;
    }

    if (!client.connect(serverIP, port)) {
      Serial.println("Error conexión");
      return;
    }

    Serial.println("Conectado al servidor");
  }

  Message msg;
  msg.nodeID = nodeID;
  msg.type = type;
  msg.psn = PSN;
  msg.length = text.length();

  memset(msg.payload, 0, sizeof(msg.payload));
  memcpy(msg.payload, text.c_str(), msg.length);

  if (type == 1) {
    encrypt(msg.payload, msg.length);
  }

  msg.key = currentKey;

  client.write((uint8_t*)&msg, sizeof(msg));

  // CONTROL DE SESIÓN
  if (type == 0) {
    sessionActive = true;
  }

  if (type == 3) {
    Serial.println("LCM enviado → cerrando conexión...");
    client.stop();
    sessionActive = false;
    PSN = 0;
    resetKeys();
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println("EMISOR listo");
  Serial.println("Comandos: /fcm /kum /lcm /keys");
}

// ===== LOOP =====
void loop() {

  if (Serial.available()) {

    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "/keys") {
      printKeyTable();
    }
    else if (input == "/fcm") sendMessage(0, "INIT");
    else if (input == "/kum") sendMessage(2, "UPDATE");
    else if (input == "/lcm") sendMessage(3, "END");
    else sendMessage(1, input);
  }
}
