////////////////////////////  Includes  ////////////////////////////
  #include "SSD1306Ascii.h"
  #include "SSD1306AsciiAvrI2c.h"
  
  #include <XBee.h>
  #include<SoftwareSerial.h>
  #include "ClosedCube_HDC1080.h"
////////////////////////////  Declaration for an SSD1306 display connected to I2C (SDA, SCL pins) //////////////////////////// 
  // The pins for I2C are defined by the Wire-library. 
  // On an arduino UNO:       A4(SDA), A5(SCL)
  #define I2C_ADDRESS 0x3C
  #define RST_PIN -1
  SSD1306AsciiAvrI2c oled;
  ClosedCube_HDC1080 hdc1080;

////////////////////////////  Declaration of Varibles ////////////////////////////
  char payload[92],RD[92];
  uint8_t Light=425,CO2=600,Ocu=1;
  double RH=0,Temp=0;
  uint8_t DDDD,rows,RECI=0,col[2]; // Used IN RRR to store data

  ////////////////////////////  For lIGHT sENSOR ////////////////////////////
  #define light_sensor A2

  ////////////////////////////  For motion sensor ////////////////////////////
  int state = LOW;             // by default, no motion detected
  int val = 0; 
  int ocu_counter =0;
  ////////////////////////////  FOR TIMER INTERUPT  ///////////////////////
  int counter_to_send = 0;
  int counter_to_clear_display = 0;
  const int TIMEPERIODTOSEND = 7; //in seconds
  const int TIMEPERIODTOREC = 15; 


////////////////////////////  create the XBee object  ////////////////////////////
  XBee xbee = XBee();
  SoftwareSerial Serial1(8,9); // Rx=8, Tx=9 // To use on Xbee // Dout from Xbee to Rx Arduino // Din from Xbee to Tx Arduino
  // SH + SL Address of receiving XBee 
  // 0x0013A200 , 0x4191ACC6 // Left
  // 0x00000000 , 0x0000FFFF // Broadcast
  // 0x00000000 , 00000000   // coordinator
  // 0x0013A200 , 0x4191AC65 // Right

  XBeeAddress64 addr64 = XBeeAddress64(0x00000000,0x00000000);
  ZBTxRequest zbTx;
  ZBTxStatusResponse txStatus;
  ZBRxResponse rx = ZBRxResponse();
  ModemStatusResponse msr = ModemStatusResponse();
////////////////////////////  PINS  ////////////////////////////
  // Rx=8, Tx=9 // To use on Xbee 
  bool doneSending =false;
  int pin5 = 0;
  const int buttonPin = 11;  // the number of the pushbutton pin
  int buttonState = 0;  // variable for reading the pushbutton status

  int Led2 = 2;
  int Led3 = 3;
  int Led4 = 4;
  int Led5 = 5;
  int Led6 = 6;
  int Led7 = 7;
  int motion_sensor = 13; 

void setup() {
  hdc1080.begin(0x40);

  //////////////////////////// Set Pins as dirctions ( output or input) Led2 - Led7 & motion sensor ////////////////////////////
    pinMode(Led2, OUTPUT);
    pinMode(Led3, OUTPUT);
    pinMode(Led4, OUTPUT);
    pinMode(Led5, OUTPUT);
    pinMode(Led6, OUTPUT);
    pinMode(Led7, OUTPUT);
    pinMode(buttonPin, INPUT);
    pinMode(motion_sensor, INPUT);
    


  // Initialize Baud rate
    Serial.begin(9600);
    Serial1.begin(9600);
    xbee.setSerial(Serial1);

  

  // Oled  

    #if RST_PIN >= 0
      oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
    #else // RST_PIN >= 0
      oled.begin(&Adafruit128x64, I2C_ADDRESS);
    #endif // RST_PIN >= 0
    
    oled.setFont(System5x7);
    rows = oled.fontRows();
    oled.clear();
    

  

  //////////////////   TIMER   ////////////////////
    //set timer1 interrupt at 1Hz
    cli();//stop interrupts
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    // set compare match register for 1hz increments
    OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
    // turn on CTC mode
    TCCR1B |= (1 << WGM12);
    
    TCCR1B |= (1 << CS12) | (1 << CS10);  
    // enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);

    sei();//allow interrupts
}

