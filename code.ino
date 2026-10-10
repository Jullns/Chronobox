// CHRONOMEDI - versao com varios alertas e dados salvos (sem app ainda)

// Bibliotecas
#include <ESP32Servo.h>        // controla os servos
#include <LiquidCrystal_I2C.h> // controla o LCD
#include <ArduinoJson.h>       // le e escreve JSON (arquivo de dados)
#include <LittleFS.h>          // memoria flash (nao perde ao desligar)
#include <Wire.h>              // comunicacao I2C do LCD
#include <WiFi.h>              // conecta no WiFi (so para pegar a hora)
#include <time.h>              // funcoes de data e hora


// LCD 20 colunas x 4 linhas, endereco 0x27
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pinos utilizados
const int servoPins[6] = {5, 17, 16, 14, 12, 13}; // fios laranja
const int ledPins[6]   = {4, 2, 15, 25, 26, 27};  // fios verde
const int buzzerPin    = 32;                      // fio rosa
const int botaoPower   = 19;                      // liga/desliga
const int botaoDose    = 18;                      // confirma que tomou o remedio

//objeto, utilizado para girar os servos
Servo servos[6];


//  Configuracoes 
//esse reset é só para esse teste
const bool RESETAR_DADOS = false;

// Tempo maximo de um alerta em milissegundos (60000 = 1 minuto).
// 0 = o alerta nunca expira, so para quando o botao for apertado.
const unsigned long TEMPO_LIMITE_ALERTA = 0;

// Abaixo ou igual a esta quantidade, aparece "Estoque baixo" na tela
const int ESTOQUE_MINIMO = 1; // estoque minimo de 1 a pessoa n pode colocar 0 remédios

//  Dados dos remedios (posicao 0 = slot 1, posicao 1 = slot 2, ...) 
// Os 3 primeiros tem o MESMO horario para testar varios alertas juntos.
// Teste
String nomes[6]   = {"Dipirona", "Vitamina D", "Amoxicilina", "Losartana", "Omeprazol", "Paracetamol"};
String horas[6]   = {"14:30", "14:30", "14:30", "14:36", "14:38", "14:40"};
int dosagem[6]    = {1, 1, 1, 1, 1, 2};       // comprimidos por dose
int quantidade[6] = {20, 30, 14, 30, 28, 16}; // estoque restante

//  Variaveis de controle 
bool sistemaLigado = true;
// Ex: apertou o botao power -> vira false e a tela apaga.

bool alertaAtivo[6] = {false};
// Um true/false por slot: esse slot esta alertando agora?
// Ex: Dipirona (slot 1) e Vitamina D (slot 2) no mesmo horario -> alertaAtivo[0] e alertaAtivo[1] ficam true.

unsigned long inicioAlerta[6] = {0};
// Ex: alerta comecou em 120000 ms; agora sao 150000 ms -> ele esta tocando ha 30 s.

int slotMostrado = -1;
// Ex: slotMostrado = 2 -> LCD mostra o remedio do slot 3 (o vetor comeca em 0).

int ultimoMinuto = -1;
// Ultimo minuto que o programa ja conferiu. So olha os horarios quando o minuto muda.

bool estadoPisca = false;
// Ex: true -> LED aceso e bip; false -> LED apagado e silencio.

unsigned long ultimoPisca = 0;
// Quando foi a ultima alternancia do pisca. Serve de cronometro sem travar o programa.
// Ex: se (millis() - ultimoPisca >= 500) -> passou meio segundo, hora de alternar de novo.

unsigned long ultimaTroca = 0;
// Quando o LCD trocou de remedio pela ultima vez (com varios alertas, troca a cada 3 s).
// Ex: 3 alertas ativos -> a tela mostra Dipirona, 3 s depois Vitamina D, 3 s depois Amoxicilina.

// LCD
// Ele corta o que passa de 20 caracteres, mas também completa com espaços o que tem menos de 20
void escreverLinha(int linha, String texto) {// melhor que o clear, a tela ficaria piscando a cada segundo
  if (texto.length() > 20) texto = texto.substring(0, 20);
  while (texto.length() < 20) texto += ' ';
  lcd.setCursor(0, linha);
  lcd.print(texto);
}

