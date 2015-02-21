// ethernet starts off from the DhcpAddressPrinter example   
// thingspeak starts off from http://community.thingspeak.com/arduino/ThingSpeakClient.pde

#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>

byte mac[] = {  
  0x90, 0xA2, 0xDA, 0x0D, 0x2A, 0x4A };

byte buf[32]; // ring buffer
unsigned int bufptr = 0; // pointer into ring buffer
unsigned int bufblk = 0; // boolean to block buffer updates

// ThingSpeak Settings
IPAddress server(184, 106, 153, 149 );          // IP Address for the ThingSpeak API
const String writeAPIKey1 = "XXXXXXXXXXXXXXXX"; // Write API Key for a ThingSpeak Channel
const String writeAPIKey2 = "XXXXXXXXXXXXXXXX"; // Write API Key for a ThingSpeak Channel
const String writeAPIKey3 = "XXXXXXXXXXXXXXXX"; // Write API Key for a ThingSpeak Channel
const int updateInterval = 30000;               // Time interval in milliseconds to update ThingSpeak, see https://thingspeak.com/docs/channels#rate_limits
EthernetClient client;

typedef struct message_t {
  unsigned long id;
  unsigned long counter;
  float value1;
  float value2;
  float value3;
  float value4;
  float value5;
  unsigned long crc;
};

// Variable Setup
unsigned long lastConnectionTime = 0; 
boolean lastConnectionStatus = false;
unsigned int lastChannel = 0;
unsigned int resetCounter = 0;
String postString1, postString2, postString3;

/*******************************************************************************************************************/

void setup() {
  Serial.begin(57600);
  Serial.println(F("\n[rfm12b_thingspeak / " __DATE__ " / " __TIME__ "]"));

  // start the Ethernet connection:
  while (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    delay(5000);
  }
  // print your local IP address:
  Serial.print(F("My IP address: "));
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();

  for (int i; i<32; i++) {
    buf[i] = 0;
  }

  Wire.begin(9);                // Start I2C Bus as a Slave (Device Number 9)
  Wire.onReceive(receiveEvent); // register event

} // setup

/*******************************************************************************************************************/

void loop() {

  // Print Update Response to Serial Monitor
  if (client.available())
  {
    char c = client.read();
    Serial.print(c);
  }

  // Disconnect from ThingSpeak
  if (!client.connected() && lastConnectionStatus)
  {
    Serial.println();

    client.stop();
  }

  if (bufblk) {
    // the buffer is full and needs to be processed
    message_t *message = (message_t *)buf;

    unsigned long check = crc_buf((char *)message, sizeof(message_t) - sizeof(unsigned long));

    if (message->crc==check) {

      Serial.print(message->id);
      Serial.print(",\t");
      Serial.print(message->counter);
      Serial.print(",\t");
      Serial.print(message->value1, 2);
      Serial.print(",\t");
      Serial.print(message->value2, 2);
      Serial.print(",\t");
      Serial.print(message->value3, 2);
      Serial.print(",\t");
      Serial.print(message->value4, 2);
      Serial.print(",\t");
      Serial.print(message->value5, 2);
      Serial.print(",\t");
      Serial.println(message->crc);

      // Update ThingSpeak
      if (!client.connected()) {
        switch (message->id) {
        case 2:
          // LM35: V, T
          postString1 += "&field1="+String(message->value1)+"&field2="+String(message->value2);
          break;
        case 3:
          // AM2301: V, T, H
          postString1 += "&field3="+String(message->value1)+"&field4="+String(message->value2)+"&field5="+String(message->value3);
          break;
        case 4:
          // BMP085: V, T, P
          // FIXME field 7 and 8 are swapped in the ThingSpeak channel
          postString1 += "&field6="+String(message->value1)+"&field8="+String(message->value2)+"&field7="+String(message->value3);
          break;
        case 5:
          // 2x CNY50: V, KWh, M3
          postString2 += "&field1="+String(message->value1)+"&field2="+String(message->value2)+"&field3="+String(message->value3);
          break;
        case 6:
          // 2x DS18B20: V, T, T
          postString2 += "&field4="+String(message->value1)+"&field5="+String(message->value2)+"&field6="+String(message->value3);
          break;
        case 7:
          // BMP180 with ESP8266: V, T, P
          postString3 += "&field1="+String(message->value1)+"&field2="+String(message->value2)+"&field3="+String(message->value3);
          break;
        } // switch

      }
      bufblk = 0; // release buffer
    }
    else {
      Serial.println(F("CRC mismatch"));
    }
  }

  // Update ThingSpeak
  if (!client.connected() && (millis() - lastConnectionTime > updateInterval)) {

    if (lastChannel==1) {
      if (postString2.length()>0) {
        updateThingSpeak(postString2, writeAPIKey2);
        postString2 = "";
        lastChannel = 2;
      }
      else if (postString3.length()>0) {
        updateThingSpeak(postString3, writeAPIKey3);
        postString3 = "";
        lastChannel = 3;
      }
      else if (postString1.length()>0) {
        updateThingSpeak(postString1, writeAPIKey1);
        postString1 = "";
        lastChannel = 1;    
      }
    }
    else if (lastChannel==2) {
      if (postString3.length()>0) {
        updateThingSpeak(postString3, writeAPIKey3);
        postString3 = "";
        lastChannel = 3;
      }
      else if (postString1.length()>0) {
        updateThingSpeak(postString1, writeAPIKey1);
        postString1 = "";
        lastChannel = 1;    
      }
      else if (postString2.length()>0) {
        updateThingSpeak(postString2, writeAPIKey2);
        postString2 = "";
        lastChannel = 2;
      }
    }
    else if (lastChannel==3) {
      if (postString1.length()>0) {
        updateThingSpeak(postString1, writeAPIKey1);
        postString1 = "";
        lastChannel = 1;    
      }
      else if (postString2.length()>0) {
        updateThingSpeak(postString2, writeAPIKey2);
        postString2 = "";
        lastChannel = 2;
      }
      else if (postString3.length()>0) {
        updateThingSpeak(postString3, writeAPIKey3);
        postString3 = "";
        lastChannel = 3;
      }
    }
    else {
      if (postString1.length()>0) {
        updateThingSpeak(postString1, writeAPIKey1);
        postString1 = "";
        lastChannel = 1;    
      }
      else if (postString2.length()>0) {
        updateThingSpeak(postString2, writeAPIKey2);
        postString2 = "";
        lastChannel = 2;
      }
      else if (postString3.length()>0) {
        updateThingSpeak(postString3, writeAPIKey3);
        postString3 = "";
        lastChannel = 3;
      }
    }
    lastConnectionTime = millis();
  }

  lastConnectionStatus = client.connected();
} //loop

