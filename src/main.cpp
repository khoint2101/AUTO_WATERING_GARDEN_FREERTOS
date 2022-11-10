#if  CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

/*Include Library*/
#include <Arduino.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>      // OLED 0.96 inch
#include <Adafruit_SSD1306.h> // OLED 0.96 inch
#include "logo"
#include "humi"
#include "temp"
#include "pump_icon"


/*GPIO Definition*/
#define DHTPIN 19
#define DHTTYPE DHT11
#define RELAY_PIN 23
#define SOILMOISTURE_PIN 35
#define AUTO_BUTTON 33
#define PUMP_BUTTON 15   //on off PUMP
#define MANU_BUTTON 18     //manual button

//Screen OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ----------SEMAPHORE----------------*/
SemaphoreHandle_t xBinarySemaphore;

/* -----------QUEUE-------------------*/
QueueHandle_t xQueueTemp, xQueueHumidity, xQueueStateBTN, xQueueCheckMode;

/*------------MUTEX-------------------*/
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

SemaphoreHandle_t xMutex;

TaskHandle_t xHandle1;
TaskHandle_t xHandle2;
TaskHandle_t xHandle3;

static int BTN_STATE ;


/*Task Definition*/
void readSensor(void * params);
void taskChooseMode(void * params);
void taskAutoMode(void * params);
void taskManualMode(void * params);
void taskPumpOn(void * params);
void taskPumpOff(void * params);  // 6 tasks
void taskButton(void * params);
void rtos_delay(uint32_t delay_in_ms);
// void IRAM_ATTR vInterruptHandler();   // 2 interrupts
// void IRAM_ATTR vInterruptHandler1();

/*Setup*/
void setup(){
    Serial.begin(115200); 

    pinMode(RELAY_PIN,OUTPUT);
    pinMode(SOILMOISTURE_PIN,INPUT);
    pinMode(AUTO_BUTTON, INPUT_PULLUP);
    pinMode(MANU_BUTTON, INPUT_PULLUP);
    pinMode(PUMP_BUTTON, INPUT_PULLUP);
    digitalWrite(RELAY_PIN,LOW); 
   
    xQueueTemp = xQueueCreate(1, sizeof(float));
    xQueueHumidity = xQueueCreate(1, sizeof(float));
    xQueueStateBTN = xQueueCreate(1, sizeof(int));
    //xQueueCheckMode = xQueueCreate(1, sizeof(int)); //check double mode error
    //---------------------
    vSemaphoreCreateBinary( xBinarySemaphore );
    xMutex = xSemaphoreCreateMutex();
    //--------------------------------
   // ESP32 has 526KB SRAM -> 526000 bytes
    xTaskCreatePinnedToCore(readSensor,"ReadSensor", 10000, NULL, 3,&xHandle3, ARDUINO_RUNNING_CORE);
    xTaskCreatePinnedToCore(taskButton, "Button",4096,NULL,2,NULL, ARDUINO_RUNNING_CORE);
    xTaskCreatePinnedToCore(taskChooseMode,"ChooseModeUsing", 10000, NULL, 3, NULL, ARDUINO_RUNNING_CORE);
}

void loop(){
    vTaskDelete(NULL);
}

//=================================READ SENSOR=====================================
void readSensor(void * params){
    float oldtemp = 0;
    float oldhumi = 0;
    float h;   
    float t;
    
    DHT dht(DHTPIN, DHTTYPE);
    dht.begin();
    xSemaphoreTake(xMutex, portMAX_DELAY);     //Lay Mutex
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {        
		Serial.println(F("SSD1306 allocation failed"));
		for(;;);
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);       
	
	
  for(;;){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
     float temphu = analogRead(SOILMOISTURE_PIN);   // doc gia tri do am cua cam bien dat
     h = map(temphu,0,4095,100,0);
     //h = dht.readHumidity();
     t = dht.readTemperature();   // doc gia tri nhiet do cua DHT 11
    
     if (isnan(t) || isnan(h)){           //Check DHT read data
      Serial.println("Faild to read from DHT11 sensor!");
      t= oldtemp;
      h= oldhumi;
     } else{
      oldtemp = t;
      oldhumi = h;
     }
    display.clearDisplay();   // xoa man hinh
    display.setTextColor(WHITE);	// cai dat mau chu 
		
		// display temperature
		display.setTextSize(1);						
		display.setCursor(0,0);						
		display.drawBitmap(10,0,temp,40,40,WHITE);
		display.setTextSize(1);					
		display.setCursor(10,52);					
		display.print(t);		
		display.print(" ");
		display.setTextSize(1);
		display.cp437(true);                       // Viet ki tu do C
		display.write(167);
		display.setTextSize(1.5);
		display.print("C");

    // display humidity
		display.setTextSize(1);					
		display.setCursor(64,0);						
		display.drawBitmap(74,0,humi,40,40,WHITE);
		display.setTextSize(1);					
		display.setCursor(74,52);					
		display.print(h);		
		display.print(" ");
		display.setTextSize(1);                  
		display.setTextSize(1.5);
		display.print("\%");

    xSemaphoreGive(xMutex);
    xQueueSendToBack(xQueueTemp,&t,0);
    xQueueSendToBack(xQueueHumidity,&h,0);
    //xQueueOverwrite(xQueueHumidity,&h);

    Serial.print(F("Nhiet do moi truong: "));
    Serial.println(t);
    Serial.print(F("Do am dat: "));
    Serial.println(h);

    display.display();
    display.clearDisplay();
    rtos_delay(1000);
    taskYIELD();
  }
}

