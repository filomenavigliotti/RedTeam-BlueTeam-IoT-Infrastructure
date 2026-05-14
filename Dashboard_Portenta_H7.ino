#include <WiFi.h>
#include <WiFiUdp.h> // Per il Syslog

// --- CONFIGURAZIONE RETE STATICA ---
IPAddress local_IP(192, 168, 137, 197); // L'IP fisso
IPAddress gateway(192, 168, 137, 1);    
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// --- CONFIGURAZIONE RETE ---
char ssid[] = "Wifi_Aziendale";     
char pass[] = "azienda1234"; 

// --- CONFIGURAZIONE WAZUH ---
WiFiUDP udp;
const char* wazuh_ip = "192.168.137.240"; 
const int syslog_port = 514;

// --- CONFIGURAZIONE SENSORI ---
#define PIRPIN 3       // Sensore Movimento (D3)
#define SOUNDPIN 4     // Sensore Suono (D4)

WiFiServer server(80);

// --- VARIABILI HIDS E ALLARMI ---
unsigned long lastCheck = 0;
int reqCount = 0;
bool underAttack = false;
const int DOS_THRESHOLD = 3;

// Memoria per i sensori (per non spammare log a Wazuh)
bool rumoreRilevato = false;
int lastMotionState = LOW;

// Variabile per il bottone di spegnimento dashboard
bool dashboardActive = true;

// --- FUNZIONE DI INVIO LOG A WAZUH ---
void sendSyslog(String severity, String message) {
  String prival = "<13>"; 
  if (severity == "ALERT") prival = "<9>";
  if (severity == "ERROR") prival = "<11>";
  if (severity == "INFO") prival = "<14>";

  // Nome generico per confondere chi legge i log non autorizzati
  String syslogMsg = prival + "portenta_h7: " + message;
  
  udp.beginPacket(wazuh_ip, syslog_port);
  udp.print(syslogMsg);
  udp.endPacket();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT); pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH);

  // Inizializza i pin dei sensori
  pinMode(PIRPIN, INPUT);
  pinMode(SOUNDPIN, INPUT);

  delay(2000);
  Serial.println("Avvio Terminale di Sicurezza (PIR + Acustico)...");

  // Applichiamo l'IP Statico richiesto
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  
  WiFi.setHostname("portenta_h7");

  Serial.println("Inizializzazione Rete WiFi...");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(500); Serial.print(".");
    digitalWrite(LEDB, LOW); delay(50); digitalWrite(LEDB, HIGH); 
  }

  Serial.println("\n=== ONLINE ===");
  Serial.print("IP Assegnato: "); Serial.println(WiFi.localIP());
  
  // Mostra il MAC address in console
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC Address (Hardware Lock): ");
  for (int i = 0; i < 6; ++i) {
    Serial.print(String(mac[i], HEX));
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  digitalWrite(LEDG, LOW); delay(2000); digitalWrite(LEDG, HIGH); 
  
  udp.begin(2390);
  server.begin();

  // Manda il primo log a Wazuh!
  sendSyslog("INFO", "Dispositivo IoT avviato e connesso alla rete.");
}

