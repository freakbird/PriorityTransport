#include <Wire.h>

#include "RFID.h"
#include "GM865.h"

#define N_GPS 5
#define N_RFID 1
int DATA_SENT=0;

// Data Structures
long latitude[5] = {1};
long longitude[5] = {2};
long time[5] = {3};

long idtime[1] = {12345};
int rfid[1] = {0x0};

// PN5324
#define IRQ   (2)
#define RESET (3)

Adafruit_NFCShield_I2C nfc(IRQ, RESET);

const int buzzerpin = 12;
#define BEEP_PULSE_SHORT 125
#define BEEP_PULSE_LONG 400
boolean validate = true;

// GM865
int onPin = 22;                      // pin to toggle the modem's on/off
char PIN[1] = "";                // replace this with your PIN
Position position;                   // stores the GPS position
GPRS modem(&Serial3, PIN);   // modem is connected to Serial3
char cmd;                            // command read from terminal
static long TIME_DELAY1 = 60000;     //Time Delay for GPRS thread
int DATA_LENGTH;

void setup(void) {
  Serial.begin(115200);


  // Setup - PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.println("Didn't find PN532");
    while (1);
  }  
  Serial.println("Starting PN5432");
  nfc.setPassiveActivationRetries(0xFF);  // Number of retry attempts 
  nfc.SAMConfig();                        // configure board to read RFID tags
  pinMode(buzzerpin, OUTPUT);
  Serial.println("Started PN5432");

  // Setup - GM865
  Serial.println("Starting GM865");
  modem.switchOn();                   // switch the modem on
  delay(2000);                        // wait for the modem to boot
  modem.init();                       // initialize the GM865
  while (!modem.isRegistered()) {
    delay(1000);
    modem.checkNetwork();             // check the network availability
  }
  Serial.println("Started GM865");
}

char* readterminal()
{
  char* check = (char*)malloc(sizeof(char)*10);
  int x=0;
  while(Serial3.available())
  {
    check[x] = Serial3.read();
    x++;
  }
  return check;
}

void gprs_perform() {
  int x=0,k,j;
  char buf[100];
  byte i = 0;
  String string = "";
  modem.initGPRS();                   // setup of GPRS context
  modem.enableGPRS();  				 // switch GPRS on
  Serial.println("Connected to Server");
  //Serial.read(buf, 0, 16);
  modem.openHTTP("www.utkarshsins.com");    // open a socket
  Serial.println("sending request ...");
  modem.send("POST /priority/arduino.php HTTP/1.1\r\n"); // search server for the corresponding request
  modem.send("HOST: www.utkarshsins.com\r\n");     // write on the socket
  modem.send("User-Agent: HTTPTool/1.1\r\n");
  modem.send("Content-Type: application/x-www-form-urlencoded\r\n");
  modem.send("Content-Length: ");
  
  // DATA_LENGTH = N_GPS*(26+6*2+5) + N_RFID*(7+5);
        
 // String string;
  for(k=0;k<N_GPS;k++) 
  {
    string += "latitude"+String(k+1, DEC)+"="+String(latitude[k], DEC)+"&longitude"+String(k+1, DEC)+"="+String(longitude[k], DEC)+"&time"+String(k+1, DEC)+"="+String(time[k], DEC);
  }

  for(j=0;j<N_RFID;j++) 
  {
    string += "idtime"+String(j+1, DEC)+"="+String(idtime[j], DEC)+"&id"+String(j+1, DEC)+"="+String(rfid[j], DEC);
  }
          
  {
    char* str;
    DATA_LENGTH = string.length();
    {
      char buff[string.length()+1];
      String lengthed = String(DATA_LENGTH, DEC) + "\r\n\r\n";
      lengthed.toCharArray(buff,lengthed.length()+1);
      start1:
      modem.send(buff);
      str = readterminal();
      if(str[0]=='O' && str[1]=='K')
      {
        DATA_SENT=1;
        x=0;
      }
      else if(str[0]=='N' && str[1]=='O' && x<10)
      {
        x++;
        goto start1;
      }
      else
      {
        setup();
      }
    }
    char buff[string.length()+1];
    string.toCharArray(buff,string.length()+1);
    start2:
    modem.send(buff);
    str  = readterminal();
    if(str[0]=='O' && str[1]=='K')
    {
        DATA_SENT=1;
    }
    else if(str[0]=='N' && str[1]=='O' && x<10)
      {
        x++;
        goto start2;
      }
    else
    {
      setup();
    }
  }
  modem.send("\r\n");
  
  
  Serial.println("receiving ...");
  while (i++ < 10) 
  {                  // try to read for 10s
    modem.receive(buf);               // read from the socket, timeout 1s
    
    if (strlen(buf) > 0) 
    {            // we received something
      Serial.print("Message from Server:"); Serial.println(buf);
      i--;                          // reset the timeout
    }
  }
}

static int rfid_thread() {
  boolean success;
  
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  
  Serial.println("Waiting...");
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  
  if (success) {
    Serial.println("Found a card!");
//Serial.read(buf, 0, serialPort.BytesToRead);
    Serial.print("UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("UID Value: ");
    for (uint8_t i=0; i < uidLength; i++) 
    {
      Serial.print(" 0x");Serial.print(uid[i], HEX); 
      rfid[0] = rfid[0] * 8 + uid[i];
    }
    Serial.println("");
  
    if(validate) 
    {
      validate = false;
      digitalWrite(buzzerpin, HIGH);
      delay(BEEP_PULSE_LONG);
    }
    else
    {
      validate = true;
      digitalWrite(buzzerpin, HIGH);
      delay(BEEP_PULSE_SHORT);
      digitalWrite(buzzerpin, LOW);
      delay(BEEP_PULSE_SHORT);
      digitalWrite(buzzerpin, HIGH);
      delay(BEEP_PULSE_SHORT);
    }

    digitalWrite(buzzerpin, LOW);
    
    // Wait 1 second before continuing
    delay(1000);
  }
  else
  {
    // PN532 probably timed out waiting for a card
    Serial.println("Timed out waiting for a card");
  }
  gprs_perform();
}
    

void loop(void) {
  rfid_thread();
}
