// =========================================================================
//  ESP32 Multi-Sensor Project met Blynk, LCD, IR, DHT11, BH1750, RGB LED
// =========================================================================
// Dit project leest data van diverse sensoren (temperatuur, luchtvochtigheid,
// lichtintensiteit), toont deze op een LCD en stuurt ze naar Blynk.
// Het kan ook een RGB LED aansturen via een IR afstandsbediening en Blynk.
// =========================================================================

// == BIBLIOTHEKEN (INCLUDES) ==
#include <Wire.h>               // Nodig voor I2C communicatie (LCD, BH1750)
#include <BH1750.h>             // Bibliotheek voor BH1750 lichtsensor
#include <DHT.h>                // Bibliotheek voor DHT temperatuur/vochtigheidssensor
#include <IRremoteESP8266.h>    // Bibliotheek voor IR communicatie op ESP32/ESP8266
#include <IRrecv.h>             // Specifiek voor IR ontvangen
#include <IRutils.h>            // Hulpmiddelen voor IR data
#include <LiquidCrystal_I2C.h>  // Bibliotheek voor I2C LCD display
#include <WiFi.h>               // Bibliotheek voor WiFi verbinding
#include <WiFiClient.h>         // Bibliotheek voor WiFi client functionaliteit

// == BLYNK CONFIGURATIE ==
#define BLYNK_TEMPLATE_ID "user1"                            // Vervang indien nodig
#define BLYNK_TEMPLATE_NAME "user1@server.wyns.it"           // Vervang indien nodig
#define BLYNK_AUTH_TOKEN "E67S7HMjQmUg3rCEgPSyuaFuGu9zM2cx"  // Jouw Blynk Authenticatie Token
#define BLYNK_PRINT Serial                                   // Gebruik Serial Monitor voor Blynk debug output

#include <BlynkSimpleEsp32.h>  // Blynk bibliotheek (na BLYNK_ defines includen)

// == WIFI CONFIGURATIE ==
char ssid[] = "Pixel 5 van Dwd";         // SSID (naam) van jouw WiFi netwerk
char pass[] = "DwdphoneWifi";            // Wachtwoord van jouw WiFi netwerk
char blynk_server[] = "server.wyns.it";  // Adres van de custom Blynk server
uint16_t blynk_port = 8081;              // Poort van de custom Blynk server

// == PIN DEFINITIES ==
// Let op: Controleer of deze pinnen correct zijn voor jouw ESP32 bord/layout!
#define IR_RECV_PIN 4   // GPIO pin waar de DATA pin (OUT) van de VS1838B op is aangesloten
#define DHT_PIN 16      // GPIO pin waar de DATA pin van de DHT sensor op is aangesloten
#define DHT_TYPE DHT11  // Type DHT sensor (DHT11 of DHT22)
#define RGB_R_PIN 27    // GPIO pin voor de RODE anode/cathode van de RGB LED (PWM capabel)
#define RGB_G_PIN 25    // GPIO pin voor de GROENE anode/cathode van de RGB LED (PWM capabel)
#define RGB_B_PIN 32    // GPIO pin voor de BLAUWE anode/cathode van de RGB LED (PWM capabel)
// Standaard I2C pinnen op de meeste ESP32 borden: SDA = GPIO 21, SCL = GPIO 22

// == SENSOR & DISPLAY OBJECTEN ==
IRrecv irrecv(IR_RECV_PIN);          // Maak een IR ontvanger object aan
decode_results irResults;            // Struct om gedecodeerde IR data op te slaan
DHT dht(DHT_PIN, DHT_TYPE);          // Maak een DHT sensor object aan
BH1750 lightMeter;                   // Maak een BH1750 lichtsensor object aan (gebruikt standaard I2C adres 0x23)
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Maak een LCD object aan (I2C adres 0x27, 16 kolommen, 2 rijen)

