// https://forums.freertos.org/t/freertos-code-is-not-working/16038
#if  CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

/* Include Library */
#include <Arduino.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "logo"

/*Dinh nghia cac chan */
#define DHTPIN 19
#define DHTTYPE DHT11
#define RELAY_PIN 18

//Screen OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ----------SEMAPHORE----------------*/
SemaphoreHandle_t xBinarySemaphore;

/* -----------QUEUE-------------------*/
QueueHandle_t xQueueTemp, xQueueHumidity;

/*------------MUTEX-------------------*/
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

SemaphoreHandle_t xMutex;









void readSensor(void * pvParameters);
void taskTuoinuoc(void * pvParameters);
void TaskHienthi(void * pvParameters);
static void vHandlerTask( void *pvParameters );


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RELAY_PIN,OUTPUT);
  xQueueTemp = xQueueCreate(2, sizeof(float));
  xQueueHumidity = xQueueCreate(2, sizeof(float));
  //----------------------
  vSemaphoreCreateBinary( xBinarySemaphore );
	xMutex = xSemaphoreCreateMutex();
  //----------
  xTaskCreatePinnedToCore(readSensor,"ReadSensor", 10000, NULL, 3,NULL, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(taskTuoinuoc,"Tuoinuoc", 10000, NULL, 2,NULL, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(TaskHienthi,"Hienthi", 10000, NULL, 2, NULL, ARDUINO_RUNNING_CORE);
  
  if( xBinarySemaphore != NULL ){

		xTaskCreatePinnedToCore( vHandlerTask, "Handler", 1000, NULL, 1, NULL, 0); //Criar HandlerTask no Core
	}
}

void loop() {
  // put your main code here, to run repeatedly:
}

static void vHandlerTask( void * pvParameters ){
  for(;;){
    xSemaphoreTake(xBinarySemaphore,portMAX_DELAY);
    Serial.println("Tao lay Semaphore");
    xSemaphoreGive(xBinarySemaphore);
    vTaskDelay(4000/portTICK_PERIOD_MS);
  }
}

void readSensor(void * pvParameters){
    (void) pvParameters;
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    float oldtemp = 0;
    float oldhumi = 0;
    float h;
    float t;

    DHT dht(DHTPIN, DHTTYPE);
    dht.begin();
    xSemaphoreTake(xMutex, portMAX_DELAY);     //Lay Mutex
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {        //Detetar se está a ocorrer comunicação
		Serial.println(F("SSD1306 allocation failed"));
		for(;;);
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);       //Ocupar processador durante 1000ms
	display.clearDisplay();                      //Limpar Display
  display.drawBitmap(0, 0, actvn_big_icon,128, 64,WHITE);
  display.display();
	
  for(;;){
    vTaskDelay(1500 / portTICK_PERIOD_MS);
     h = dht.readHumidity();
     t = dht.readTemperature();
    
     if (isnan(t) || isnan(h)){           //Check DHT read data
      Serial.println("Faild to read from DHT11 sensor!");
      t= oldtemp;
      h= oldhumi;
     } else{
      oldtemp = t;
      oldhumi = h;
     }
    display.clearDisplay();
    display.setTextColor(WHITE);	
		//Lê Temperatura corretamente
		// display temperature
		display.setTextSize(1);						//Selecionar tamanho do texto
		display.setCursor(0,0);						//Selecionar onde imprime o texto no display
		display.print("Temperature: ");
		display.setTextSize(1);					//Selecionar tamanho do texto
		display.setCursor(0,10);					//Selecionar onde imprime o texto no display
		display.print(t);		//Mostrar Temperatura no display
		display.print(" ");
		display.setTextSize(1);
		display.cp437(true);                       //Desenhar "º" no display
		display.write(167);
		display.setTextSize(1.5);
		display.print("C");

    xSemaphoreGive(xMutex);
    xQueueSendToBack(xQueueTemp,&t,0);
    xQueueSendToBack(xQueueHumidity,&h,0);

    Serial.print(F("Nhiet do moi truong: "));
    Serial.println(t);
    Serial.print(F("Do am moi truong: "));
    Serial.println(h);

    display.display();
    vTaskDelayUntil(&xLastWakeTime, 1000/ portTICK_PERIOD_MS);

  }

}


void TaskHienthi(void *pvParameters){
  (void) pvParameters;
   float Buff;
  //  display.clearDisplay();
  //  display.drawBitmap(0,0,actvn_big_icon,128,64,WHITE);
  for(;;){
    xQueueReceive(xQueueHumidity,&Buff,portMAX_DELAY);
    
   
     //lay du lieu tu QUeue
    Serial.println(Buff);
    if (Buff >=50){
      Serial.println(F("Do am binh thuong"));
      digitalWrite(RELAY_PIN,HIGH);
    }else{
      Serial.println(F("Dat bi kho!!"));
    }
    vTaskDelay(1000);
  }
}

void taskTuoinuoc(void *pvParameters){
        (void) pvParameters;
  for(;;){
    Serial.println(F("Dang tuoi nuoc"));
    // digitalWrite(RELAY_PIN,LOW);
    vTaskDelay(1000);
  }
}

