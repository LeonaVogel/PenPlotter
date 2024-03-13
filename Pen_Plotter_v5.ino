#include<Servo.h>
#include<AccelStepper.h>
#include<MultiStepper.h>
#include<SPI.h>
#include<SD.h>

   // Festlegung der Pin-Belegung als Konstanten
   #define PIN_Y_UNTEN 0      //Endlagentaster unten
   #define PIN_Y_OBEN 1       //Endlagentaster oben
   #define PIN_X_RECHTS 2     //Endlagentaster rechts
   #define PIN_X_LINKS 3      //Endlagentaster links
   #define PIN_SERVO 4        //Servo
   #define PIN_STEPX_STP 6    //Schrittmotor X-Richtung Ansteuerung
   #define PIN_STEPX_DIR 7    //Schrittmotor X-Richtung Richtung
   #define PIN_STEPY_DIR 8    //Schrittmotor Y-Richtung Richtung
   #define PIN_STEPY_STP 9    //Schrittmotor Y-Richtung Ansteuerung
   #define PIN_BTN_HOCH 10    //Steuerkreuz A nach oben
   #define PIN_BTN_RECHTS 11  //Steuerkreuz B nach rechts
   #define PIN_BTN_RUNTER 12  //Steuerkreuz C nach unten
   #define PIN_BTN_LINKS 13   //Steuerkreuz D nach links
   #define PIN_CS 17          //SD-Karte
   #define PIN_BTN_STIFT 20   //Taster Stift hoch/runter
   #define PIN_BTN_MODUS 21   //Taster Modus SD-Modus/manuell
   #define PIN_VRX 26         //Joystick Y-Richtung
   #define PIN_VRY 27         //Joystick X-Richtung

   // Festlegung der verwendeten Pulsweiten für oben und unten des Servos
   #define OBEN 1000
   #define UNTEN 1500

   // Anlegen der Mitglieder einer Klasser für den Servo und den Schrittmotoren
   Servo myServo;
   AccelStepper StepperX = AccelStepper(1, PIN_STEPX_STP, 14);
   AccelStepper StepperY = AccelStepper(1, PIN_STEPY_STP, 15);

   //Anlegen der globalen Variablen
   long velocityX = 0;      //Geschwindigkeiten des Motors beim Joystick
   long velocityY = 0;      //Geschwindigkeiten des Motors beim Joystick

   //globale Variablen zum Lesen von der SD-Karte
   File myFile;                       //Datei auf SD-Karte
   String filename = "testfile.txt";  //Name der Datei auf der SD-Karte
   char tempchar;                     //von SD-Karte eingelesener Character
   String input = "";                 //von SD-Karte eingelesene Zeile
   bool stopRead = false;             //Stoppen des Lesens einer Datei, wenn Ende der Zeile erreicht wird

   //globale Variablen zum Steuern des Motors im SD-Modus
   long positionX;    //Position des Motors für die X-Richtung
   long positionY;    //Position des Motors für die Y-Richtung
   String x = "";     //von SD-Karte eingelesener X-Wert
   String y = "";     //von SD-Karte eingelesener Y-Wert
   int StepsX;        //X-Wert in Schritten umgerechnet
   int StepsY;        //Y-Wert in Schritten umgerechnet
   bool merkeX = false;   //Variable erlaubt Aufaddieren des X-Wert-String
   bool merkeY = false;   //Variable erlaubt Aufaddieren des Y-Wert-String

   // globale Variablen die zur Nutzung der Taster als Schalter benötigt werden
   bool schalter_stift = LOW;
   bool schalter_modus = LOW;
   bool merker_stift = LOW;
   bool merker_modus = LOW;
   unsigned long lasttime = 0;
   bool sw_sd_modus;         //Programm wird von der SD-Karte gelesen, wenn true
   bool sw_stift_oben;       //Steuert Servo mit Stift hoch/runter im manuellen Modus
    
void setup(){
   Serial.begin(9600);
   while(!Serial){}      //wartet bis Serial-Monitor geöffnet ist

   //Festlegung der Pin-Modes  
   pinMode(PIN_Y_UNTEN, INPUT_PULLUP);
   pinMode(PIN_Y_OBEN, INPUT_PULLUP);
   pinMode(PIN_X_LINKS, INPUT_PULLUP);
   pinMode(PIN_X_RECHTS, INPUT_PULLUP);  
   pinMode(PIN_STEPX_DIR, OUTPUT);
   pinMode(PIN_STEPX_STP, OUTPUT);
   pinMode(PIN_STEPY_DIR, OUTPUT);
   pinMode(PIN_STEPY_STP, OUTPUT);
   pinMode(PIN_BTN_STIFT, INPUT_PULLUP);

   //Zuordnung des Servos zu den Pin und zu seinen Pulsweiten
   myServo.attach(PIN_SERVO, 900, 2100);
   myServo.writeMicroseconds(OBEN); //anheben des Stifts

   //Festlegung der maximalen Geschwindigkeiten und Beschleunigungen der Schrittmotoren
   StepperX.setMaxSpeed(1000);
   StepperX.setAcceleration(150);
   StepperY.setMaxSpeed(1000);
   StepperY.setAcceleration(150);

   //Fahren in die Position (0,0)
   goHome();

   // Öffne Kommunikation SD Karte
   if (!SD.begin(PIN_CS)) {          
     Serial.println("SD-Karte verweigert");
   } else {
     Serial.println("SD-Karte erfolgreich");
   }
} //Ende Setup

