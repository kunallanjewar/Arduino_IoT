//Program: Motion Sensor & LED lights
//Programmer: Kunal Lanjewar
#include <MySensor.h>  
#include <SPI.h>

unsigned long SLEEP_TIME = 120000; // Sleep time between reports (in milliseconds)
#define DIGITAL_INPUT_SENSOR 3   // The digital input you attached your motion sensor.  (Only 2 and 3 generates interrupt!)
#define INTERRUPT DIGITAL_INPUT_SENSOR-2 // Usually the interrupt = pin -2 (on uno/nano anyway)
#define CHILD_ID 1   // Id of the sensor child
#define LED_1 4  // LED connected to Digital PIN 4 on Arduino
#define NUMBER_OF_LEDS 1 //Total number of LEDs attached to Arduino
#define LED_ON 1  // GPIO value to write to turn ON attached relay
#define LED_OFF 0 // GPIO value to write to turn OFF attached relay

MySensor gw;
// Initialize motion message
MyMessage msg(CHILD_ID, V_TRIPPED);

void setup()  
{  
  gw.begin(incomingMessage, AUTO, true);  // Initialize library and add callback for incoming messages
  
   // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("LED", "1.0");

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("Motion Sensor", "1.0");
  
   // Fetch LED status
  for (int sensor=1, pin=LED_1; sensor<=NUMBER_OF_LEDS;sensor++, pin++) 
  {
    // Register all sensors to gw (they will be created as child devices)
    gw.present(sensor, S_LIGHT);
    // Then set LED pins in output mode
    pinMode(pin, OUTPUT);   
    // Set relay to last known state (using eeprom storage) 
    digitalWrite(pin, gw.loadState(sensor)?LED_ON:LED_OFF);
  }

  pinMode(DIGITAL_INPUT_SENSOR, INPUT);      // sets the motion sensor digital pin as input
  // Register all sensors to gw (they will be created as child devices)
  gw.present(CHILD_ID, S_MOTION);
  
  
  
}

void loop()     
{
  gw.process();  
  // Read digital motion value
  boolean tripped = digitalRead(DIGITAL_INPUT_SENSOR) == HIGH; 
        
  Serial.println(tripped);
  gw.send(msg.set(tripped?"Motion Detected":"No Motion Detected"));  // Send tripped value to gw 
 
  // Sleep until interrupt comes in on motion sensor. Send update every two minute. 
  gw.sleep(INTERRUPT,CHANGE, SLEEP_TIME);
}

void incomingMessage(const MyMessage &message) 
{
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type==V_LIGHT) 
  {
     // Change LED state
     digitalWrite(message.sensor-1+LED_1, message.getBool()?LED_ON:LED_OFF);
     // Store state in eeprom
     gw.saveState(message.sensor, message.getBool());
     // Write some debug info
     Serial.print("Incoming change for sensor:");
     Serial.print(message.sensor);
     Serial.print(", New status: ");
     Serial.println(message.getBool());
   } 
}