//==============================Button Mode===================================
void taskButton(void * params){
   (void) params;
    xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
    display.clearDisplay();                      
    display.drawBitmap(0, 0, actvn_big_icon,128, 64,WHITE); //velogo nha truong
    display.display();

   xTaskCreatePinnedToCore(taskAutoMode,"AutoMode",12288,NULL,2,&xHandle1,ARDUINO_RUNNING_CORE);

  for (;;) {
    if(digitalRead(MANU_BUTTON) == 1 ) { 
    }else { 
      
      Serial.println("Nhân nút");
      if (BTN_STATE==0){
      while (digitalRead(MANU_BUTTON)==0)
      {
        BTN_STATE = 1;
        xQueueOverwrite(xQueueStateBTN,&BTN_STATE);
      }
      }
      
    }
    if(digitalRead(AUTO_BUTTON) == 1 ) { 
    }else { 
      
      Serial.println("Nhân nút 2");
      if (BTN_STATE==1){
      while (digitalRead(AUTO_BUTTON)==0)
      {
        BTN_STATE = 0;
        xQueueOverwrite(xQueueStateBTN,&BTN_STATE);
      }
      }
      
    }
    }
    xSemaphoreGive(xBinarySemaphore);
    rtos_delay(100);
}

//==============================TASK MODE==================================
void taskChooseMode(void * params){   
    
    static int temp ;
    
    for(;;){
        xQueueReceive(xQueueStateBTN,&temp,portMAX_DELAY);
       
            //btn_state = !btn_state;
          Serial.print(temp);     // thu lai khong dung ngat ma dung digitalRead();
          //Serial.print(check);
       // }else{
            if (temp ==0 ){
            vTaskDelete(xHandle2);
            // vTaskDelete(xHandle1);
            xTaskCreatePinnedToCore(taskAutoMode,"AutoMode",12288,NULL,2,&xHandle1,ARDUINO_RUNNING_CORE);
              
                Serial.println(F("AUTO"));
                
            }else if(temp ==1 ){
                vTaskDelete(xHandle1);
                xTaskCreatePinnedToCore(taskManualMode,"ManualMode",12288,NULL,2,&xHandle2,ARDUINO_RUNNING_CORE);
           
            Serial.println(F("MANUAL"));
                
       }   

        rtos_delay(200);
        taskYIELD();
    }  
}

void taskAutoMode(void * params){
     //xTaskCreatePinnedToCore(readSensor,"ReadSensor", 5000, NULL, 2,NULL, ARDUINO_RUNNING_CORE);
    vTaskResume(xHandle3);
    
    xSemaphoreTake(xMutex, portMAX_DELAY);
    float Buff;
    display.clearDisplay();
    display.setTextColor(WHITE);
    for(;;){
        xQueueReceive(xQueueHumidity,&Buff,portMAX_DELAY);
        Serial.println(F("Hello im auto"));
        if (Buff >=35 ){
    
    xTaskCreatePinnedToCore(taskPumpOff,"TurnOFF",4096,NULL,4,NULL, 0);
    //   digitalWrite(RELAY_PIN,LOW);
    }else{
   
       xTaskCreatePinnedToCore(taskPumpOn,"TurnON",4096,NULL,4,NULL,0);
    }
   // xSemaphoreGive(xBinarySemaphore);
     xSemaphoreGive(xMutex);
        rtos_delay(4000);
        taskYIELD();
        
    }
}

void taskManualMode(void * params){
     vTaskSuspend(xHandle3);
    // vTaskDelay(500);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1.8);
    display.setCursor(32,15);
    display.print("MANUAL MODE");
    display.display();
    int pumpState ;
    // vTaskDelay(500);
     vTaskDelay(1000);

    for(;;){
      
        int temp = digitalRead(PUMP_BUTTON);
        if (temp == 0){
            pumpState = !pumpState;
        }
         if (pumpState==0){
                xTaskCreatePinnedToCore(taskPumpOff,"TurnOFF",4096,NULL,4,NULL,0);
            }else{
                xTaskCreatePinnedToCore(taskPumpOn,"TurnON",4096,NULL,4,NULL,0);
            }
        
        rtos_delay(200);
        taskYIELD();
    } 
}

void taskPumpOn(void * params){
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(64,25);
    display.print("ON");
    display.drawBitmap(10, 12, pump,40, 40,WHITE); //velogo may bom
    display.display();
    digitalWrite(RELAY_PIN, HIGH);
    vTaskDelete(NULL);
}

void taskPumpOff(void * params){
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(64,25);
    display.print("OFF");
    display.drawBitmap(10, 12, pump,40, 40,WHITE); //velogo may bom
    display.display();
    digitalWrite(RELAY_PIN, LOW);
    vTaskDelete(NULL);
}

void rtos_delay(uint32_t delay_in_ms)
{
	uint32_t current_tick_count = xTaskGetTickCount();
	uint32_t delay_in_ticks = (delay_in_ms * configTICK_RATE_HZ ) /1000 ;
	while(xTaskGetTickCount() <  (current_tick_count + delay_in_ticks));
}
