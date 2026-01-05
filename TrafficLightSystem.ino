#include <LiquidCrystal.h>
#include <Servo.h>

const int carGreen = 8;
const int carYellow = 9; 
const int carRed   = 10;
const int pedGreen = 11, pedRed = 12;
const int crossButton = 13; 
const int ldrPin   = A0;
const int buzzerPin = A1;
const int trigPin  = A2;
const int echoPin  = A3;   
const int servoPin = A4;
const int limitSwitchPin = A5;

unsigned long greenTimeCars    = 5000; 
unsigned long yellowTimeCars   = 2000; 
unsigned long allRedIntergreen = 1000; 
unsigned long greenTimePed     = 4000; 
unsigned long redBufferCars    = 2000; 

// ultrasonic
const int trigger_cm = 40;   
const int release_cm = 50;  

bool pedRequest = false;
bool barrierIsUp = false;   
bool train = false; // for bluetooth

// for bluetooth string read from app
String cmd = "";

LiquidCrystal lcd(7, 6, 5, 4, 3, 2); 
Servo srv;

const int srv_stop  = 94;  
const int srv_down  = 104;  
const int srv_up    = 86;   
const int gate_time = 700;  
 

void setCars(bool r,bool y,bool g){
  digitalWrite(carRed, r);
  digitalWrite(carYellow,y);
  digitalWrite(carGreen, g);
}

void setPeds(bool r,bool g){
  digitalWrite(pedRed, r);
  digitalWrite(pedGreen, g);
}

bool isDark(){
  return (digitalRead(ldrPin) == HIGH);
}
void bluetooth() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\r' && c != '\n') {
      cmd += c;
    }
  }
  if (cmd.length() == 0) return;


  if (cmd == "p") {
    pedRequest = true;
    Serial.println("Pedestrian request");
  }
  else if (cmd == "t") {
     Serial.println("Train passing!");

    setCars(true, false, false);
    setPeds(true, false);
    barrierDown();
    tone(buzzerPin, 200);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("All RED. STOP!");
    lcd.setCursor(0,1);
    lcd.print("TRAIN COMMING");

    delay(3000);  

    barrierUp();
    noTone(buzzerPin);
  }
  else if (cmd == "s") {
    if (isDark()) {
        Serial.println("Night mode");
    } else {
        Serial.println("Day mode");
      }

    if (barrierIsUp) {
        Serial.println("Barrier UP");
    } else {
        Serial.println("Barrier DOWN");
      }

  }
  else if (cmd.startsWith("cg=")) {
    int val = cmd.substring(3).toInt();
    greenTimeCars = (unsigned long)val * 1000UL;
    Serial.print("Cars GREEN updated to ");
    Serial.print(val);
    Serial.println(" sec");
  }
  else if (cmd.startsWith("cy=")) {
    int val = cmd.substring(3).toInt();
    yellowTimeCars = (unsigned long)val * 1000UL;
    Serial.print("Cars YELLOW updated to ");
    Serial.print(val);
    Serial.println(" sec");
  }
  else if (cmd.startsWith("pg=")) {
    int val = cmd.substring(3).toInt();
    greenTimePed = (unsigned long)val * 1000UL;
    Serial.print("Ped GREEN updated to ");
    Serial.print(val);
    Serial.println(" sec");
  }
  else if (cmd.startsWith("ar=")) {
    int val = cmd.substring(3).toInt();
    redBufferCars = (unsigned long)val * 1000UL;
    Serial.print("ALL RED updated to ");
    Serial.print(val);
    Serial.println(" sec");
  }

  cmd = "";
}


void initialize() {
  lcd.clear();
  lcd.print("Calibrating...");
  srv.attach(servoPin);
  
  while (digitalRead(limitSwitchPin) == HIGH) {

    srv.write(90); 
  }

  srv.write(srv_stop);
  delay(100);
  srv.detach();

  barrierIsUp = true;
  lcd.clear();
  lcd.print("Barrier Open");
  delay(500);
}
void barrierDown() {
   if (!barrierIsUp) return; 
  srv.attach(servoPin);
  srv.write(srv_down);       
  delay(gate_time);                 
  srv.write(srv_stop);        
  delay(100);
  srv.detach();
  barrierIsUp = false;
}

void barrierUp() {
  if (barrierIsUp) return; 
  srv.attach(servoPin);
  srv.write(srv_up);          
  delay(gate_time);                   
  srv.write(srv_stop);        
  delay(100);
  srv.detach();
  barrierIsUp = true;
}

// ultrasonic sensor 
long readDistanceCm(){
  long duration, cm;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000UL); // 30ms timeout

  if (duration == 0) {
    return -1;
  }

  cm = duration / 29 / 2;
  return cm;
}