void loop(){
  //Lesen des Schalterzustandes von SD-Modus (SD-Modus/Manueller Modus)
  sw_sd_modus = buttonToggle(PIN_BTN_MODUS,schalter_modus,merker_modus);

  //SD-Modus
  if(sw_sd_modus){
    goHome(); // Fahre zur Ausgangsposition (0,0)
    myFile = SD.open(filename); //Öffne die Datei
    
    if(myFile){
      while (myFile.available()&& sw_sd_modus){ //Lesen der Datei bis diese vollständig gelesen wurde bzw. der SD-Modus verlassen wurde
        sw_sd_modus = buttonToggle(PIN_BTN_MODUS,schalter_modus,merker_modus);
        if(!stopRead){                  //Solange Motor nicht noch mit Auswertung der letzten Zeile beschäftigt ist
          tempchar = myFile.read();     //Zeichenweise einlesen
        }
        if (tempchar != '\n') {         //Erkennen von Zeilenumbruch
            input += tempchar;          //String aufaddieren

            //Stoppen des Aufaddieren des X- bzw. Y-Wert-String bei Leerzeichen
            if (tempchar == ' ') {      
              merkeX = false;
              merkeY = false;
            }

            //Aufaddieren des X- bzw. Y-Wert-String aus der Datei
            if (merkeX == true) {
              x += tempchar;
            }
            if (merkeY == true) {
              y += tempchar;
            }

            //Start des Aufaddieren des X- bzw. Y-Wert-String mit dem darauffolgenden Zeichen
            if (tempchar == 'X') {
              merkeX = true;
            }
            if (tempchar == 'Y') {
              merkeY = true;
            }
        } else {            //Auswertung der gelesenen Zeile
          stopRead = true;
          merkeX = false;
          merkeY = false;
          Serial.print("Input: ");
          Serial.println(input);

          //Auslesen der derzeitigen Position
          positionX = StepperX.currentPosition();
          positionY = StepperY.currentPosition();
            
          //Berechnung der benötigten Anzahl an Steps aus der Datei
          StepsX = x.toFloat()/0.15;
          StepsY = y.toFloat()/0.15;

          //Auslesen der Anweisung des G-Code, ob Linie gezeichnet werden soll 
          if (input.substring(0, 3) == "G01"){
            myServo.writeMicroseconds(UNTEN);
          }
          if (input.substring(0, 3) == "G00"){
            myServo.writeMicroseconds(OBEN);
          } 

          //Ansteuerung des Schrittmotors in X-Richtung
          if((StepsX < positionX) && digitalRead(PIN_X_LINKS)){
            digitalWrite(PIN_STEPX_DIR, HIGH);
            StepperX.moveTo(StepsX);
            StepperX.run();
          } else if ((StepsX >= positionX) && digitalRead(PIN_X_RECHTS)){
            digitalWrite(PIN_STEPX_DIR, LOW);
            StepperX.moveTo(StepsX);
            StepperX.run();
          } else {
            StepperX.stop();
            myFile.close();
            goHome();
            SD.open(filename);
          }

          //Ansteuerung des Schrittmotors in Y-Richtung
          if((StepsY < positionY) && digitalRead(PIN_Y_UNTEN)){
            digitalWrite(PIN_STEPY_DIR, LOW);
            StepperY.moveTo(StepsY);
            StepperY.run();
          } else if ((StepsY >= positionY) && digitalRead(PIN_Y_OBEN)){
            digitalWrite(PIN_STEPY_DIR, HIGH);
            StepperY.moveTo(StepsY);
            StepperY.run();
          } else {
            StepperY.stop();
            myFile.close();
            goHome();
            SD.open(filename);
          }

           // gewünschte Position wurde erreicht -> Strings zurücksetzen und nächste Zeile soll gelesen werden
          if((StepsX == StepperX.currentPosition()) && (StepsY == StepperY.currentPosition())){
            stopRead = false;
            input = "";
            x = "";
            y = "";
          }
        }
      } //Ende der Datei bzw. Ende des SD-Modus
      myFile.close(); //Datei schließen
    }
  } else { //Manueller Modus
    Serial.println("Manueller Modus");  
    myFile.close();   

    //Button-Modus, wenn Joystick in Mittelstellung
    if(analogRead(PIN_VRX) > 750 && analogRead(PIN_VRX) < 850 && analogRead(PIN_VRY) > 750 && analogRead(PIN_VRX) < 850){
      if(digitalRead(PIN_BTN_HOCH) && digitalRead(PIN_Y_OBEN) && digitalRead(PIN_BTN_RUNTER) == LOW){ //nach oben
        digitalWrite(PIN_STEPY_DIR, LOW);
        StepperY.setSpeed(600);
        StepperY.run();
      }
      if(digitalRead(PIN_BTN_RUNTER) && digitalRead(PIN_Y_UNTEN) && digitalRead(PIN_BTN_HOCH) == LOW){ //nach unten
        digitalWrite(PIN_STEPY_DIR, HIGH);
        StepperY.setSpeed(600);
        StepperY.run();
      }
      if(digitalRead(PIN_BTN_LINKS) && digitalRead(PIN_X_LINKS) && digitalRead(PIN_BTN_RECHTS) == LOW){ //nach rechts
        digitalWrite(PIN_STEPX_DIR, LOW);
        StepperX.setSpeed(600);
        StepperX.run();
      }
      if(digitalRead(PIN_BTN_RECHTS) && digitalRead(PIN_X_RECHTS) && digitalRead(PIN_BTN_LINKS) == LOW){ //nach links
        digitalWrite(PIN_STEPX_DIR, HIGH);
        StepperX.setSpeed(600);
        StepperX.run();
      }

      // Geschwindigkeit durch den Joystick = 0
      velocityX = 0;
      velocityY = 0;
    }

    //Joystick-Modus
    //Kontrolle der X-Richtung
    if(digitalRead(PIN_X_RECHTS) && analogRead(PIN_VRX) > 850){
      digitalWrite(PIN_STEPX_DIR, LOW);
      velocityX = ((analogRead(PIN_VRX)-850)*1000)/173;
      StepperX.setSpeed(velocityX);
      StepperX.run();
    } else if(digitalRead(PIN_X_LINKS) && analogRead(PIN_VRX) < 750){
      digitalWrite(PIN_STEPX_DIR, HIGH);
      velocityX = (750-(analogRead(PIN_VRX)))*1.3333;
      StepperX.setSpeed(velocityX);
      StepperX.run();
    } else{
      StepperX.stop();
    }
    
    //Kontrolle der Y-Richtung
    if(digitalRead(PIN_Y_UNTEN) && analogRead(PIN_VRY) < 750){
      digitalWrite(PIN_STEPY_DIR, LOW);
      velocityY = (750-(analogRead(PIN_VRY)))*1.3333;
      StepperY.setSpeed(velocityY);
      StepperY.run();
    } else if(digitalRead(PIN_Y_OBEN) && analogRead(PIN_VRY) > 850){
      digitalWrite(PIN_STEPY_DIR, HIGH);
      velocityY = ((analogRead(PIN_VRY)-850)*1000)/173;
      StepperY.setSpeed(velocityY);
      StepperY.run();
    } else{
      StepperY.stop();
    }

    //Kontrolle des Stifts
    sw_stift_oben = buttonToggle(PIN_BTN_STIFT, schalter_stift, merker_stift);
    if(sw_stift_oben){
      myServo.writeMicroseconds(OBEN);
    } else {
      myServo.writeMicroseconds(UNTEN);
    }
    
  } // Ende Manueller Modus
} //Ende Loop