// == BLYNK VIRTUELE PINNEN ==
// Definieer welke virtuele pin in de Blynk app correspondeert met welke data
#define VPIN_TEMP V1  // Virtuele Pin voor Temperatuur
#define VPIN_HUM V2   // Virtuele Pin voor Luchtvochtigheid
#define VPIN_LUX V3   // Virtuele Pin voor Lichtintensiteit (Lux)
#define VPIN_RGB V4   // Virtuele Pin voor RGB Kleur (bijv. ZeRGBa widget)

// == TIMING VARIABELEN ==
// Voor het niet-blokkerend uitlezen van sensoren
const unsigned long sensorInterval = 5000;  // Interval in milliseconden (5000 ms = 5 seconden)
unsigned long laatsteSensorUpdate = 0;      // Houdt bij wanneer de sensoren voor het laatst zijn gelezen

// =========================================================================
//  SETUP FUNCTIE - Wordt één keer uitgevoerd bij opstarten/reset
// =========================================================================
void setup() {
  // Start seriële communicatie voor debuggen
  Serial.begin(115200);
  Serial.println("\n\n--- ESP32 Multi-Sensor Initialisatie Start ---");

  // Initialiseer I2C bus (nodig voor LCD & BH1750)
  Wire.begin();
  Serial.println("I2C Bus Geïnitialiseerd.");

  // Initialiseer LCD display
  lcd.init();                     // Initialiseer het LCD scherm
  lcd.backlight();                // Zet de achtergrondverlichting aan
  lcd.print("Initialiseren...");  // Toon opstartbericht
  Serial.println("LCD Geïnitialiseerd.");

  // Initialiseer DHT sensor
  dht.begin();
  Serial.println("DHT Sensor Geïnitialiseerd.");

  // Initialiseer BH1750 lichtsensor
  // Gebruik HIGH_RES_MODE voor betere nauwkeurigheid
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 Lichtsensor Geïnitialiseerd.");
  } else {
    Serial.println("!! FOUT: BH1750 Initialisatie Mislukt!");
    lcd.setCursor(0, 1);
    lcd.print("BH1750 Fout!");
    // Hier zou je kunnen kiezen om de code te stoppen of een foutstatus te behouden
  }

  // Maak verbinding met WiFi
  setupWifi();  // Deze functie handelt ook output naar Serial/LCD af

  // Configureer en verbind met Blynk server (pas NA succesvolle WiFi verbinding)
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi verbonden, configureren van Blynk...");
    lcd.clear();
    lcd.print("Blynk Config...");
    Blynk.config(BLYNK_AUTH_TOKEN, blynk_server, blynk_port);

    // Probeer te verbinden met Blynk (met een korte timeout)
    // Blynk.connect() kan blokkeren, dus gebruik met voorzichtigheid of timeout
    bool blynkConnected = Blynk.connect(3000);  // Probeer 3 seconden te verbinden

    if (blynkConnected) {
      Serial.println("Verbonden met Blynk server!");
      lcd.clear();
      lcd.print("Blynk Verbonden");
      delay(1500);  // Toon bericht even
    } else {
      Serial.println("!! FOUT: Kon niet verbinden met Blynk server binnen timeout.");
      lcd.clear();
      lcd.print("Blynk Fout!");
      delay(1500);  // Toon foutmelding even
    }
  } else {
    Serial.println("!! FOUT: Geen WiFi verbinding, kan Blynk niet configureren.");
    lcd.clear();
    lcd.print("Blynk Fout!");
    lcd.setCursor(0, 1);
    lcd.print("Geen WiFi");
    delay(1500);
  }


  // Start de IR ontvanger
  irrecv.enableIRIn();  // Zet de IR ontvanger aan
  Serial.println("IR Ontvanger Geactiveerd.");

  // Configureer de pinnen voor de RGB LED als OUTPUT
  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);
  setRGB(0, 0, 0);  // Begin met de LED uit
  Serial.println("RGB LED Pinnen Geconfigureerd.");

  // Initialiseer de timer voor sensor updates
  laatsteSensorUpdate = millis();
  Serial.println("--- Setup Voltooid ---");
  lcd.clear();
  lcd.print("Systeem Gereed");
  delay(1500);  // Toon bericht even
  lcd.clear();  // Maak LCD leeg voor sensor data
}

