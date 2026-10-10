// CHRONOMEDI - versao com varios alertas e dados salvos (sem app ainda)

// Bibliotecas
#include <ESP32Servo.h>        // controla os servos
#include <LiquidCrystal_I2C.h> // controla o LCD
#include <ArduinoJson.h>       // le e escreve JSON (arquivo de dados)
#include <Preferences.h>       // armazena dados sem erro de particao no Wokwi
#include <Wire.h>              // comunicacao I2C do LCD
#include <WiFi.h>              // conecta no WiFi (so para pegar a hora)
#include <time.h>              // funcoes de data e hora

// Objeto para gerenciar preferências (substitui o LittleFS no simulador)
Preferences preferencias;

// LCD 20 colunas x 4 linhas, endereco 0x27
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pinos utilizados
const int servoPins[6] = {5, 17, 16, 14, 12, 13}; // fios laranja
const int ledPins[6]   = {4, 2, 15, 25, 26, 27};  // fios verde
const int buzzerPin    = 32;                      // fio rosa
const int botaoPower   = 19;                      // liga/desliga
const int botaoDose    = 18;                      // confirma que tomou o remedio

Servo servos[6];

// Configuracoes 
const bool RESETAR_DADOS = false;
const unsigned long TEMPO_LIMITE_ALERTA = 0;
const int ESTOQUE_MINIMO = 1; 

// Dados dos remedios
String nomes[6]   = {"Dipirona", "Vitamina D", "Amoxicilina", "Losartana", "Omeprazol", "Paracetamol"};
String horas[6]   = {"14:30", "14:30", "14:30", "14:36", "14:38", "14:40"};
int dosagem[6]    = {1, 1, 1, 1, 1, 2}; 
int quantidade[6] = {20, 30, 14, 30, 28, 16}; 

// Variaveis de controle 
bool sistemaLigado = true;
bool alertaAtivo[6] = {false};
unsigned long inicioAlerta[6] = {0};
int slotMostrado = -1;
int ultimoMinuto = -1;
bool estadoPisca = false;
unsigned long ultimoPisca = 0;
unsigned long ultimaTroca = 0;

unsigned long ultimaSincronizacaoNTP = 0;
const unsigned long INTERVALO_SINCRONIZACAO = 120000;
int ultimoSegundoTela = -1;

void escreverLinha(int linha, String texto) {
  if (texto.length() > 20) texto = texto.substring(0, 20);
  while (texto.length() < 20) texto += ' ';
  lcd.setCursor(0, linha);
  lcd.print(texto);
}