//Verwendung eines Tasters als Schalters
bool buttonToggle(int btn, bool &schalter, bool &merker){
  const unsigned long debouncetime = 40;
   
  if (millis()-lasttime > debouncetime){
    if(digitalRead(btn)){
      lasttime = millis();
      if(!merker){
        schalter = !schalter;
        merker = HIGH;
      }
    } else {
      merker = LOW;
    }
  }
  return (schalter);
}

// Fahre in die Position (0,0)
void goHome(){
  bool links_merke = HIGH;
  bool unten_merke = HIGH;
  
  while(digitalRead(PIN_X_LINKS)|| digitalRead(PIN_Y_UNTEN)){     
    if(links_merke == HIGH){
      links_merke = digitalRead(PIN_X_LINKS);
      digitalWrite(PIN_STEPX_DIR, HIGH);
      StepperX.setSpeed(600);
      StepperX.run();
    } else {
      StepperX.stop();
    }
    if(unten_merke == HIGH){
      unten_merke = digitalRead(PIN_Y_UNTEN);
      digitalWrite(PIN_STEPY_DIR, LOW);
      StepperY.setSpeed(600);
      StepperY.run();
    } else {
      StepperY.stop();
    }
  }
  
  // Setzen der derzeitigen Position als Referenzpunkt (0,0)
  positionX = StepperX.currentPosition();
  positionY = StepperY.currentPosition();
  StepperX.setCurrentPosition(positionX);
  StepperY.setCurrentPosition(positionY);
  positionX = 0;
  positionY = 0;
}
