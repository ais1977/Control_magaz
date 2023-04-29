// "+" - добавляется сам автоматом
// "C" - вызывает режим смены номера ( загорается "+" в низу экрана)
// "D" - сохраняет номер, если нажать "С" в ходе набора номера выходит из режима нечего не сохраняя


#include <Arduino.h>
#include <SoftwareSerial.h>
#include <microDS18B20.h>
#include <GyverOLED.h>
#include <Keypad.h>

#define pin_DS 4                    // датчик температуры
#define pin_IR 5                    // ИК датчик
#define pin_RX 2                    // пин RX Arduino (TX SIM800L)
#define pin_TX 3                    // пин TX Arduino (RX SIM800L)
#define SMSopen "Okno open"          // Текст СМС сообщения при открытии
#define SMSclose "Okno close"        // Текст СМС сообщения при закрытии
#define SMS_T_Text "Temperature is " // Текст СМС сообщения о температуре
#define SMS_cod "TEMP"               // Текст в смс чтобы получить ответ о температуре
#define UP_T 15                      // Верхняя граница температуры
#define DOWN_T -3                    // Нижняя граница температуры
String numberSMS = "+79281455513";   // Номер абонента для СМС

//настройки клавы
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = { 12, 11, 10, 9 };
byte colPins[COLS] = { A0, A1, A2, A3 };
char hexaKeys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

Keypad Key = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
MicroDS18B20<pin_DS> sensor;
SoftwareSerial SIM800(pin_RX, pin_TX);
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

String _response = "";               // Переменная для хранения ответа модуля
String inputString;                  // хранение сообщения
int threashold = 30;                 // Порог срабатывания
int status = 10;
int T_SMS = 0; // Последняя отправленная температура
int T;
uint32_t tm; // переменная таймера
char key;

int DS();
String sendATCommand(String cmd, bool waiting);
String waitResponse();
void distances();
void read_SMS();
void CheckTemperature();
void sendSMS(String text, String phone);
void olled(int temp, String tel);
void nomer();

void setup()
{
  Serial.begin(115000); // Скорость обмена данными с компьютером
  SIM800.begin(9600);   // Скорость обмена данными с модемом
  pinMode(pin_IR, INPUT_PULLUP);

  Serial.println("Start!");
  sendATCommand("AT", true);                       // Отправили AT для настройки скорости обмена данными
  _response = sendATCommand("AT+CLIP=1", true);    // Включаем АОН
  _response = sendATCommand("AT+CMGF=1;&W", true); // Включаем текстовый режима SMS (Text mode) и сразу сохраняем значение (AT&W)!

  oled.init();  // инициализация
  oled.clear(); // очистить дисплей (или буфер)
  delay(1000);

  T = DS();
  distances();
  delay(1000);
}

void loop()
{

  if (millis() - tm >= 5000)
  {
    tm = millis();
    T = DS();
    olled(T, numberSMS);
    CheckTemperature();
    read_SMS();
  }

  distances();

  key = Key.getKey();
  if (key != NO_KEY)
  {
    if ('C' == key)
    {
      Serial.println("new nom");
      nomer();
    }
  }
}

void olled(int temp, String tel)
{
  oled.clear();                    // очистить дисплей (или буфер)
  oled.setScale(1);                // масштаб шрифта (1-4)
  oled.setCursorXY(10, 3);         // поставить курсор для символа
  oled.print(F("Температура C:")); // печатай что угодно

  oled.setScale(2); // масштаб шрифта (1-4)
  if (temp >= 0)
  {
    oled.setCursorXY(40, 24);
  } // поставить курсор для символа
  else
  {
    oled.setCursorXY(35, 24);
  }

  oled.print(temp, 1); // температура
  oled.setScale(1);
  oled.setCursorXY(5, 54);
  oled.print(tel);
}

int DS()
{
  sensor.requestTemp(); // запрос температуры
  delay(1000);
  if (sensor.readTemp())
    return sensor.getTempInt();
  else
    Serial.println("error");
  return 0;
}