void loop() {
  // --- ASCOLTO CONTINUO DEL MICROFONO ---
  if (digitalRead(SOUNDPIN) == HIGH && !rumoreRilevato) { 
    rumoreRilevato = true;
    sendSyslog("ALERT", "Allarme! Rumore sospetto rilevato nella Sala Server!");
  }

  // --- ASCOLTO CONTINUO DEL MOVIMENTO ---
  int motionState = digitalRead(PIRPIN);
  if (motionState == HIGH && lastMotionState == LOW) {
    sendSyslog("ALERT", "Intrusione rilevata! Sensore Volumetrico attivato.");
  }
  lastMotionState = motionState;

  // --- GESTIONE HIDS (Ogni 1 secondo) ---
  if (millis() - lastCheck >= 1000) {
    lastCheck = millis();
    if (underAttack && reqCount == 0) {
      digitalWrite(LEDR, HIGH); 
    }
    reqCount = 0;
    underAttack = false;
  }

  // --- GESTIONE WEB SERVER E DASHBOARD ---
  WiFiClient client = server.available();
  if (client) {
    reqCount++;
    if (reqCount > DOS_THRESHOLD) {
      if (!underAttack) {
        underAttack = true;
        digitalWrite(LEDB, HIGH); digitalWrite(LEDR, LOW);  
        sendSyslog("ERROR", "Possibile attacco DoS in corso! Superata la soglia di richieste HTTP.");
      }
    }

    if (!underAttack) digitalWrite(LEDB, LOW); 
    
    String header = ""; 
    boolean currentLineIsBlank = true;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c; 
        
        if (c == '\n' && currentLineIsBlank) {
          
          // --- CONTROLLO DEI BOTTONI ---
          if (header.indexOf("GET /spegni") >= 0 && dashboardActive) {
            dashboardActive = false; 
            sendSyslog("ALERT", "La Dashboard Web e' stata DISATTIVATA manualmente!");
          } 
          else if (header.indexOf("GET /accendi") >= 0 && !dashboardActive) {
            dashboardActive = true; 
            sendSyslog("INFO", "La Dashboard Web e' stata RIATTIVATA manualmente!");
          }

          // --- RISPOSTE DEL SERVER ---
          if (underAttack) {
            client.println("HTTP/1.1 503 Service Unavailable\r\nServer: Apache/2.4.41 (Ubuntu)\r\nContent-Type: text/html\r\nConnection: close\r\n");
            client.println("<html><body style='background-color: #c62828; color: white; text-align: center; font-family: Arial;'>");
            client.println("<h1 style='font-size: 50px; margin-top: 15%;'>[ ERRORE 503 ]</h1>");
            client.println("<h2>Superato il limite di richieste!</h2><p>Servizio temporaneamente irraggiungibile.</p></body></html>");
          
          } else if (!dashboardActive) {
            client.println("HTTP/1.1 200 OK\r\nServer: Apache/2.4.41 (Ubuntu)\r\nContent-Type: text/html\r\nConnection: close\r\n");
            client.println("<html><body style='background-color: #000000; color: #ff0000; text-align: center; font-family: monospace; padding-top: 15%;'>");
            client.println("<h1>[!] ACCESSO NEGATO [!]</h1><p>La Dashboard di Sicurezza e' stata messa offline.</p>");
            client.println("<br><br><a href='/accendi' style='text-decoration:none;'><button style='background-color:#3fb950; color:white; font-weight:bold; padding:15px 30px; border:none; border-radius:5px; cursor:pointer; font-size: 16px;'>RIATTIVA DASHBOARD</button></a>");
            client.println("</body></html>");
            
          } else {
            client.println("HTTP/1.1 200 OK\r\nServer: Apache/2.4.41 (Ubuntu)\r\nContent-Type: text/html\r\nConnection: close\r\n");
            client.println("<html><head><meta http-equiv='refresh' content='2'><title>Dashboard Sicurezza</title></head>");
            client.println("<body style='font-family: monospace; background-color: #0d1117; color: #58a6ff; padding: 20px;'>");
            client.println("<h1 style='border-bottom: 1px solid #58a6ff; padding-bottom: 10px;'>TERMINALE DI SICUREZZA - SALA SERVER</h1>");
            
            client.println("<div style='border: 1px solid #30363d; padding: 20px; background-color: #161b22;'>");
            client.println("<h2 style='color: #c9d1d9;'>RILEVAMENTO INTRUSIONI</h2>");
            
            if (motionState == HIGH) {
              client.println("<p style='font-size: 20px;'><b>[!] Sensore Volumetrico:</b> <span style='background-color: #da3633; color: white; padding: 5px;'>INTRUSIONE RILEVATA! (Movimento)</span></p>");
            } else {
              client.println("<p style='font-size: 20px;'><b>[+] Sensore Volumetrico:</b> <span style='color: #3fb950;'>Area Sicura</span></p>");
            }

            if (rumoreRilevato) {
              client.println("<p style='font-size: 20px;'><b>[!] Sensore Acustico:</b> <span style='background-color: #da3633; color: white; padding: 5px;'>ALLARME! RUMORE SOSPETTO!</span></p>");
              rumoreRilevato = false; 
            } else {
              client.println("<p style='font-size: 20px;'><b>[+] Sensore Acustico:</b> <span style='color: #3fb950;'>Silenzio (Normale)</span></p>");
            }
            client.println("</div>");
            
            client.println("<div style='border: 1px solid #30363d; padding: 20px; background-color: #161b22; margin-top: 20px;'>");
            client.println("<h2 style='color: #c9d1d9;'>TELEMETRIA DI RETE & HIDS</h2>");
            client.print("<p><b>Uptime Nodo:</b> "); client.print(millis() / 1000); client.println(" sec</p>");
            client.print("<p><b>Traffico Web (Req/Sec):</b> "); client.print(reqCount); client.println("</p>");
            client.print("<p><b>Stato HIDS:</b> <span style='color: #3fb950;'>Attivo (Sicuro)</span></p>");
            
            client.println("<hr style='border-color: #30363d; margin-top:20px; margin-bottom:20px;'>");
            client.println("<a href='/spegni' style='text-decoration:none;'><button style='background-color:#da3633; color:white; font-weight:bold; padding:10px 20px; border:none; border-radius:5px; cursor:pointer;'>SPEGNI DASHBOARD</button></a>");
            
            client.println("</div></body></html>");
          }
          break;
        }
        if (c == '\n') { currentLineIsBlank = true; } else if (c != '\r') { currentLineIsBlank = false; }
      }
    }
    delay(1); client.stop();
    if (!underAttack) digitalWrite(LEDB, HIGH); 
  }
}