// Tela normal: hora, data e aviso de estoque baixo
void mostrarHorario(struct tm t) {
  char texto[21];
  escreverLinha(0, "   CHRONOMEDIBOX   ");
  sprintf(texto, "Hora: %02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  escreverLinha(1, texto);
  sprintf(texto, "Data: %02d/%02d/%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  escreverLinha(2, texto);

  // procura o primeiro remedio com estoque baixo
  String aviso = "Aguardando horario";
  for (int i = 0; i < 6; i++) {
    if (quantidade[i] <= ESTOQUE_MINIMO) {
      aviso = "Estoque baixo: S" + String(i + 1);
      break;
    }
  }
  escreverLinha(3, aviso);
}

// Tela de alerta: dados do remedio que esta em slotMostrado
void mostrarAlertaNaTela() {
  int i = slotMostrado;
  if (i < 0) return;

  // conta quantos alertas existem e qual a posicao do que esta na tela
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
// DADOS NA FLASH (LittleFS + JSON)
// =====================================================

// Salva nome, hora, dose e estoque dos 6 slots em /medicamentos.json
void salvarDados() {
  JsonDocument doc;
  JsonArray lista = doc["medicamentos"].to<JsonArray>();

  for (int i = 0; i < 6; i++) {
    JsonObject o = lista.add<JsonObject>();
    o["slot"] = i;
    o["nome"] = nomes[i];
    o["hora"] = horas[i];
    o["dosagem"] = dosagem[i];
    o["quantidade"] = quantidade[i];
  }

  File f = LittleFS.open("/medicamentos.json", "w");
  serializeJson(doc, f);
  f.close();
}

// Le /medicamentos.json e preenche os vetores. Retorna false se nao existir
bool carregarDados() {
  if (!LittleFS.exists("/medicamentos.json")) return false;

  File f = LittleFS.open("/medicamentos.json", "r");
  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, f);
  f.close();
  if (erro) return false;

  int i = 0;
  for (JsonObject o : doc["medicamentos"].as<JsonArray>()) {
    if (i >= 6) break;
    nomes[i] = o["nome"].as<String>();
    horas[i] = o["hora"].as<String>();
    dosagem[i] = o["dosagem"];
    quantidade[i] = o["quantidade"];
    i++;
  }
  return (i == 6);
}

// Guarda um evento ("tomado" ou "nao_tomado") em /eventos.txt, uma linha por evento.
// No futuro, esses eventos serao enviados ao app quando o Bluetooth conectar.
void registrarEvento(String tipo, int slot) {
  struct tm t;
  char hora[20] = "sem-hora";
  if (getLocalTime(&t, 0)) {
    sprintf(hora, "%02d/%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
  }

  String linha = tipo + ";" + nomes[slot] + ";slot" + String(slot + 1) + ";" +
                 String(hora) + ";restam" + String(quantidade[slot]);

  File f = LittleFS.open("/eventos.txt", "a"); // "a" = acrescenta no fim
  f.println(linha);
  f.close();

  Serial.println("EVENTO: " + linha);
}

// Mostra no monitor serial os eventos guardados (util para testar)
void mostrarEventosNoSerial() {
  if (!LittleFS.exists("/eventos.txt")) return;
  Serial.println("--- Eventos guardados ---");
  File f = LittleFS.open("/eventos.txt", "r");
  while (f.available()) Serial.write(f.read());
  f.close();
  Serial.println("-------------------------");
}

// =====================================================
// ALERTAS (varios ao mesmo tempo)
// =====================================================

// Quantos slots estao alertando agora
int contarAlertas() {
  int total = 0;
  for (int i = 0; i < 6; i++) if (alertaAtivo[i]) total++;
  return total;
}

// Acha o proximo slot com alerta depois de "depois". Retorna -1 se nao houver
int proximoAlerta(int depois) {
  for (int k = 1; k <= 6; k++) {
    int idx = (depois + k + 6) % 6;
    if (alertaAtivo[idx]) return idx;
  }
  return -1;
}

// Comeca o alerta de um slot: abre a tampa
void iniciarAlerta(int i) {
  alertaAtivo[i] = true;
  inicioAlerta[i] = millis();
  servos[i].write(90); // abre a tampa
  if (slotMostrado == -1) slotMostrado = i; // primeiro alerta vai para a tela
  ultimoPisca = 0;                          // forca atualizar a tela ja
  Serial.println("ALERTA: " + nomes[i] + " (slot " + String(i + 1) + ")");
}

// Termina o alerta de um slot. tomou = true (dose confirmada) ou false (expirou)
void encerrarAlerta(int i, bool tomou) {
  servos[i].write(0);            // fecha a tampa
  digitalWrite(ledPins[i], LOW); // apaga o LED
  alertaAtivo[i] = false;

  if (tomou) {
    quantidade[i] -= dosagem[i]; // desconta do estoque
    if (quantidade[i] < 0) quantidade[i] = 0;
    salvarDados();               // guarda o novo estoque na flash
    registrarEvento("tomado", i);
  } else {
    registrarEvento("nao_tomado", i);
  }

  if (contarAlertas() == 0) noTone(buzzerPin); // ninguem mais alertando
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Botoes: apertado = HIGH
  pinMode(botaoPower, INPUT_PULLDOWN);
  pinMode(botaoDose, INPUT_PULLDOWN);

  // LEDs e buzzer
  for (int i = 0; i < 6; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  pinMode(buzzerPin, OUTPUT);

  // Servos (comecam fechados em 0 graus)
  for (int i = 0; i < 6; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(0);
  }

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  escreverLinha(0, "   CHRONOMEDI   ");
  escreverLinha(1, "Carregando dados...");

  // Dados salvos: carrega se existir, senao usa os exemplos e salva
  LittleFS.begin(true); // true = formata na primeira vez
  if (RESETAR_DADOS || !carregarDados()) {
    salvarDados();
    Serial.println("Dados de exemplo salvos");
  } else {
    Serial.println("Dados carregados da flash");
  }
  mostrarEventosNoSerial();

  // WiFi + hora da internet (fuso de Brasilia, UTC-3)
  escreverLinha(1, "Conectando WiFi...");
  Serial.begin(9600);
  Serial.print("Connecting to WiFi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  if (WiFi.status() == WL_CONNECTED) {
    configTime(-3 * 3600, 0, "pool.ntp.org");
    struct tm t;
    if (getLocalTime(&t, 5000)) Serial.println("Hora sincronizada!");
    else Serial.println("Falha ao pegar a hora");
    WiFi.disconnect(true); // nao precisa mais do WiFi
  } else {
    Serial.println("Sem WiFi");
  }
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // ----- Botao liga/desliga -----
  if (digitalRead(botaoPower) == HIGH) {
    sistemaLigado = !sistemaLigado;

    if (sistemaLigado) {
      lcd.backlight();
    } else {
      // standby: desliga tudo (os alertas abertos sao cancelados sem registrar)
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
    delay(300); // evita ler o mesmo aperto duas vezes
  }

  if (!sistemaLigado) return; // em standby nao faz mais nada

  // ----- Pega a hora atual -----
  struct tm agora;
  if (!getLocalTime(&agora)) {
    escreverLinha(1, "Sem hora (NTP)");
    delay(500);
    return;
  }

  // ----- Confere se chegou a hora de algum remedio (1 vez por minuto) -----
  if (agora.tm_min != ultimoMinuto) {
    ultimoMinuto = agora.tm_min;

    char textoHora[6];
    sprintf(textoHora, "%02d:%02d", agora.tm_hour, agora.tm_min);

    // sem "break": se varios remedios tem o mesmo horario, todos disparam
    for (int i = 0; i < 6; i++) {
      if (horas[i] == String(textoHora) && !alertaAtivo[i]) {
        iniciarAlerta(i);
      }
    }
  }

  // alerta tempo limite, aqui é só para quando o usuário coloca um 
  //limite específico para o alerta tocar ex: 10min ininterruptos
  if (TEMPO_LIMITE_ALERTA > 0) {
    for (int i = 0; i < 6; i++) {
      if (alertaAtivo[i] && millis() - inicioAlerta[i] > TEMPO_LIMITE_ALERTA) {
        encerrarAlerta(i, false);
        if (slotMostrado == i) slotMostrado = proximoAlerta(i);
        ultimoPisca = 0;
      }
    }
  }

  // ----- Durante os alertas -----
  if (contarAlertas() > 0) {
    // garante que a tela esta mostrando um slot que ainda alerta
    if (slotMostrado == -1 || !alertaAtivo[slotMostrado]) {
      slotMostrado = proximoAlerta(slotMostrado);
    }

    // com mais de um alerta, troca o remedio da tela a cada 3 segundos
    if (contarAlertas() > 1 && millis() - ultimaTroca >= 3000) {
      ultimaTroca = millis();
      slotMostrado = proximoAlerta(slotMostrado);
      ultimoPisca = 0; // atualiza a tela agora
    }

    // a cada 500 ms: pisca os LEDs, apita o buzzer e atualiza a tela
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

    // botao de dose: confirma o remedio que esta na tela
    if (digitalRead(botaoDose) == HIGH) {
      int confirmado = slotMostrado;
      encerrarAlerta(confirmado, true);
      slotMostrado = proximoAlerta(confirmado); // passa para o proximo, se houver
      ultimaTroca = millis();
      ultimoPisca = 0;
      delay(300);
    }
  } else {
    // sem alertas: mostra o relogio
    slotMostrado = -1;
    mostrarHorario(agora);
    delay(200);
  }
}
