/*********
Таблет в автомобил. Управление
*********/

#include <OneWire.h>           // комуникация с датчик за температура
#include <DallasTemperature.h> // датчик за температура
#include <Bounce2.h>           // за отстраняване на притрепването
#include <arduino-timer.h>     // Таймер 

#define DEBUG 1

//ВХОДОВЕ
const byte one_wire_bus=11; // Датчик за вътрешна температура DS18B20 
const byte acc_pin=10 ;     // Датчик за ACC режим(ключа за запалване е в ACC +12V)

//ИЗХОДИ
const byte hall_relay_pin=5; // Управление на събуждане на таблета 
const byte usbPowerPin=6;    // захранвама USB HUB
const byte cam_power_pin=7;  // захранване на камера
const byte otg_relay_pin=8;  // Управлемие OTG режим 
const byte fan_relay_pin=9;  // Управление на вентилатора

// други константи
const float tempMax = 50;    // над тази темепратура вкл.вентилатора
const int checkACC  = 2;     // секунди през които да се проверява за АCC
const int powerUSB  = 4;     // секуди след. които се подава захранване на USB хъба
const int checkTemp = 10;    // интервал в секунди за измерване на температурата
const int TabletOn = 2;      // секунди след подаване на АЦЦ се вкл таблета

//променливи
boolean accOn  = false;       // ACC на колата
boolean IsOn = false;         // всички у-ва са включени
float tempC = 0;              // текуща температура 

// комуникация с датчика за температура
OneWire oneWire(one_wire_bus); 
DallasTemperature temp_sensor(&oneWire);

// ACC датчик - не трябв да реагира на къси вкл/изключвания
// затова минава през Bounce
Bounce ACCInput = Bounce();  


//Таймер 
auto timer  = timer_create_default(); 
Timer<>::Task chk_Temp;
Timer<>::Task USB_Off; 
Timer<>::Task Hall_On; 

void setup(void)
{
  // пускаме серийния интерфейс
  Serial.begin(9600);
  //подкарваме вход ACC
  pinMode(acc_pin,INPUT);                 // Включваме PULLUP резистор. (Другия край е на маса)        
  ACCInput.attach(acc_pin,INPUT_PULLUP);  // добавяме Bounce.
  ACCInput.interval(50);                  // Не трябва да реагира на контакт под 50мс

  //подкарваме изходите
  pinMode(hall_relay_pin, OUTPUT);        // реле за управление на събуждане на таблета  
  digitalWrite(hall_relay_pin, LOW);
  pinMode(usbPowerPin, OUTPUT);           // реле за подаване на захранване към USB HUB
  digitalWrite(usbPowerPin, LOW);
  pinMode(cam_power_pin, OUTPUT);         // реле за управление на камера за задно виждане ??
  digitalWrite(cam_power_pin, LOW);
  pinMode(otg_relay_pin, OUTPUT);         // реле за управление на OTG 
  digitalWrite(otg_relay_pin, LOW);
  pinMode(fan_relay_pin, OUTPUT);         // реле за упр.ветилатор  
  digitalWrite(fan_relay_pin, LOW);
  
  //подкарваме датчик за вътрешна температура
  temp_sensor.begin();

  //таймер за АЦЦ през checkACC секунди
  timer.every(1000UL*checkACC, ACC_Control);
  if(DEBUG)Serial.println("SETUP COMPELTE"); 
 
}
 

void loop(void){     
  timer.tick(); //обновява таймера 
}


//Регистрира вкл/изкл на запалването на колата
//извършва се на всеки checkACC секунди
bool ACC_Control(void *)
{
  if(DEBUG)Serial.println("ACC CHECK STARTED"); 
  ACCInput.update() ;          // Сигурно ли е вклчен
  int accVal = ACCInput.read(); 
 
  if ( accVal == LOW) // логиката е обратна HIGH=ACC OFF
  {
    // действията се изпълняват ако колата е устойчиво вклчена    
    if(DEBUG)Serial.println("ACC ON"); 
    accOn = true; 
    //събуждаме таблета
    if(!IsOn){
        
        //1.след TabletOn секунди се събужда таблета
        Hall_On = timer.at(millis()+TabletOn*1000UL,  HALL_On );  
       
        //2. включваме периферията след powerUSB секунди  
        USB_Off = timer.at(millis()+powerUSB*1000UL,  powerOnUSB );   
      
        //3.започваме да проверяваме температурата през checkTemp секунди
        chk_Temp= timer.every(millis()+checkTemp*1000UL, checkTemperature);  
        if(DEBUG)Serial.println("TASKS STARTED.");         
      
        //започват периодичните задачи  taskTemp
        IsOn = true;        
      }       
   
    }
  else
  {
    // колата е загасена
    accOn = false;
    if(DEBUG)Serial.println("ACC OFF"); 

    // действията се изпълнавата само, ако вече не са изпълнени
    // гасим периферията      
    if(IsOn){
      //анулираме периодичните задачи
      timer.cancel(chk_Temp); 
      timer.cancel(USB_Off);   
      if(DEBUG)Serial.println("TASKS CANCELED.");      
      // гасим таблета
      powerOffUSB();
      // спираме вентилатора
      powerOffFAN();
      // спираме камерата
      powerOffCamera();
      // изключваме режим OTG
      OTG_Off();
      // приспиваме таблета
      HALL_Off();
      IsOn = false;
    }
  }; 
 return true;
};

bool checkTemperature(void *){
     tempC = temp_sensor.getTempCByIndex(0); //може да има повече от един сензор. затова питаме по индекс
     if(DEBUG){
        Serial.print("Celsius temperature: ");     
        Serial.println(tempC);
       }; 
     if (tempC>tempMax){
       powerOnFAN();        
      }
     else
     {
        powerOnFAN();      
      }
     return true;      
  };
 

bool HALL_On(void *){  
      digitalWrite(hall_relay_pin, HIGH);
      if(DEBUG)Serial.println("HALL ON"); 
      return false; // изпълнява се един път
    } ;

void HALL_Off(){ 
      digitalWrite(hall_relay_pin, LOW);
      if(DEBUG)Serial.println("HALL OFF"); 
    }  ;
void OTG_On(){  
      digitalWrite(otg_relay_pin, HIGH);
      if(DEBUG)Serial.println("OTG ON"); 
    } ;

void OTG_Off(){ 
      digitalWrite(otg_relay_pin, LOW);
      if(DEBUG)Serial.println("OTG OFF"); 
    }  ;

void powerOnCamera(){ 
      digitalWrite(cam_power_pin, HIGH);
      if(DEBUG)Serial.println("CAMERA ON");
 
    } ;

void powerOffCamera(){ 
      digitalWrite(cam_power_pin, LOW);
      if(DEBUG)Serial.println("CAMERA OFF");
   
    }  ;

void powerOnFAN(){ 
      digitalWrite(fan_relay_pin, HIGH);
      if(DEBUG)Serial.println("FAN ON"); 
    } ;

void powerOffFAN(){ 
      digitalWrite(fan_relay_pin, LOW);
      if(DEBUG)Serial.println("FAN OFF");    
    } ;  
    
bool  powerOnUSB(void *){ 
      digitalWrite(usbPowerPin, HIGH); 
      if(DEBUG)Serial.println("USB ON"); 
      return false; // изпълнява се един път
    } ;

void powerOffUSB(){ 
      digitalWrite(usbPowerPin, LOW); 
      if(DEBUG)Serial.println("USB OFF"); 
 
    }  ;   
