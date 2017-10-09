// frimware MySensorsBootLoaderRF24 1MHz

// Enable debug prints to serial monitor
#define MY_DEBUG
// Enable and select radio type attached
#define MY_RADIO_NRF24
#define MY_RF24_PA_LEVEL RF24_PA_MAX
#define MY_BAUD_RATE 9600 // т.к. частота 1МГц
#define MY_NODE_ID 65
#define MY_TRANSPORT_WAIT_READY_MS 30000 //даем хх мс на подключение у родителю, потом переходим в цикл loop()
#define MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS 10000 // сколько не спать и пытаться найти родителя

#define PRIMARY_CHILD_ID 1      // вибро
#define SECONDARY_CHILD_ID 2    // геркон
#define Vmax_ID 3               // Максимальное напряжение батарей (для расчета процентов)
//#define MULTIPLIER_ID 4         // коэффициент преобразования напряжения в %

//ноги
#define PRIMARY_BUTTON_PIN 2    // Arduino Digital I/O pin for button/reed switch
#define SECONDARY_BUTTON_PIN 3  // Arduino Digital I/O pin for button/reed switch
#define LED_PIN 4               // Светодиод
#define nRF24_POWER_PIN 5       // Управление питанием модуля nRF24
#define BATTERY_SENSE_PIN  A5   // Считывание состояния батареи
#define MAX1724_PIN  A4         // включение MAX1724

#include <MySensors.h>
#include <avr/wdt.h>

int blinks = 100;
float Vmax = 2.8; // максимальное напряжение для расчета % батареи
float Kconst = 0.003363075;      //резистор 470кОм
//float Kconst = 0.004333659;      //резистор 330кОм
byte count_error = 6;   // кол-во попыток на отправку сообщения
byte send_error = 6;    // кол-во попыток на отправку "я жив"
bool transportInit_ok = true;

MyMessage msg(PRIMARY_CHILD_ID, V_TRIPPED);
MyMessage msg2(SECONDARY_CHILD_ID, V_TRIPPED);

void before() // конфигурация до инициализации nRF24
{
  pinMode(nRF24_POWER_PIN, OUTPUT);
  digitalWrite(nRF24_POWER_PIN, LOW);
  pinMode(MAX1724_PIN, OUTPUT);
  digitalWrite(MAX1724_PIN, HIGH);
  
  // подключаем опорное напряжение 1.1 V для аналоговых измерений
#if defined(__AVR_ATmega2560__)
   analogReference(INTERNAL1V1);
#else
   analogReference(INTERNAL);
#endif

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(blinks);  // wait ставить нельзя в этом куске
  digitalWrite(LED_PIN, LOW); 
  wdt_enable(WDTO_8S);
}

void setup()  
{
  uint8_t tmp = loadState(Vmax_ID);
  if (tmp != 0xFF) // проверяем на левые значения
      Vmax = float(tmp)/10;
  int sensorValue = analogRead(BATTERY_SENSE_PIN);
}

void presentation() {
   // Send the sketch version information to the gateway and Controller
   sendSketchInfo("Sensor nRF24", "2.0.5");
   present(PRIMARY_CHILD_ID, S_DOOR, "Vibro");  
   present(SECONDARY_CHILD_ID, S_DOOR, "Gerkon");
   present(Vmax_ID, S_MULTIMETER, "Battery");
   wait(5);
   request(Vmax_ID, V_VOLTAGE);
   wait(5); // ждем ответа от сервера
   request(Vmax_ID, V_IMPEDANCE);
   wait(5); // ждем ответа от сервера
}

