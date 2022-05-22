#include "arduino_secrets.h"
/* 
  Sketch for SmartMailbox
  https://create.arduino.cc/cloud/things/8d5e5892-14ef-4d1b-a412-8b9445602a07 

  Arduino IoT Cloud Variables description

  The following variables are automatically generated and updated when changes are made to the Thing

  bool doorState;
  bool lockCommand;

  Variables which are marked as READ/WRITE in the Cloud Thing will also have functions
  which are called when their values are changed from the Dashboard.
  These functions are generated with the Thing and added at the end of this sketch.
*/
#include <arduino-timer.h>
#include "thingProperties.h"
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>

int buzzer = 3; //connecting buzzer to pin 7

#define SW_PIN          4

#define RST_PIN         5           // Configurable, see typical pin layout above
#define SS_PIN          18          // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

#include <Servo.h>
Servo myservo;  // create servo object to control a servo
int status = WL_IDLE_STATUS;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient client;
char HOST_NAME[] = "maker.ifttt.com";
String PATH_NAME = "/trigger/..."; // change your EVENT-NAME and YOUR-KEY
String PATH_NAME_ALARM= "/trigger/..."; // change your EVENT-NAME and YOUR-KEY
String queryString = "?value1=57&value2=25";

int sw_state = 4;
int servo_state = 0;
int notice_state = 0;
int rfid_state;
  //Resident ID
  byte CID[]    = {
    0x4b, 0xb9, 0x56, 0x73,
  };
  //Mail Carrier ID
  byte CID2[]    = {
    0x5d, 0xbe, 0x55, 0x73,
  };

  byte buffer[8];
  byte size = sizeof(buffer);

auto timer = timer_create_default();

void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(500); 

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  /*
     The following function allows you to obtain more information
     related to the state of network and IoT Cloud connection and errors
     the higher number the more granular information you’ll get.
     The default is 0 (only errors).
     Maximum is 4
 */
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
  
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  turn_servo(0);
  lockCommand = true;
  
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card
  
  pinMode(SW_PIN, INPUT_PULLUP);
  
  WiFi.begin(ssid, pass);
  
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card
  auto timer = timer_create_default();
  
}

void loop() {
  timer.tick();
  ArduinoCloud.update();
  // Your code here 
  rfid_state = checkRFID();
  if(rfid_state == 4 || rfid_state == 5){
    lockCommand = false;
  }
  delay(20);
  update_lockCommand();
  delay(20);
  updateDoorState(rfid_state);
  
}

void updateDoorState(int state){
  sw_state = digitalRead(SW_PIN);
  delay(50);
  if(sw_state == LOW){
    doorState = true;
    if(notice_state < 1){
      notice_state++;
      if(lockCommand){
        sendEmailAlarm();
        beepAlarm();
      }
    }
    //auto task = timer.in(20000, beepAlarm());
    
  } else{
    doorState = false;
    timer.cancel();
    if(notice_state > 0){
      lockCommand = true;
    }
    notice_state = 0;
  }
  
  delay(100);
}

int checkRFID(){
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return 1;

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return 2;

  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  
  int result = 0;
  if(matchResident(mfrc522.uid.uidByte, mfrc522.uid.size)){
    result = 4;
  } else if (matchMC(mfrc522.uid.uidByte, mfrc522.uid.size)){
    result = 5;
        int sent_email = sendEmailNotice();
        if(sent_email){
          Serial.println("email notice command sent successfully");
        } else{
        Serial.println("email notice command unable to reach server");
        }
  } else {
    result = 3;
  }
  Serial.println("RFID status: " + String(result));

  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
  return result;
}

/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

bool matchResident(byte *buffer, byte bufferSize){
  Serial.println(F("Checking result..."));
  byte count = 0;
  for (byte i = 0; i < bufferSize; i++) {
    // Compare buffer (= what we've read) with dataBlock (= what we've written)
    if (buffer[i] == CID[i])
      count++;
  }
  Serial.print(F("Number of bytes that match = ")); Serial.println(count);
  if (count == bufferSize) {
    Serial.println(F("Success :-)"));
    return true;

  } else {
    Serial.println(F("Failure, no match :-("));
    return false;
  }
}

bool matchMC(byte *buffer, byte bufferSize){
  Serial.println(F("Checking result..."));
  byte count = 0;
  for (byte i = 0; i < bufferSize; i++) {
    // Compare buffer (= what we've read) with dataBlock (= what we've written)
    if (buffer[i] == CID2[i])
      count++;
  }
  Serial.print(F("Number of bytes that match = ")); Serial.println(count);
  if (count == bufferSize) {
    Serial.println(F("Success :-)"));
    return true;

  } else {
    Serial.println(F("Failure, no match :-("));
    return false;
  }
}


void turn_servo(int angle) {
  myservo.write(angle);              // tell servo to go to position in variable 'pos'
  // Serial.print("servo angle:");
  // Serial.println(angle);
  delay(100);

}

void update_lockCommand(){
  if(lockCommand){
    //lock action
    servo_state = 0;
    turn_servo(0);
    //Serial.println("lock");
  } else{
    //unlock action
    servo_state = 1;
    turn_servo(90);
    //Serial.println("unlock");
  }
}

int sendEmailNotice(){
    // connect to web server on port 80:
  if (client.connect(HOST_NAME, 80)) {
    // if connected:
    Serial.println("Connected to server");
  }
  else {// if not connected:
    Serial.println("connection failed");
    return 0;
  }
        // make a HTTP request:
        // send HTTP header
        client.println("GET " + PATH_NAME + queryString + " HTTP/1.1");
        client.println("Host: " + String(HOST_NAME));
        client.println("Connection: close");
        client.println(); // end HTTP header

        while (client.connected())
        {
            if (client.available())
            {
                // read an incoming byte from the server and print it to serial monitor:
                char c = client.read();
                Serial.print(c);
            }
        }

        // the server's disconnected, stop the client:
        client.stop();
        Serial.println();
        Serial.println("disconnected");
    return 1;
  
}

int sendEmailAlarm(){
    // connect to web server on port 80:
  if (client.connect(HOST_NAME, 80)) {
    // if connected:
    Serial.println("Connected to server");
  }
  else {// if not connected:
    Serial.println("connection failed");
    return 0;
  }
        // make a HTTP request:
        // send HTTP header
        client.println("GET " + PATH_NAME_ALARM + queryString + " HTTP/1.1");
        client.println("Host: " + String(HOST_NAME));
        client.println("Connection: close");
        client.println(); // end HTTP header

        while (client.connected())
        {
            if (client.available())
            {
                // read an incoming byte from the server and print it to serial monitor:
                char c = client.read();
                Serial.print(c);
            }
        }

        // the server's disconnected, stop the client:
        client.stop();
        Serial.println();
        Serial.println("disconnected");
    return 1;
  
}

void beepAlarm(){
  tone(buzzer,1000,1000); //freq 1000 Hz,delay 1 sec
  tone(buzzer,1200,1000); //freq 1200 Hz,delay 1 sec
  delay(50);
}

//Placeholder function for Arduino IoT Cloud variable update, not used
/*
  Since LockCommand is READ_WRITE variable, onLockCommandChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onLockCommandChange()  {
  // Add your code here to act upon LockCommand change

}








