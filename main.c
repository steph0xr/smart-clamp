//#include "GPIO.h"
#include <EEPROM.h>


#define RELEASED           0
#define PRESSED            1

// GPIO definitions
const int releMotEnable         = 2;
const int releMotDir            = 3;
const int releCilindro          = 4;
const int comandoPinza          = 5;
const int homeFineCorsa         = 6;
const int led                   = 7;
const int calibrazioneSetpoint  = 8;
const int fineCorsaCilindro     = 9;
const int fineCorsaPinza        = 10;
const int homeCilindro          = 11;

int contextEEAddress = 0;
int timeout = 0;

static struct {
  int numeroGiri;
  int setpointPosizioneAssoluta;
} _context;

// functions declaration
void initContextInRom();
void saveNumeroGiriInRom(const int giri);
int  getNumeroGiriFromRom();
void aggiornaStatoIngressi();

static struct {
  int pulsantePinza = 0;
  int isAtHome = 0;
  int cilindroEsteso = 0;
  int cilindroRetratto = 0;
} _state;


void setup()
{
  // set gpio mode
  pinMode(releMotEnable,        OUTPUT);
  pinMode(releMotDir,           OUTPUT);
  pinMode(releCilindro,         OUTPUT);
  pinMode(led,                  OUTPUT);
  pinMode(comandoPinza,         INPUT);
  pinMode(homeFineCorsa,        INPUT);
  pinMode(calibrazioneSetpoint, INPUT);
  pinMode(fineCorsaCilindro,    INPUT);
  pinMode(fineCorsaPinza,       INPUT);
  pinMode(homeCilindro,         INPUT);

  initContextInRom();

  // set serial
  Serial.begin(115200);
  delay(1);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("********************************");
  Serial.println("Avvio...");
  Serial.println("setup periferiche in corso");

  delay(1000);

  // per testare pin digitali scommentare la seguente funzione:
   // provaGPIO();

  aggiornaStatoIngressi();

if (! _state.cilindroRetratto) {
    Serial.println("cilindro non retratto");
  timeout = rilasciaCilindroContrasto();
  if (timeout)
    allarme();
}

  if (! _state.isAtHome) {
    Serial.println("pinza non in home");
    timeout = homing();
    if (timeout)
      allarme();
  }

  // now at home
  Serial.println("procedura homing avvenuta con successo");
  Serial.println("dispositivo pronto");
  Serial.println("Premere pulsante pinza per aprire pinza");
}




void loop()
{
  aggiornaStatoIngressi();

  
  if (_state.pulsantePinza == PRESSED) {

    timeout = avviaCilindroContrasto(); 
    if (timeout)
      allarme();

    impostaDirezioneMotoreAvanti();

    // delay(1000) ??
    avviaMotore();

    timeout = checkSetpointPinza();
    if (timeout)
      allarme();

    Serial.println("setpoint raggiunto");

    fermaMotore();

    timeout = checkTimeoutAperturaPinza();
    if (timeout)
      Serial.println("ritorno in posizione di homing per timeout");
    else
      Serial.println("ritorno in posizione di homing per pressione pulsante pinza");

    timeout = homing();
    if (timeout)
      allarme();

    timeout = rilasciaCilindroContrasto();
    if (timeout)
      allarme();

    Serial.println("fine ciclo !!");
    Serial.println("");
    Serial.println("*** nuova operazione ***");
    Serial.println("Premere pulsante pinza per aprire pinza");

    delay(1000);
  }

  delay(100);
}





// helper functions

void avviaMotore()
{
  Serial.println("avvia Motore");
  digitalWrite(releMotEnable, HIGH);
}

void fermaMotore()
{
  Serial.println("ferma Motore");
  digitalWrite(releMotEnable, LOW);
}

void impostaDirezioneMotoreAvanti()
{
  Serial.println("imposta Direzione Motore: Avanti");
  digitalWrite(releMotDir, LOW);
}

void impostaDirezioneMotoreIndietro()
{
  Serial.println("imposta Direzione Motore: Indietro");
  digitalWrite(releMotDir, HIGH);
}

int avviaCilindroContrasto()
{
  int timeout_cilindro_s = 120;
  Serial.print("avvio Cilindro Contrasto. Timeout [s]:");
  Serial.println(timeout_cilindro_s);

  digitalWrite(releCilindro, HIGH);

  do {
    delay(1000);
    _state.cilindroEsteso = digitalRead(fineCorsaCilindro);

    // check timeout
    if (timeout_cilindro_s == 0) {
      Serial.print("timeout posizionamento Cilindro Contrasto [s]: ");
      Serial.println(timeout_cilindro_s);
      return -1;
    }
    timeout_cilindro_s = timeout_cilindro_s - 1;

  } while (! _state.cilindroEsteso);

  Serial.println("cilindro esteso");

  return 0;
}

