//Receptor

#include <WiFi.h>

const char* ssid = "TIGO-1F62";
const char* password = "4G79ED802616";

WiFiServer server(1234);

// ===== CONFIG =====
uint8_t nodeID = 2;

uint32_t P = 61;
uint32_t Q = 53;
uint32_t S = 12345;

uint8_t PSN = 0;
uint64_t currentKey;
WiFiClient activeClient;

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


// ===== DESCIFRADO =====
void op1(char* msg, uint64_t key, uint8_t length) {
  for (int i = 0; i < length; i++)
    msg[i] ^= (key >> (i % 8)) & 0xFF;
}

void op2(char* msg, uint8_t length) {
  for (int i = 0; i < length; i++)
    msg[i] -= 1;
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
    msg[i] = msg[i] - ((key >> ((i + 3) % 8)) & 0x0F);
  }
}

void decrypt(char* msg, uint8_t length) {

  switch (PSN % 4) {

    case 0:
      op2(msg, length);
      op1(msg, currentKey, length);
      break;

    case 1:
      op3(msg, length);
      op2(msg, length);
      break;

    case 2:
      op1(msg, currentKey, length);
      op3(msg, length);
      break;

    case 3:
      op4(msg, currentKey, length);
      break;
  }
}


// ===== PROCESAR =====
void processMessage(Message msg) {

  PSN = msg.psn;
  currentKey = msg.key; 

  switch (msg.type) {

    case 0:
      Serial.println("FCM recibido");
      break;

    case 1:
      decrypt(msg.payload, msg.length);
      Serial.print("Mensaje: ");
      Serial.write(msg.payload, msg.length);
      Serial.println();
      break;

    case 2:
      Serial.println("KUM recibido");
      break;

    case 3:
      Serial.println("LCM recibido");
      if (activeClient){
        activeClient.stop();
      }
      PSN = 0;
      currentKey = 0;

      break;
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  server.begin();

  Serial.println("RECEPTOR listo");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ===== LOOP =====

void loop() {

  // esperar cliente
  if (!activeClient.connected()) {
    activeClient = server.available();

    if (activeClient) {
      Serial.println("Cliente conectado");
    }
  }

  // procesar mensajes
  if (activeClient.connected()) {

    while (activeClient.available() >= sizeof(Message)) {
      Message msg;
      activeClient.read((uint8_t*)&msg, sizeof(msg));
      processMessage(msg);
    }

  }
}