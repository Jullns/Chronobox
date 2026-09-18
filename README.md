# Chronomedi — Firmware da Caixa Organizadora Inteligente

Firmware embarcado (ESP32) da **Chronobox**, o dispositivo físico do sistema Chronomedi: uma solução IoT de saúde digital voltada à organização e administração assistida de medicamentos, com foco em usuários idosos e ambientes de cuidado de longa permanência.

<img width="535" height="412" alt="image" justify-self='center' src="https://github.com/user-attachments/assets/81ba8a93-acf6-4ddb-bbfa-f8b082b40054" />


# Visão geral

A Chronobox é um sistema embarcado com **seis compartimentos independentes**, cada um associado a um medicamento cadastrado pelo usuário através do aplicativo Chronomedi (React Native). A caixa:

- Controla os horários de administração de cada medicamento;
- Libera automaticamente apenas o compartimento correto, no horário correto, via servomotor;
- Emite alertas sonoros e luminosos configuráveis;
- Detecta tentativas de abertura de compartimentos fora do horário programado;
- Exibe informações em um display LCD;
- Funciona de forma **totalmente offline**, sincronizando eventos pendentes assim que a conexão com o aplicativo é restabelecida;
- Persiste todos os dados localmente (LittleFS), sobrevivendo a desligamentos.

# Hardware utilizado

| Componente | Modelo/Especificação |
|---|---|
| Microcontrolador | ESP32 DevKitC-1, 30 pinos, módulo WROOM-32 (Wi-Fi + BT + BLE 4) |
| Servomotores (x6) | MG90S (engrenagem metálica) |
| LEDs indicadores (x6) | LED 5mm verde + resistor 220-330Ω |
| Buzzer | Piezo passivo, 3,3-5V |
| Display | LCD 2004 (20x4) HD44780 + módulo adaptador I2C (PCF8574) |
| Expansor de I/O | Módulo PCF8574 (para os 6 reed switches) |
| Sensores de abertura | Reed switch magnético + ímã (x6) |
| Botões | 3x push button momentâneo (confirmar, liga/desliga, emergência) |
| Bateria | LiPo 3,7V, formato pouch, 2000-3000mAh, conector JST |
| Carregamento | Módulo TP4056 com proteção, entrada USB-C |
| Conversor de tensão | Boost DC-DC MT3608 (saída fixa 5V) |
| Estrutura | Impressão 3D (PLA/PETG) com estrutura em MDF |

# Frameworks e bibliotecas 

O firmware será desenvolvido utilizando o ambiente Arduino para ESP32, juntamente com bibliotecas específicas para o controle dos componentes utilizados no projeto. 
Entre as principais bibliotecas previstas estão: 
* ESP32Servo – controle dos seis servomotores; 
* ArduinoJson – criação, leitura e manipulação dos dados em formato JSON; 
* LittleFS – armazenamento dos arquivos e dados localmente na memória Flash da ESP32; 
* biblioteca do display utilizado – controle da interface visual da caixa; 
* bibliotecas de comunicação Bluetooth BLE – comunicação entre a ESP32 e o aplicativo; 
* bibliotecas de controle de tempo/RTC – gerenciamento dos horários programados para os medicamentos.

# Vídeo da modelagem 3d 