// =========================================================================
//  LOOP FUNCTIE - Wordt continu herhaald na setup()
// =========================================================================
void loop() {
  // Voer Blynk taken uit (communicatie met server, widget updates)
  // Doe dit alleen als er een actieve verbinding is om crashes te voorkomen
  if (Blynk.connected()) {
    Blynk.run();
  } else {
    // Optioneel: Probeer hier opnieuw te verbinden als de verbinding verbroken is.
    // Wees voorzichtig, Blynk.connect() kan lang blokkeren.
    Serial.println("Blynk verbinding verloren, probeer opnieuw...");
    Blynk.connect(3000);  // Probeer kort opnieuw te verbinden
  }


  // Controleer continu op inkomende IR signalen
  readIRRemote();

  // Controleer of het tijd is om sensor data te lezen en te updaten
  // Dit gebeurt niet-blokkerend dankzij millis() timing
  if (millis() - laatsteSensorUpdate >= sensorInterval) {
    updateEnvironmentData();         // Roep de functie aan om data te verwerken
    laatsteSensorUpdate = millis();  // Reset de timer voor de volgende interval
  }

  // Een kleine vertraging om te voorkomen dat de loop te snel draait
  // en de CPU onnodig belast. Kan aangepast worden indien nodig.
  delay(10);
}

// =========================================================================
//  FUNCTIE: setupWifi - Maakt verbinding met het WiFi netwerk
// =========================================================================
void setupWifi() {
  Serial.print("Verbinden met WiFi: ");
  Serial.println(ssid);
  lcd.clear();
  lcd.print("Verbinden met:");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.begin(ssid, pass);  // Start de WiFi verbinding

  int wifi_attempts = 0;
  // Wacht tot de verbinding is gemaakt (met een maximum aantal pogingen)
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 30) {  // Max ~15 seconden
    delay(500);
    Serial.print(".");
    lcd.print(".");
    wifi_attempts++;
  }

  // Controleer of de verbinding succesvol was
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbonden!");
    Serial.print("IP-adres: ");
    Serial.println(WiFi.localIP());  // Print het verkregen IP adres
    lcd.clear();
    lcd.print("WiFi Verbonden!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(1500);  // Toon IP even op LCD
  } else {
    Serial.println("\n!! FOUT: Kon geen WiFi verbinding maken!");
    lcd.clear();
    lcd.print("WiFi Fout!");
  }
}