/*******************************************************************************************************************/

void updateThingSpeak(const String tsData, const String writeAPIKey)
{
  if (client.connect(server, 80))
  { 
    Serial.println(F("Connected to ThingSpeak..."));
    Serial.println();
    
    client.print(F("POST /update HTTP/1.1\n"));
    client.print(F("Host: api.thingspeak.com\n"));
    client.print(F("Connection: close\n"));
    client.print(F("X-THINGSPEAKAPIKEY: "));
    client.print(writeAPIKey);
    client.print(F("\n"));
    client.print(F("Content-Type: application/x-www-form-urlencoded\n"));
    client.print(F("Content-Length: "));
    client.print(tsData.length());
    client.print(F("\n\n"));
    client.print(tsData);

    resetCounter = 0;
  }
  else
  {
    Serial.println(F("Connection Failed."));   
    Serial.println();

    resetCounter++;

    if (resetCounter >=5 ) {
      resetEthernetShield();
    }
  }
} // updateThingSpeak

void resetEthernetShield()
{
  Serial.println(F("Resetting Ethernet Shield."));   
  Serial.println();

  client.stop();
  delay(1000);

  while (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    delay(5000);
  }
}

void receiveEvent(int howMany) {
  while (howMany-->0) {
    int x = Wire.read();    // receive byte as an integer
    if (!bufblk)
      buf[bufptr++] = x; // insert into buffer

    if ((bufptr % 32)==0) {
      bufptr = 0; // point to the first element
      bufblk = 1; // block buffer
    }
  }

} // receiveEvent

/*******************************************************************************************************************/

static PROGMEM prog_uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

unsigned long crc_update(unsigned long crc, byte data)
{
  byte tbl_idx;
  tbl_idx = crc ^ (data >> (0 * 4));
  crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
  tbl_idx = crc ^ (data >> (1 * 4));
  crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
  return crc;
}

unsigned long crc_buf(char *b, long l)
{
  unsigned long crc = ~0L;
  for (unsigned long i=0; i<l; i++)
    crc = crc_update(crc, ((char *)b)[i]);
  crc = ~crc;
  return crc;
}

unsigned long crc_string(char *s)
{
  unsigned long crc = ~0L;
  while (*s)
    crc = crc_update(crc, *s++);
  crc = ~crc;
  return crc;
}