int rilasciaCilindroContrasto()
{
  int timeout_cilindro_s = 120;
  Serial.print("rilascia Cilindro Contrasto. Timeout [s]:");
  Serial.println(timeout_cilindro_s);
  digitalWrite(releCilindro, LOW);

  do {
    delay(1000);
    _state.cilindroRetratto = digitalRead(homeCilindro);

    // check timeout
    if (timeout_cilindro_s == 0) {
      Serial.print("timeout ritiro Cilindro Contrasto [s]: ");
      Serial.println(timeout_cilindro_s);
      return -1;
    }
    timeout_cilindro_s = timeout_cilindro_s - 1;

  } while (! _state.cilindroRetratto);

  Serial.println("cilindro di contrasto rilasciato correttamente");
  return 0;
}

void aggiornaStatoIngressi()
{
  _state.pulsantePinza = digitalRead(comandoPinza);
  _state.isAtHome = digitalRead(homeFineCorsa);
  _state.cilindroEsteso = digitalRead(fineCorsaCilindro);
  _state.cilindroRetratto = digitalRead(homeCilindro);
}

void clearRom()
{
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
}

void initContextInRom()
{
  _context.numeroGiri = 0;
  EEPROM.put(contextEEAddress, _context);
}

void saveDatiEncoderInRom(const int giri, const float angolo)
{
  _context.numeroGiri = giri;
  _context.setpointPosizioneAssoluta = angolo;
  EEPROM.put(contextEEAddress, _context);
}

int getNumeroGiriFromRom()
{
  Serial.print("Read current motor turns from eeprom: ");
  EEPROM.get(contextEEAddress, _context);
  Serial.print("numero giri encoder: ");
  Serial.println(_context.numeroGiri);
  return _context.numeroGiri;
}

int getAngoloResiduoFromRom()
{
  Serial.print("Read current motor angle from eeprom: ");
  EEPROM.get(contextEEAddress, _context);
  Serial.print("angolo residuo encoder: ");
  Serial.println(_context.setpointPosizioneAssoluta);
  return _context.setpointPosizioneAssoluta;
}

int homing()
{
  int timeout_homing_s = 120;
  Serial.print("avvio procedura homing. timeout[s]: ");
  Serial.println(timeout_homing_s);

  impostaDirezioneMotoreIndietro();

  // delay(100)   ???
  avviaMotore();

  Serial.println("attesa homing..");
  do {
    _state.isAtHome = digitalRead(homeFineCorsa);

    // check timeout
    if (timeout_homing_s == 0) {
      Serial.println("timeout procedura homing");
      return -1;
    }
    timeout_homing_s = timeout_homing_s - 1;

    delay(1000);
  } while (! _state.isAtHome);


  fermaMotore();

  return 0;
}

void allarme()
{
  Serial.println("--> allarme <--");

  while (1) {
    digitalWrite(led, HIGH);
    delay(1000);
    digitalWrite(led, LOW);
    delay(1000);
  }
}

int checkSetpointPinza()
{
  float currentAngle;
  int delay_ms = 100;
  int timeout_finecorsa_pinza_s = 120;
  int count = 0;
  int setpointRaggiunto = 0;

  Serial.println("attesa raggiungimento setpoint..");
  do {
    setpointRaggiunto = digitalRead(fineCorsaPinza);

    // check timeout
    if (count*delay_ms > timeout_finecorsa_pinza_s*1000) {
      Serial.print("timeout secondi raggiungimento setpoint: ");
      Serial.println(timeout_finecorsa_pinza_s);
      return -1;
    }
    count = count + 1;

    delay(delay_ms);
  } while (! setpointRaggiunto);

  return 0;
}

int checkTimeoutAperturaPinza()
{
  uint32_t timeout_pinza_s = 10*60;
  int delay_ms = 100;
  int count = 0;

  Serial.print("attesa pulsante in stato pinza aperta.. timeout [s]:");
  Serial.println(timeout_pinza_s);
  do {
    _state.pulsantePinza = digitalRead(comandoPinza);

    // check timeout
    if (count*delay_ms > timeout_pinza_s*1000) {
      Serial.print("timeout secondi pinza aperta [s]: ");
      Serial.println(timeout_pinza_s*1000);
      return -1;
    }
    count = count + 1;

 
    delay(delay_ms);

  } while (! _state.pulsantePinza);

  return 0;
}

void provaGPIO()
{
    while (1) {
    Serial.print("comandoPinza:");
    Serial.println(digitalRead(comandoPinza));
    Serial.print("homeFineCorsa:");
    Serial.println(digitalRead(homeFineCorsa));
    Serial.print("calibrazioneSetpoint:");
    Serial.println(digitalRead(calibrazioneSetpoint));
    Serial.print("fineCorsaCilindro:");
    Serial.println(digitalRead(fineCorsaCilindro));
    Serial.print("fineCorsaPinza:");
    Serial.println(digitalRead(fineCorsaPinza));
   
    digitalWrite(releMotEnable, HIGH);
    delay(500);
    digitalWrite(releMotEnable, LOW);

    digitalWrite(releMotDir, HIGH);
    delay(500);
    digitalWrite(releMotDir, LOW);
    
    digitalWrite(releCilindro, HIGH);
    delay(500);
    digitalWrite(releCilindro, LOW);

    Serial.println("-------------------------------");
     Serial.println(" ");
  }
}
