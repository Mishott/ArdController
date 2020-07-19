/*********
Таблет в автомобил. Управление
*********/

#include <OneWire.h>           // комуникация с датчик за температура
#include <DallasTemperature.h> // датчик за температура
#include <Bounce2.h>           // за отстраняване на притрепването
#include <arduino-timer.h>     // Таймер 


// макроси за определяне на изминали дни
/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

/* Useful Macros for getting elapsed time */
//#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
//#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
//#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY) 

#define DEBUG 1

//ВХОДОВЕ
const byte one_wire_bus=8; // Датчик за вътрешна температура DS18B20 
const byte acc_pin=7 ;     // Датчик за ACC режим(ключа за запалване е в ACC +12V)

//ИЗХОДИ
const byte hall_relay_pin=3; // Управление на събуждане на таблета 
const byte usbPowerPin=6;    // захранвама USB HUB
const byte cam_power_pin=8;  // захранване на камера
const byte otg_relay_pin=4;  // Управлемие OTG режим 
const byte fan_relay_pin=2;  // Управление на вентилатора

//темепратура
const float tempMax   = 50;    // над тази темепратура вкл.вентилатора

// таймаути
const int checkACC    = 2UL;   // секунди през които да се проверява за АCC
const int powerUSB    = 4UL;   // секуди след. които се подава захранване на USB хъба
const int checkTemp   = 10UL;  // интервал в секунди за измерване на температурата
const int TabletOn    = 2UL;   // секунди след подаване на АЦЦ се вкл таблета
const int daysToOFF   = 2UL;   // дни след, които юе бъде изкл.всичко, ако не е подаван сигнал ACC
const int shutdowafter= 2UL;   // секунид след acc=0 гаси всичко


//променливи
boolean accOn  = false;       // ACC на колата
boolean IsOn = false;         // всички у-ва са включени
float tempC = 0;              // текуща температура 
boolean testMode = false;     // включен тестов режим
String recdata="";            // стринг от rs232    
DeviceAddress insideTempSensor;// масив с адреси на темп.датчик

// комуникация с датчика за температура
OneWire oneWire(one_wire_bus); 
DallasTemperature temp_sensor(&oneWire);

// ACC датчик - не трябв да реагира на къси вкл/изключвания
// затова минава през Bounce
Bounce ACCInput = Bounce();  


//Таймер 
auto timer  = timer_create_default(); 
Timer<>::Task chk_Temp;
Timer<>::Task USB_On; 
Timer<>::Task Hall_On;  
Timer<>::Task start_Off;  //процедуреа по спиране на периферията


//кога последно е изключен двигатела - използва се за да изгаси всичко след
//определен интервал от време daysToOFF
long int ACCDaysOff = 0;
long int LastMilis = 0; //последно мерене за ACCDaysOff

void setup(void)
{
  // пускаме серийния интерфейс
  Serial.begin(9600);
  //подкарваме вход ACC
  pinMode(acc_pin,INPUT);                 // Включваме PULLUP резистор. (Другия край е на маса)        
  ACCInput.attach(acc_pin);  // добавяме Bounce.
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
  if (!temp_sensor.getAddress(insideTempSensor, 0)) Serial.println("Unable to find address for Device 0");   
  temp_sensor.setResolution(insideTempSensor, 10);
  
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
  ACCInput.update() ;          // Сигурно ли е вклчен
  int accVal = ACCInput.read(); 
 
  if ( accVal == LOW) // логиката е обратна HIGH=ACC OFF
  {
    // действията се изпълняват ако колата е устойчиво вклчена        
    accOn = true; 
    //събуждаме таблета
    if(!IsOn){
        if(DEBUG)Serial.println("START UP"); 
        //0. спираме процеса на гасене (ако е започнал)
        timer.cancel(start_Off);
        
        //1.след TabletOn секунди се събужда таблета
        Hall_On = timer.at(millis()+TabletOn*1000,  HALL_start );  
       
        //2. включваме периферията след powerUSB секунди  
        USB_On = timer.at(millis()+powerUSB*1000,  powerOnUSB );   
      
        //3.започваме да проверяваме температурата през checkTemp секунди
        chk_Temp= timer.every(checkTemp*1000, checkTemperature);  
        if(DEBUG)Serial.println("TASKS STARTED.");         
      
        //започват периодичните задачи  taskTemp
        IsOn = true;        
      }       
   
    }
  else
  {
    // колата е загасена
    accOn = false;
    // действията се изпълнавата само, ако вече не са изпълнени
    // гасим периферията     
    start_Off= timer.at(millis()+shutdowafter*1000UL,  shutdow_all ); 
 
  }; 
 return true;
};


