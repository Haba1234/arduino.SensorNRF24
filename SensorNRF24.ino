// frimware MySensorsBootLoaderRF24 1MHz

// Enable debug prints to serial monitor
#define MY_DEBUG
// Enable and select radio type attached
#define MY_RADIO_NRF24
#define MY_RF24_PA_LEVEL RF24_PA_MAX
#define MY_BAUD_RATE 9600 // т.к. частота 1МГц
#define MY_NODE_ID 62
#define MY_TRANSPORT_WAIT_READY_MS 30000 //даем хх мс на подключение у родителю, потом переходим в цикл loop()
#define MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS 10000 // сколько не спать и пытаться найти родителя
#define MY_SMART_SLEEP_WAIT_DURATION_MS (1000ul) // таймаут перед сном при использовании SmartSleep, чтобы контроллер успел отправить данные
#define MY_RF24_POWER_PIN (5)

#define PRIMARY_CHILD_ID 1      // вибро
#define SECONDARY_CHILD_ID 2    // геркон
#define Vmax_ID 3               // Максимальное напряжение батарей (для расчета процентов)

//ноги
#define PRIMARY_BUTTON_PIN 2    // Arduino Digital I/O pin for button/reed switch
#define SECONDARY_BUTTON_PIN 3  // Arduino Digital I/O pin for button/reed switch
#define LED_PIN 4               // Светодиод
#define MAX1724_PIN  A4         // включение MAX1724

#include <MySensors.h>

int blinks = 100;
float Vmax = 2.8; // максимальное напряжение для расчета % батареи
byte count_error = 6;   // кол-во попыток на отправку сообщения
byte send_error = 6;    // кол-во попыток на отправку "я жив"
bool transportInit_ok = true;

MyMessage msg(PRIMARY_CHILD_ID, V_TRIPPED);
MyMessage msg2(SECONDARY_CHILD_ID, V_TRIPPED);

void preHwInit()
{
  // включаем стабилизатор питания перед включением радио
  pinMode(MAX1724_PIN, OUTPUT);
  digitalWrite(MAX1724_PIN, HIGH);
}

void before() 
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(blinks);  // wait ставить нельзя в этом куске
  digitalWrite(LED_PIN, LOW); 
}

void setup()  
{
  uint8_t tmp = loadState(Vmax_ID);
  if (tmp != 0xFF) // проверяем на левые значения
      Vmax = float(tmp)/10;
}

void presentation() {
   // Send the sketch version information to the gateway and Controller
   sendSketchInfo("Sensor nRF24", "2.0.6");
   present(PRIMARY_CHILD_ID, S_DOOR, "Vibro");  
   present(SECONDARY_CHILD_ID, S_DOOR, "Gerkon");
   present(Vmax_ID, S_MULTIMETER, "Battery");
   wait(5);
   request(Vmax_ID, V_VOLTAGE);
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
      
      uint16_t batteryV  = hwCPUVoltage();
      int batteryPcnt = ((batteryV/1000)/Vmax)*100;
      if (batteryPcnt > 100) batteryPcnt = 100;
      #ifdef MY_DEBUG
          Serial.print("Vmax: ");
          Serial.println(Vmax);
          
          Serial.print("Battery Voltage: ");
          Serial.print(batteryV);
          Serial.println(" mV");
    
          Serial.print("Battery percent: ");
          Serial.print(batteryPcnt);
          Serial.println(" %");
      #endif
      
      sendBatteryLevel(batteryPcnt);
      sleeping(0);
      //sendHeartbeat();
   } else {
      // если не вышло подключиться, то будем спать с заданным интервалом, чтобы не сдохла батарея
      // иначе не будет спать совсем и постоянно пытаться подключиться к родителю
      #ifdef MY_DEBUG
         Serial.println("Transport not ready!");
      #endif
     // wait(MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS);
     // if(isTransportReady()) sleeping(3600000); // если так и не подключились, то спим заданное время
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
}

void sleeping(unsigned long ms) {
    #ifdef MY_DEBUG
        Serial.println("nRF24 Power off. Slipping...");
    #endif
    sleep(PRIMARY_BUTTON_PIN-2, CHANGE, SECONDARY_BUTTON_PIN-2, CHANGE, ms); //SLEEP_TIME
    #ifdef MY_DEBUG
        Serial.println("nRF24 Power on. Init radio");
    #endif
    transportInit();  // инициализируем радио после сна
    //wait(1);
    //transportAssignNodeID(getNodeId());
    //wait(1);
}
