#include <BLEDevice.h> 
#include <BLEServer.h> 
#include <BLEUtils.h> 
#include <BLE2902.h> 
#include <WiFi.h> 
#include <ESP32Servo.h> 
#include <ArduinoJson.h> 
#include <LittleFS.h> 
#include <Wire.h> 
#include <hd44780.h> 
#include <hd44780ioclass/hd44780_I2Cexp.h> 
#include <time.h> 


// Display LCD 
#define ENDERECO_LCD 0x27  
#define LCD_COLUNAS 20  
#define LCD_LINHAS 4 
hd44780_I2Cexp lcd(ENDERECO_LCD); 

 

// Expansor 
#define ENDERECO_EXPANSOR 0x20 