bool shutdow_all(void *){
  if(IsOn){
     if(DEBUG)Serial.println("SHUTDOWN STARTED"); 
      //анулираме  задачи
      timer.cancel(chk_Temp); 
      timer.cancel(USB_On);  
      timer.cancel(Hall_On);
      
      if(DEBUG)Serial.println("-TASKS CANCELED.");      
      // гасим таблета
      powerOffUSB();
      // спираме вентилатора
      powerOffFAN();
      // спираме камерата
      powerOffCamera();
      // изключваме режим OTG
      OTG_Off();
      // приспиваме таблета
      HALL_stop();
      IsOn = false;
    }  
    return false;// еднократно се изпълнява
}



bool checkTemperature(void *){
     temp_sensor.requestTemperatures(); //може да има повече от един сензор. затова питаме по индекс
     tempC = temp_sensor.getTempC(insideTempSensor);
     if(DEBUG){
        Serial.print("-Temp C: ");     
        Serial.println(tempC);
       }; 
     if (tempC>tempMax){
       powerOnFAN();        
      }
     else
     {
        powerOffFAN();      
      }
     return true;      
  };
 

bool HALL_start(void *){  
      digitalWrite(hall_relay_pin, HIGH);
      if(DEBUG)Serial.println("-HALL ON"); 
      return false; // изпълнява се един път
    } ;

void HALL_stop(){ 
      digitalWrite(hall_relay_pin, LOW);
      if(DEBUG)Serial.println("-HALL OFF"); 
    }  ;
void OTG_On(){  
      digitalWrite(otg_relay_pin, HIGH);
      if(DEBUG)Serial.println("-OTG ON"); 
    } ;

void OTG_Off(){ 
      digitalWrite(otg_relay_pin, LOW);
      if(DEBUG)Serial.println("-OTG OFF"); 
    }  ;

void powerOnCamera(){ 
      digitalWrite(cam_power_pin, HIGH);
      if(DEBUG)Serial.println("-CAMERA ON");
 
    } ;

void powerOffCamera(){ 
      digitalWrite(cam_power_pin, LOW);
      if(DEBUG)Serial.println("-CAMERA OFF");
   
    }  ;

void powerOnFAN(){ 
      digitalWrite(fan_relay_pin, HIGH);
      if(DEBUG)Serial.println("-FAN ON"); 
    } ;

void powerOffFAN(){ 
      digitalWrite(fan_relay_pin, LOW);
      if(DEBUG)Serial.println("-FAN OFF");    
    } ;  
    
bool  powerOnUSB(void *){ 
      digitalWrite(usbPowerPin, HIGH); 
      if(DEBUG)Serial.println("-USB ON"); 
      return false; // изпълнява се един път
    } ;

void powerOffUSB(){ 
      digitalWrite(usbPowerPin, LOW); 
      if(DEBUG)Serial.println("-USB OFF"); 
 
    }  ;   

// ако двигателя е изключен брои дните, ако е вкл. нулира бряча
void time(long val){  
  if(!IsOn){
    ACCDaysOff = elapsedDays(val); 
    if(DEBUG)Serial.print("-ACCDaysOff: ");    
    if(DEBUG)Serial.println(ACCDaysOff);
  }
  else {
      ACCDaysOff = 0;
    };   
}    

 

void self_test(){
  int v = 0;
    powerOnUSB(&v);
    delay(500);
    powerOffUSB();
    delay(500);
    
    powerOnFAN();
    delay(500);
    powerOnFAN();
    delay(500);

    powerOnCamera();
    delay(500);
    powerOffCamera();
    delay(500);
    
    OTG_On();
    delay(500);
    OTG_Off();
    delay(500);  

    HALL_start(&v);
    delay(500);
    HALL_stop();
    delay(500); 

    checkTemperature(&v);
      
  }