void handleUltrasonicAllRed(){
  setCars(true,false,false);
  setPeds(true,false);

  // train approaching -> close barrier
  barrierDown();
  tone(buzzerPin, 2000);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("All RED. STOP!");
  lcd.setCursor(0,1);
  lcd.print("TRAIN COMMING");

  // wait until train gone
  while (true) {

    bluetooth();
    long dist = readDistanceCm();
    if (dist == -1 || dist > release_cm) {
      break;
    }
    delay(50); 
  }

  // train gone -> open barrier
  barrierUp();
  noTone(buzzerPin);
}

void waitWithLCD(unsigned long ms, const char* phase, bool carR, bool carY, bool carG, bool pedR, bool pedG)
{
  unsigned long time = millis();
  unsigned long lastUpdate = 0;

  setCars(carR, carY, carG);
  setPeds(pedR, pedG);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(phase);

  while (millis() - time < ms) {

    bluetooth();  

    if (digitalRead(crossButton) == LOW) {
      pedRequest = true;
    }

    long dist = readDistanceCm();

    // only ultrasonic trigger now
    if (dist > 0 && dist <= trigger_cm) {
      handleUltrasonicAllRed();

      // resume this phase after the train
      setCars(carR, carY, carG);
      setPeds(pedR, pedG);

      time = millis();              // restart timer for this phase
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(phase);
    }

    // update countdown on LCD
    if (millis() - lastUpdate > 200) {
      long remaining = (long)(ms - (millis() - time)) / 1000;
      if (remaining < 0) remaining = 0;

      lcd.setCursor(0,1);
      lcd.print("Time: ");
      lcd.print(remaining);
      lcd.print(" s   ");
      lastUpdate = millis();
    }

    delay(1);
  }
}


// night mode 
void isNight(){

  setPeds(false,false);
  setCars(false,false,false);
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Night mode");
  lcd.setCursor(0,1);
  lcd.print("Yellow flashing");
  
  while (isDark()){

    bluetooth(); 

    long dist = readDistanceCm();
    if (dist > 0 && dist <= trigger_cm || train){
      handleUltrasonicAllRed();
      train = false;

      if (!isDark()) 
        return;
      
      // still night -> restore night display
      setPeds(false,false);
      setCars(false,false,false);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Night mode");
      lcd.setCursor(0,1);
      lcd.print("Yellow flashing");
    }

    // keep checking ultrasonic in small steps
    digitalWrite(carYellow,HIGH);
    for (int t = 0; t < 500 && isDark(); t += 50){
      delay(50);
      long d2 = readDistanceCm();
      if (d2 > 0 && d2 <= trigger_cm){
        handleUltrasonicAllRed();
        if (!isDark()) 
          return;

        setPeds(false,false);
        setCars(false,false,false);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Night mode");
        lcd.setCursor(0,1);
        lcd.print("Yellow flashing");
        break;  
      }
    }

    // flash yellow off, again checking ultrasonic
    digitalWrite(carYellow,LOW);
    for (int t = 0; t < 500 && isDark(); t += 50){
      delay(50);
      long d3 = readDistanceCm();
      if (d3 > 0 && d3 <= trigger_cm){
        handleUltrasonicAllRed();
        if (!isDark()) 
          return;

        setPeds(true,false);
        setCars(false,false,false);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Night mode");
        lcd.setCursor(0,1);
        lcd.print("Yellow flashing");
        break;  
      }
    }
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(carGreen, OUTPUT);
  pinMode(carYellow, OUTPUT);
  pinMode(carRed, OUTPUT);
  pinMode(pedGreen, OUTPUT);
  pinMode(pedRed, OUTPUT);
  pinMode(crossButton, INPUT_PULLUP);  
  pinMode(buzzerPin, OUTPUT);
  pinMode(ldrPin, INPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.begin(16,2);

  pinMode(limitSwitchPin, INPUT_PULLUP); 
  initialize(); // for servo

 
  lcd.clear();
  lcd.print("Traffic Ready");
  delay(1000);

  setCars(false,false,false);
  setPeds(true,false);
}

void loop() {

  if(isDark()){
    isNight();
  }

  waitWithLCD(greenTimeCars, "Cars: GREEN", false, false, true, true,  false);
  waitWithLCD(yellowTimeCars, "Cars: YELLOW", false, true,  false, true,  false);
  waitWithLCD(allRedIntergreen, "ALL RED", true,  false, false, true,  false);

  if (pedRequest) {
    tone(buzzerPin, 300);
    waitWithLCD(greenTimePed, "Ped: WALK", true,  false, false, false, true);

    noTone(buzzerPin);
    pedRequest = false;
    // all red for a little time
    waitWithLCD(allRedIntergreen, "ALL RED", true,  false, false, true,  false);

  } else {
    waitWithLCD(redBufferCars, "ALL RED", true,  false, false, true,  false);
  }
}