// =========================================================================
//  FUNCTIE: readIRRemote - Leest IR codes en stelt RGB kleur in
// =========================================================================
void readIRRemote() {
  // Controleer of er een IR code is ontvangen en gedecodeerd
  if (irrecv.decode(&irResults)) {
    Serial.print("IR Code Ontvangen: 0x");  // Print hex code voor debuggen
    Serial.println(irResults.value, HEX);

    // Verwerk de code met een switch statement
    // De 'case' waarden zijn de HEX codes die JIJ hebt opgenomen van JOUW afstandsbediening
    switch (irResults.value) {
      // --- Cijfer Knoppen ---
      case 0xFF6897: setRGB(255, 0, 0); break;      // 0 -> Rood
      case 0xFF30CF: setRGB(0, 255, 0); break;      // 1 -> Groen
      case 0xFF18E7: setRGB(0, 0, 255); break;      // 2 -> Blauw
      case 0xFF7A85: setRGB(255, 255, 0); break;    // 3 -> Geel
      case 0xFF10EF: setRGB(0, 255, 255); break;    // 4 -> Cyaan
      case 0xFF38C7: setRGB(255, 0, 255); break;    // 5 -> Magenta
      case 0xFF5AA5: setRGB(255, 255, 255); break;  // 6 -> Wit
      case 0xFF42BD: setRGB(255, 100, 0); break;    // 7 -> Oranje
      case 0xFF4AB5: setRGB(128, 0, 128); break;    // 8 -> Paars
      case 0xFF52AD:
        setRGB(0, 128, 128);
        break;  // 9 -> Teal (Blauwgroen)

      // --- Symbool/Functie Knoppen ---
      case 0xFFE01F: setRGB(100, 100, 100); break;  // - -> Grijs (Gedimd Wit)
      case 0xFFA857: setRGB(150, 255, 255); break;  // + -> Helder Cyaan
      case 0xFF906F: setRGB(255, 105, 180); break;  // EQ -> Roze
      case 0xFF22DD: setRGB(150, 255, 0); break;    // < -> Lime Groen
      case 0xFF02FD: setRGB(0, 150, 255); break;    // > -> Hemelsblauw
      case 0xFFC23D: setRGB(255, 180, 0); break;    // Start -> Goud
      case 0xFF9867: setRGB(75, 0, 130); break;     // 100+ -> Indigo
      case 0xFFB04F:
        setRGB(238, 130, 238);
        break;  // 200+ (of 100-) -> Violet

      // --- Kanaal Knoppen ---
      case 0xFFA25D: setRGB(139, 0, 0); break;  // CH- -> Donkerrood
      case 0xFFE21D: setRGB(0, 100, 0); break;  // CH+ -> Donkergroen
      case 0xFF629D:
        setRGB(0, 0, 128);
        break;  // CH -> Marineblauw

      // --- Speciaal: Herhaal Code ---
      case 0xFFFFFFFF:  // Standaard NEC herhaalcode (wanneer knop ingedrukt blijft)
        Serial.println("IR: Herhaal signaal");
        // Meestal geen actie nodig bij herhaling, dus alleen 'break'
        break;

      // --- Standaard: Onbekende Code ---
      default:
        Serial.println("IR: Onbekende knop code");
        // Optioneel: Zet LED uit of naar een standaard kleur
        // setRGB(0, 0, 0); // Voorbeeld: LED uit bij onbekende code
        break;
    }
    // Bereid de ontvanger voor op het volgende signaal
    irrecv.resume();
  }
}