void loop()
{
  uint8_t value;
  static uint8_t sentValue=2;
  static uint8_t sentValue2=2;
  
  if(isTransportReady()){
      #ifdef MY_DEBUG
         Serial.println("Transport ready!");
      #endif 
      value = digitalRead(PRIMARY_BUTTON_PIN);
      if (value != sentValue) {
         // Value has changed from last transmission, send the updated value
         for ( byte i = 0; i < count_error; i++ ) { 
            bool tmp1 = send(msg.set(value==HIGH ? 1 : 0), false);
            #ifdef MY_DEBUG
                Serial.print("Ack1 = ");Serial.println(tmp1);
            #endif
            if (tmp1) break;
            wait (10);
         }
         sentValue = value;
         digitalWrite(LED_PIN, HIGH);
         wait(blinks);
         digitalWrite(LED_PIN, LOW);
      }
    
      value = digitalRead(SECONDARY_BUTTON_PIN);
      
      if (value != sentValue2) {
         // Value has changed from last transmission, send the updated value
         for ( byte i = 0; i < count_error; i++ ) { 
            bool tmp = send(msg2.set(value==HIGH ? 1 : 0));
            #ifdef MY_DEBUG
                Serial.print("Ack2 = ");Serial.println(tmp);
            #endif
            if (tmp) break;
            wait (10);
         }
         sentValue2 = value;
         digitalWrite(LED_PIN, HIGH);
         wait(blinks);
         digitalWrite(LED_PIN, LOW);
      }
      
    // get the battery Voltage
      int sensorValue = analogRead(BATTERY_SENSE_PIN);
      #ifdef MY_DEBUG
         Serial.println(sensorValue);
      #endif
       
      // 1M, 470K divider across battery and using internal ADC ref of 1.1V
      // Sense point is bypassed with 0.1 uF cap to reduce noise at that point
      // ((1e6+470e3)/470e3)*1.1 = Vmax = 3.44 Volts
      // ((1e6+330e3)/330e3)*1.1 = Vmax = 4.433 Volts
      // 3.44/1023 = Volts per bit = 0.003363075
      // 4.433/1023 = Volts per bit = 0.0043336591723688
       
      float batteryV  = sensorValue * Kconst;
      int batteryPcnt = (batteryV/Vmax)*100;
      if (batteryPcnt > 100) batteryPcnt = 100;
      #ifdef MY_DEBUG
          Serial.print("Vmax: ");
          Serial.println(Vmax);
          
          Serial.print("Battery Voltage: ");
          Serial.print(batteryV);
          Serial.println(" V");
    
          Serial.print("Battery percent: ");
          Serial.print(batteryPcnt);
          Serial.println(" %");
      #endif
      sendBatteryLevel(batteryPcnt);
      sleeping(0);
      sendHeartbeat();
   } else {
      // если не вышло подключиться, то будем спать с заданным интервалом, чтобы не сдохла батарея
      // иначе не будет спать совсем и постоянно пытаться подключиться к родителю
      #ifdef MY_DEBUG
         Serial.println("Transport not ready!");
      #endif
      wait(MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS);
      if(isTransportReady()) sleeping(3600000); // если так и не подключились, то спим заданное время
   }   
}

void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if ((message.sensor == Vmax_ID)&&(message.type==V_VOLTAGE)) {
     Vmax = message.getFloat();
     uint8_t tmp = uint8_t(Vmax*10);
     if (tmp > 0) saveState(Vmax_ID, tmp);
     // Write some debug info
     #ifdef MY_DEBUG
        Serial.println(tmp);
        Serial.print("Incoming change for sensor:");
        Serial.print(message.sensor);
        Serial.print(", New status: ");
        Serial.println(Vmax);
     #endif
  }
    if ((message.sensor == Vmax_ID)&&(message.type==V_IMPEDANCE)) {
     Kconst = message.getFloat();
     // Write some debug info
     #ifdef MY_DEBUG
        Serial.print("Incoming change for sensor:");
        Serial.print(message.sensor);
        Serial.print(", New status: ");
        Serial.println(Kconst);
     #endif
  }
}

void sleeping(unsigned long ms) {
    #ifdef MY_DEBUG
        Serial.println("nRF24 Power off. Slipping...");
    #endif
    digitalWrite(MAX1724_PIN, LOW); // отключаем работу MAX1724
    digitalWrite(nRF24_POWER_PIN, HIGH); // отключаем nRF24 перед сном
    delay(1);
    wdt_reset();      // сброс сторожевого таймера
    wdt_disable();
    sleep(PRIMARY_BUTTON_PIN-2, CHANGE, SECONDARY_BUTTON_PIN-2, CHANGE, ms); //SLEEP_TIME
    #ifdef MY_DEBUG
        Serial.println("nRF24 Power on. Init radio");
    #endif
    wdt_enable(WDTO_4S);
    digitalWrite(MAX1724_PIN, HIGH); // включаем работу MAX1724
    delay(1);
    digitalWrite(nRF24_POWER_PIN, LOW); // включаем nRF24 после сна
    delay(1);  // вместо wait() т.к. нрф еще не иницилизирован и глючит
    transportInit();  // инициализируем радио после сна
    wait(1);
    transportAssignNodeID(getNodeId());
    wait(1);
}