void mostrarHorario(struct tm t) {
  char texto[21];
  escreverLinha(0, "   CHRONOMEDIBOX   ");
  sprintf(texto, "Hora: %02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  escreverLinha(1, texto);
  sprintf(texto, "Data: %02d/%02d/%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  escreverLinha(2, texto);

  String aviso = "Aguardando horario";
  for (int i = 0; i < 6; i++) {
    if (quantidade[i] <= ESTOQUE_MINIMO) {
      aviso = "Estoque baixo: S" + String(i + 1);
      break;
    }
  }
  escreverLinha(3, aviso);
}

void mostrarAlertaNaTela() {
  int i = slotMostrado;
  if (i < 0) return;

  int total = 0, posicao = 0;
  for (int k = 0; k < 6; k++) {
    if (alertaAtivo[k]) {
      total++;
      if (k <= i) posicao = total;
    }
  }

  char texto[21];
  escreverLinha(0, nomes[i]);
  sprintf(texto, "Slot:%d  Hora:%s", i + 1, horas[i].c_str());
  escreverLinha(1, texto);
  sprintf(texto, "Dose:%d Restam:%d", dosagem[i], quantidade[i]);
  escreverLinha(2, texto);
  sprintf(texto, "Alerta %d/%d  [OK]", posicao, total);
  escreverLinha(3, texto);
}

// =====================================================
// DADOS SALVOS (Preferences)
// =====================================================
void salvarDados() {
  preferencias.begin("medicamentos", false);
  for (int i = 0; i < 6; i++) {
    String kQtd = "qtd_" + String(i);
    preferencias.putInt(kQtd.c_str(), quantidade[i]);
  }
  preferencias.end();
}

bool carregarDados() {
  preferencias.begin("medicamentos", true);
  if (!preferencias.isKey("qtd_0")) {
    preferencias.end();
    return false; // Se nao existir chave salva, retorna falso para usar os padroes
  }
  for (int i = 0; i < 6; i++) {
    String kQtd = "qtd_" + String(i);
    quantidade[i] = preferencias.getInt(kQtd.c_str(), quantidade[i]);
  }
  preferencias.end();
  return true;
}

void registrarEvento(String tipo, int slot) {
  struct tm t;
  char hora[20] = "sem-hora";
  if (getLocalTime(&t, 0)) {
    sprintf(hora, "%02d/%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
  }
  Serial.println("EVENTO: " + tipo + ";" + nomes[slot] + ";slot" + String(slot + 1) + ";" + String(hora) + ";restam" + String(quantidade[slot]));
}

// =====================================================
// ALERTAS
// =====================================================
int contarAlertas() {
  int total = 0;
  for (int i = 0; i < 6; i++) if (alertaAtivo[i]) total++;
  return total;
}

int proximoAlerta(int depois) {
  for (int k = 1; k <= 6; k++) {
    int idx = (depois + k + 6) % 6;
    if (alertaAtivo[idx]) return idx;
  }
  return -1;
}

void iniciarAlerta(int i) {
  alertaAtivo[i] = true;
  inicioAlerta[i] = millis();
  servos[i].write(90); 
  if (slotMostrado == -1) slotMostrado = i; 
  ultimoPisca = 0;                          
  Serial.println("ALERTA: " + nomes[i] + " (slot " + String(i + 1) + ")");
}

void encerrarAlerta(int i, bool tomou) {
  servos[i].write(0);            
  digitalWrite(ledPins[i], LOW); 
  alertaAtivo[i] = false;

  if (tomou) {
    quantidade[i] -= dosagem[i]; 
    if (quantidade[i] < 0) quantidade[i] = 0;
    salvarDados();               
    registrarEvento("tomado", i);
  } else {
    registrarEvento("nao_tomado", i);
  }

  if (contarAlertas() == 0) noTone(buzzerPin); 
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(botaoPower, INPUT_PULLDOWN);
  pinMode(botaoDose, INPUT_PULLDOWN);

  for (int i = 0; i < 6; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  pinMode(buzzerPin, OUTPUT);

  for (int i = 0; i < 6; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(0);
  }

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  escreverLinha(0, "   CHRONOMEDI   ");
  escreverLinha(1, "Carregando dados...");

  if (RESETAR_DADOS || !carregarDados()) {
    salvarDados();
    Serial.println("Dados de exemplo salvos");
  } else {
    Serial.println("Dados carregados da memoria");
  }

  // WiFi + hora NTP
  escreverLinha(1, "Conectando WiFi...");
  Serial.print("Connecting to WiFi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  if (WiFi.status() == WL_CONNECTED) {
    configTime(-3 * 3600, 0, "pool.ntp.org");
    struct tm t;
    if (getLocalTime(&t, 5000)) {
      Serial.println("Hora sincronizada!");
      ultimaSincronizacaoNTP = millis();
    } else {
      Serial.println("Falha ao pegar a hora");
    }
  } else {
    Serial.println("Sem WiFi");
  }
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  if (digitalRead(botaoPower) == HIGH) {
    sistemaLigado = !sistemaLigado;

    if (sistemaLigado) {
      lcd.backlight();
    } else {
      noTone(buzzerPin);
      for (int i = 0; i < 6; i++) {
        digitalWrite(ledPins[i], LOW);
        servos[i].write(0);
        alertaAtivo[i] = false;
      }
      slotMostrado = -1;
      lcd.clear();
      lcd.noBacklight();
    }
    delay(300); 
  }

  if (!sistemaLigado) return; 

  // Ressincronização periódica do relógio via NTP
  if (millis() - ultimaSincronizacaoNTP > INTERVALO_SINCRONIZACAO) {
    if (WiFi.status() == WL_CONNECTED) {
      struct tm tempT;
      if (getLocalTime(&tempT, 1000)) {
        ultimaSincronizacaoNTP = millis();
        Serial.println("Relogio ressincronizado com o servidor NTP.");
      }
    }
  }

  struct tm agora;
  if (!getLocalTime(&agora)) {
    escreverLinha(1, "Sem hora (NTP)");
    delay(100);
    return;
  }

  if (agora.tm_min != ultimoMinuto) {
    ultimoMinuto = agora.tm_min;

    char textoHora[6];
    sprintf(textoHora, "%02d:%02d", agora.tm_hour, agora.tm_min);

    for (int i = 0; i < 6; i++) {
      if (horas[i] == String(textoHora) && !alertaAtivo[i]) {
        iniciarAlerta(i);
      }
    }
  }

  if (contarAlertas() > 0) {
    if (slotMostrado == -1 || !alertaAtivo[slotMostrado]) {
      slotMostrado = proximoAlerta(slotMostrado);
    }

    if (contarAlertas() > 1 && millis() - ultimaTroca >= 3000) {
      ultimaTroca = millis();
      slotMostrado = proximoAlerta(slotMostrado);
      ultimoPisca = 0; 
    }

    if (millis() - ultimoPisca >= 500) {
      ultimoPisca = millis();
      estadoPisca = !estadoPisca;

      for (int i = 0; i < 6; i++) {
        if (alertaAtivo[i]) digitalWrite(ledPins[i], estadoPisca);
      }
      if (estadoPisca) tone(buzzerPin, 2000);
      else noTone(buzzerPin);

      mostrarAlertaNaTela();
    }

    if (digitalRead(botaoDose) == HIGH) {
      int confirmado = slotMostrado;
      encerrarAlerta(confirmado, true);
      slotMostrado = proximoAlerta(confirmado); 
      ultimaTroca = millis();
      ultimoPisca = 0;
      delay(300);
    }
  } else {
    slotMostrado = -1;
    if (agora.tm_sec != ultimoSegundoTela) {
      ultimoSegundoTela = agora.tm_sec;
      mostrarHorario(agora);
    }
  }
}