// =========================================================================
//  FUNCTIE: updateEnvironmentData - Leest sensoren, update LCD & Blynk
// =========================================================================
void updateEnvironmentData() {
  // Lees Temperatuur & Vochtigheid van DHT sensor
  float temp = dht.readTemperature();  // Lees temperatuur in graden Celsius
  float hum = dht.readHumidity();      // Lees relatieve luchtvochtigheid in %

  // Lees Lichtintensiteit van BH1750 sensor
  float lux = -1.0;  // Initialiseer op ongeldige waarde
  // Controleer of er een nieuwe meting beschikbaar is (voor continuous mode)
  if (lightMeter.measurementReady()) {
    lux = lightMeter.readLightLevel();  // Lees lichtniveau in Lux
  }

  // --- Update LCD Display ---
  lcd.clear();          // Maak LCD leeg voor nieuwe data
  lcd.setCursor(0, 0);  // Zet cursor op eerste regel, eerste positie
  lcd.print("T:");
  if (!isnan(temp)) {      // Alleen printen als de waarde geldig is
    lcd.print(temp, 1);    // Print temperatuur met 1 decimaal
    lcd.print((char)223);  // Graden symbool °
    lcd.print("C");
  } else {
    lcd.print("---");  // Toon streepjes bij ongeldige meeting
  }

  lcd.print(" H:");
  if (!isnan(hum)) {    // Alleen printen als de waarde geldig is
    lcd.print(hum, 0);  // Print luchtvochtigheid zonder decimalen
    lcd.print("%");
  } else {
    lcd.print("---");  // Toon streepjes bij ongeldige meeting
  }

  lcd.setCursor(0, 1);  // Zet cursor op tweede regel, eerste positie
  lcd.print("Licht: ");
  if (lux >= 0) {    // Alleen printen als de waarde geldig is (niet -1)
    lcd.print(lux);  // Print Lux waarde zonder decimalen
    lcd.print(" lux");
  } else {
    lcd.print("--- lux");  // Toon streepjes bij ongeldige meeting
  }

  // --- Update Serial Monitor ---
  Serial.print(" Sensor Data: ");
  if (!isnan(temp)) {
    Serial.print("Temp=");
    Serial.print(temp);
    Serial.print("C ");
  } else {
    Serial.print("Temp=Error ");
  }
  if (!isnan(hum)) {
    Serial.print("Hum=");
    Serial.print(hum);
    Serial.print("% ");
  } else {
    Serial.print("Hum=Error ");
  }
  if (lux >= 0) {
    Serial.print("Lichtniveau=");
    Serial.print(lux);
    Serial.print("lux");
  } else {
    Serial.print("Lux=Error");
  }
  Serial.println();  // Nieuwe regel na sensor data

  // --- Update Blynk (alleen als verbonden en data geldig is) ---
  if (Blynk.connected()) {
    if (!isnan(temp)) {
      Blynk.virtualWrite(VPIN_TEMP, temp);  // Stuur temperatuur naar Blynk
    }
    if (!isnan(hum)) {
      Blynk.virtualWrite(VPIN_HUM, hum);  // Stuur luchtvochtigheid naar Blynk
    }
    if (lux >= 0) {
      Blynk.virtualWrite(VPIN_LUX, lux);  // Stuur lichtintensiteit naar Blynk
    }
  }
}

// =========================================================================
//  FUNCTIE: setRGB - Stelt de kleur van de RGB LED in
// =========================================================================
// Parameters:
//  r: Rood intensiteit (0-255)
//  g: Groen intensiteit (0-255)
//  b: Blauw intensiteit (0-255)
void setRGB(int r, int g, int b) {
  // Optioneel: Beperk waarden tot 0-255 bereik voor veiligheid
  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);

  // Stuur PWM signaal naar de respectievelijke LED pinnen
  // Aanname: Common Cathode LED (LOW = uit, HIGH = aan -> 0 = uit, 255 = max)
  // Als je een Common Anode LED hebt, moet je de waarden omkeren: (255 - r), etc.
  analogWrite(RGB_R_PIN, r);
  analogWrite(RGB_G_PIN, g);
  analogWrite(RGB_B_PIN, b);

  // Optioneel: Print ingestelde kleur naar Serial Monitor
  // Serial.printf("RGB ingesteld op: R=%d, G=%d, B=%d\n", r, g, b);
}

// =========================================================================
//  BLYNK FUNCTIE: BLYNK_WRITE - Wordt aangeroepen als widget op VPIN_RGB verandert
// =========================================================================
// Deze functie wordt uitgevoerd wanneer de waarde van een widget die is gekoppeld
// aan VPIN_RGB (bijv. een ZeRGBa kleurenkiezer) in de Blynk app verandert.
BLYNK_WRITE(VPIN_RGB) {
  // Haal de kleurwaarden (Rood, Groen, Blauw) op uit de parameter array
  // Blynk stuurt voor ZeRGBa meestal de waarden als R, G, B in param[0], param[1], param[2]
  int red = param[0].asInt();    // Haal Rood waarde op
  int green = param[1].asInt();  // Haal Groen waarde op
  int blue = param[2].asInt();   // Haal Blauw waarde op

  // Stel de RGB LED in op de ontvangen kleur
  setRGB(red, green, blue);

  Serial.println("RGB Kleur Aangepast via Blynk App.");
}

// =========================================================================
//                            EINDE VAN DE CODE
// =========================================================================