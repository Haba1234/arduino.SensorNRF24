# arduino.SensorNRF24
Sensor Atmega328p + nRF24L01

Прошивка для версии 1Мгц на врутреннем кварце

### Changelog
#### 2.0.6 (2018-01-14)
* убрал внешний делитель для считывания питания батарей. Теперь функция hwCPUVoltage()
* убрал сторожевой таймер
* управление питанием радио через #define MY_RF24_POWER_PIN (5) и библиотеку MySensors

#### 2.0 (2017-10-09)
* Создан
* MySensors v2.2.0 beta

### Планы
* в место Sleep использовать SmartSleep
* добавить в драйвер ИоБ поддержу SmartSleep (отправка команд в момент пробуждения устройства)