void distances()
{
  int cm = digitalRead(pin_IR);
  if (cm)
  {
    if (status != 1)
    {
      status = 1;
      String smsText = SMS_T_Text + String(T) + " " + SMSopen;
      sendSMS(smsText, numberSMS); // Open
      Serial.println(F("Окно открыто"));
    }
  }
  else
  {
    if (status != 0)
    {
      status = 0;
      String smsText = SMS_T_Text + String(T) + " " + SMSclose;
      sendSMS(smsText, numberSMS); // Close
      Serial.println(F("Окно закрыто"));
    }
  }
}

void CheckTemperature()
{
  if ((T >= UP_T && T_SMS != UP_T) || (T <= DOWN_T && T_SMS != DOWN_T))
  {
    if (T >= UP_T)
    {
      T_SMS = UP_T;
    }
    if (T <= DOWN_T)
    {
      T_SMS = DOWN_T;
    }
    String smsText = SMS_T_Text + T;
    sendSMS(smsText, numberSMS); // Отправка текущей температуры
  }
}

void sendSMS(String text, String phone)
{
  Serial.print(F("Send sms with text: "));
  sendATCommand("AT+CMGS=\"" + phone + "\"", true);        // Переходим в режим ввода текстового сообщения
  sendATCommand(text + "\r\n" + (String)((char)26), true); // После текста отправляем перенос строки и Ctrl+Z
  Serial.println(text);
  Serial.println("");
  delay(1000);
}

String sendATCommand(String cmd, bool waiting)
{
  String _resp = "";   // Переменная для хранения результата
  Serial.println(cmd); // Дублируем команду в монитор порта
  SIM800.println(cmd); // Отправляем команду модулю
  if (waiting)
  {                         // Если необходимо дождаться ответа...
    _resp = waitResponse(); // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
    if (_resp.startsWith(cmd))
    { // Убираем из ответа дублирующуюся команду
      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
    }
    Serial.println(_resp); // Дублируем ответ в монитор порта
  }
  return _resp; // Возвращаем результат. Пусто, если проблема
}

String waitResponse()
{                                   // Функция ожидания ответа и возврата полученного результата
  String _resp = "";                // Переменная для хранения результата
  long _timeout = millis() + 10000; // Переменная для отслеживания таймаута (10 секунд)
  while (!SIM800.available() && millis() < _timeout)
  {
  }; // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
  if (SIM800.available())
  {                              // Если есть, что считывать...
    _resp = SIM800.readString(); // ... считываем и запоминаем
  }
  else
  {                               // Если пришел таймаут, то...
    Serial.println("Timeout..."); // ... оповещаем об этом и...
  }
  return _resp; // ... возвращаем результат. Пусто, если проблема
}

void read_SMS()
{
  if (SIM800.available()) // Проверяем, если есть доступные данные
  {
    delay(100);
    while (SIM800.available()) // Проверяем, есть ли еще данные.
    {
      inputString += SIM800.read(); // Записываем считанный байт в массив inputString
    }
    delay(100);
    Serial.println(inputString);           // Отправка в "Мониторинг порта" считанные данные
    inputString.toUpperCase();             // Меняем все буквы на заглавные
    if (inputString.indexOf(SMS_cod) > -1) // Проверяем полученные данные
    {
      String smsText = SMS_T_Text + T;
      Serial.println(smsText);
      sendSMS(smsText, numberSMS); // Отправка текущей температуры
    }
    delay(100);
    if (inputString.indexOf("OK") == -1)
    {
      sendATCommand("AT+CMGDA=\"DEL ALL\"", true); // Удаляем все сообщения
      delay(100);
    }
    inputString = "";
  }
}

void nomer()
{
  String new_tel = "+";
  char key;
  byte i = 1;
  olled(0, new_tel);
  while (i)
  {
    key = Key.getKey();
    switch (key)
    {
    case 'C':
      i = 0;
      Serial.println("brek");
      break;

    case 'D':
      i = 0;
      numberSMS = new_tel;
      Serial.println("number save");
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      new_tel += key;
      olled(0, new_tel);
    }
  }
}