void loop() {
  int light_sensor_reading = analogRead(light_sensor);
  light_sensor_reading = map(light_sensor_reading, 0, 1024, 300, 1100);
  
  if (digitalRead(motion_sensor) == HIGH){
    if (state == LOW) {
      state = HIGH;
      ocu_counter++;
    }
  }
  else{
    if (state == HIGH){
        state = LOW; 
    }
  }

  RH = hdc1080.readHumidity();
  Temp = hdc1080.readTemperature();
  Light = light_sensor_reading ;
  CO2 = random(300)+1;
  Ocu = ocu_counter ;
  

  buttonState = digitalRead(buttonPin);

  ////////////////////////////////////To send and print the transmitted data////////////////////////
  RRR();
  if (counter_to_send >= TIMEPERIODTOSEND){
  SSS();
  
  counter_to_send = 0;
  }
  if (counter_to_clear_display >= TIMEPERIODTOREC){
    clearRecivedFieldOnDisplay();
    turnOffAllLeds();
    counter_to_clear_display = 0 ;
  }
  delay(1000);

}

void SSS (){
  sprintf(payload,"T:%s,RH:%s,L:%d,Co2:%d,Oc:%d-",String(Temp, 2).c_str(),String(RH, 2).c_str(),Light,CO2,Ocu);
  
  zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
  
  txStatus = ZBTxStatusResponse();
  ocu_counter=0;
  
  xbee.send(zbTx);

  

  if (xbee.readPacket(500)) {
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      if (txStatus.getDeliveryStatus() == SUCCESS) {

        doneSending=true;        
        
      } else {
        doneSending=false;
      }
    }
  } else if (xbee.getResponse().isError()) {
    
  } else {
  
  }

  DIS();

}

void RRR(){
  

  xbee.readPacket();
    
    if (xbee.getResponse().isAvailable()) {
    
      
      if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
        
        xbee.getResponse().getZBRxResponse(rx);
            
        if (rx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
        
        } else {
        
        }
        
        for (int i=0; i <= rx.getDataLength()-1; i++) {
        
        RD[i]=rx.getData(i);
        
        }
        
        turnOnLeds(RD);

      } else if (xbee.getResponse().getApiId() == MODEM_STATUS_RESPONSE) {
        xbee.getResponse().getModemStatusResponse(msr);
        
        if (msr.getStatus() == ASSOCIATED) {
        
        } else if (msr.getStatus() == DISASSOCIATED) {
          
        } else {
          
        }
      } else {
      	
      }
    } else if (xbee.getResponse().isError()) {
    
    }

    DIS();
}

//////////////////////////////////////  At the End we clear Recived buffer. So,it wouldn't appaer on screen. //////////////////////////////////////
void DIS(){
  int CCOO=0;
  oled.println(" Send: "); 
  oled.print(" ");
  int i = 15;
  payload[i] = '\0';
  payload[strstr("-", payload)-payload] = '\0';
  oled.print(&(payload[0]));
  oled.clearToEOL();
  oled.println("");
  oled.print("  ");

  oled.print(&(payload[i+1]));
  oled.clearToEOL();
  oled.println("");
  
  oled.println("\n  Recived: ");
  oled.print(" Counter ");
  oled.print(RECI);
  oled.print("   , ");
  
  oled.print(RD);
  oled.clearToEOL();
  oled.println(); /////////////////// Needed if a new line to be printed ///////////////////

  
  oled.print("Sending back: "); 
  if (doneSending){
    oled.print("SUCCESS");
    }
    else{
      oled.print("Fail");
      }
    


  oled.setCursor(0,0);

  
  RECI++;

  memset(RD,'\0', sizeof(RD));

}

void turnOnLeds(char RR[]){
  int X=RR[0]-'0';
  
  switch(X){
          
          case 2:
          digitalWrite(Led2,HIGH);
          break;
          case 3:
          digitalWrite(Led3,HIGH);
          break;
          case 4:
          digitalWrite(Led4,HIGH);
          break;
          case 5:
          digitalWrite(Led5,HIGH);
          break;
          case 6:
          digitalWrite(Led6,HIGH);
          break;
          case 7:
          digitalWrite(Led7,HIGH);
          break;

          
        }
}
void turnOffAllLeds(){

  digitalWrite(Led2,LOW);
  digitalWrite(Led3,LOW);
  digitalWrite(Led4,LOW);
  digitalWrite(Led5,LOW);
  digitalWrite(Led6,LOW);
  digitalWrite(Led7,LOW);
}

void clearRecivedFieldOnDisplay(){

  col[0] = oled.fieldWidth(0); // 
  col[1] = oled.fieldWidth(String(RECI).length()+String("        ").length());
  oled.setCursor(col[1], rows*5);
  oled.clearToEOL();
  oled.setCursor(0, 0);
  
  memset(RD,'\0', sizeof(RD));
  
  
}


ISR(TIMER1_COMPA_vect){
  
  counter_to_send = counter_to_send + 1;
  counter_to_clear_display = counter_to_clear_display + 1;
}
