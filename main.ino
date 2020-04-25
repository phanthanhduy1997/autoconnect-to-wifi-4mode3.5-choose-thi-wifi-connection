#include <FS.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266TimeAlarms.h>
#include <string.h>
#include <stdlib.h>

#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>



byte willQoS = 1; // gui tin hieu len sever khi mat ket noi
const char *willTopic = "connected/192168110";
const char *willMessage = "0";
boolean willRetain = false;

const char *device_serial_number = "192168110";
const bool ON = true;
const bool OFF = false;
const int button = D2;  //khai bao button reset
const int ledwifi = D3;   //khai bao den led wifi
const int ledreset = D4;   //khai bao den led wifi
const int outputpin = A0; //Khai bao chan doc nhiet do
const int Relay = D8;     //Khai bao chan output vao relay
const int MODE_MANUAL = 1;
const int MODE_AUTO_BALANCE = 2;
const int MODE_CYCLE = 3;
const int MODE_DATETIME = 4;

bool relay_status = OFF;
bool is_set_alarm_cycle = false;
bool is_set_alarm_date_time = false;

// // Wifi config
// const char *ssid = "My Friend";
// const char *password = "khongcopass";
String ssid = WiFi.SSID();
String password = WiFi.psk();
char saved_ssid[100] = "";
char saved_psk[100] = "";
const char *mqtt_server = "farm-dev.affoz.com";

AlarmId id;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int mode = 1;
float nhietmax = 0;
float nhietmin = 0;
bool chedo = true;
float celsius = 0;
int solan = 0;
int value = 0;

long cy_run_time = 0;
long cy_rest_time = 0;
long cy_apply_datetime = 0;
long cy_stop_datetime = 0;
long cy_repeat = 0;
const char* AP_Name = "ez1111";
String mon;
String tue;
String wed;
String thu;
String fri;
String sat;
String sun;
const timeDayOfWeek_t weekly_Arlarm_name[7] = {dowMonday, dowTuesday, dowWednesday, dowThursday, dowFriday, dowSaturday, dowSunday};
const char* weekly_time_name[7] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

char json[550] = "{\"mode\":1,\"limit\":{\"min\":1235,\"max\":1234,\"mode_on\":false}}";


//     {"mode":2,"setting":{"min":20,"max":33,"mode_on":true}}
//{"mode":3,"setting":{"run_time":7,"rest_time":3,"apply_datetime":1587124290,"stop_datetime":1587124290,"repeat":100}}
//{"mode":4,"setting":{"mon":"'08:00', '00:00', '22:00'","tue":"'08:00', '00:00', '22:00'","tue":"'08:00', '00:00', '22:00'","wed":"'08:00', '00:00', '22:00'","fri":"'08:00', '00:00', '22:00'","sat":"'08:00', '00:00', '22:00'","sun":"'08:00', '00:00', '22:00'","apply_datetime":1587124290,"stop_datetime":1587124290,"repeat":100}}
//{"mode":4,"setting":{"mon":[["11:59","12:12"],["11:59","12:12"],["18:00","19:00"]],"tue":[["11:59","12:12"],["11:59","12:12"],["18:00","19:00"]],"wed":[["12:04","12:12"],["11:59","12:12"],["18:00","19:00"]],"thu":[["11:59","12:12"],["11:59","12:12"],["18:00","19:00"]],"fri":[["11:59","12:12"],["11:59","12:12"],["18:00","19:00"]],"sat":[["11:59","12:12"],["11:59","12:12"],["18:00","19:00"]],"sun":[["11:59","12:12"],["11:59","12:12"],["18:00","19:00"]],"apply_datetime":1587124290,"stop_datetime":1587124290,"repeat":100}}
//      http://www.hivemq.com/demos/websocket-client/?#data

/**
 * settup function
 **/
bool shouldSaveConfig = false;
bool forceConfigPortal = false;
WiFiManager wifiManager;

// Callback notifying us of the need to save config
void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}
    void setup()
    {
        pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output
        pinMode(Relay, OUTPUT);       // Khai bao chan output cua relay
        pinMode(button, INPUT);       //Khai bao button
        pinMode(ledwifi, OUTPUT);     //Khai bao led wifi
        pinMode(ledreset, OUTPUT);
        digitalWrite(Relay, LOW);     //  Relay on 5V
        digitalWrite(button, LOW);
         digitalWrite(ledreset, LOW);
        Serial.begin(115200);
        setup_wifi();
        client.setServer(mqtt_server, 1883);
        client.setCallback(mqttCallback);
    }
    void setup_wifi()
    {

        for (int i = 0; i < 5; i++)
        {
            Serial.print(".");
            delay(1000);
        }
        wifiManager.setSaveConfigCallback(saveConfigCallback);
        
        if(forceConfigPortal) {
    wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    // Force config portal while jumper is set for flashing
     if (!wifiManager.startConfigPortal(AP_Name)) {
          Serial.println("** ERROR ** Failed to connect with new config / possibly hit config portal timeout; Resetting in 3sec...");
          delay(3000);
          //reset and try again, or maybe put it to deep sleep
          ESP.reset();
          delay(5000);
        }
  } else {
    // Autoconnect if we can
    
    // Fetches ssid and pass and tries to connect; if it does not connect it starts an access point with the specified name
    // and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect(AP_Name)) {
      Serial.println("** ERROR ** Failed to connect with new config / possibly hit timeout; Resetting in 3sec...");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }
  WiFi.SSID().toCharArray(saved_ssid, 100);
  WiFi.psk().toCharArray(saved_psk, 100);

    }
    /**
 * Loop function
 **/void reconnectIfNecessary() {
  while(WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected; Attempting reconnect to " + String(saved_ssid) + "...");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(saved_ssid, saved_psk);
    // Output reconnection status info every second over the next 10 sec
    for( int i = 0; i < 10 ; i++ )  {
      delay(1000);
      Serial.print("WiFi status = ");
      if( WiFi.status() == WL_CONNECTED ) {
        Serial.println("Connected");
        break;
      } else {
           digitalWrite(ledwifi, LOW);
        Serial.println("Disconnected");
      }
    }
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("Failure to establish connection after 10 sec. Will reattempt connection in 2 sec");
       digitalWrite(ledwifi, LOW);
      delay(2000);
    }
  }
}
    void loop()
    {
        if (digitalRead(button) == HIGH)
        {
            digitalWrite(ledreset, HIGH);
             digitalWrite(ledwifi, LOW);
            Serial.println("nut nhan reset wifi da duoc nhan");
            wifiManager.resetSettings();
            setup_wifi();
        }else
        {
            digitalWrite(ledreset, LOW);
        }
        

        reconnectIfNecessary();
        if (!client.connected())
        {
            digitalWrite(ledwifi, LOW);
            reconnect();
        }
        client.loop();
        // publish temperature after 3 seconds
        unsigned long now = millis();
        if (now - lastMsg > 3000)
        {
            lastMsg = now;
            ++value;
            int analogValue = analogRead(outputpin);
            float millivolts = (analogValue / 1024.0) * 3300; //3300 is the voltage provided by NodeMCU
            float nearest = roundf(millivolts * 100) / 100;
            celsius = nearest / 10;

            snprintf(msg, MSG_BUFFER_SIZE, "%.2f", celsius);
            Serial.print("Publish message: ");
            Serial.println(msg);
            Serial.println(celsius);

            publish("sensor", msg);
            // digitalWrite(button, HIGH);
            digitalWrite(ledwifi, HIGH);
            Serial.println(mode);
        }

        if (mode == MODE_MANUAL)
        {
            // CODE MODE MANUAL HERE
        }
        else if (mode == MODE_AUTO_BALANCE)
        {
            if (chedo == false)
            {
                if (celsius > nhietmax || celsius < nhietmin)
                {
                    switchRelay(ON);
                }
                else
                {
                    switchRelay(OFF);
                }
            }
            if (chedo == true)
            {
                if (celsius < nhietmax && celsius > nhietmin)
                {
                    switchRelay(ON);
                }
                else
                {
                    switchRelay(OFF);
                }
            }
        }
        else if (mode = MODE_CYCLE)
        {
            // CODE MODE CYCLE HERE
            // digitalClockDisplay();
            Alarm.delay(200);
        }
        else if (mode == MODE_DATETIME)
        {

            // CODE MODE DATETIME HERE
            // digitalClockDisplay();
            // Alarm.delay(1000);
            Alarm.delay(200);
            //    Alarm.alarmRepeat(11, 56, 0, MorningAlarm);
            //    Alarm.alarmRepeat(11, 57, 30, EveningAlarm);

            //  is_set_alarm_date_time = true;
            // Alarm.timerOnce(10, RunCycleAlarm);
        }
    }

    /**
 * subscribe helper
 **/
    void subscribe(char *subtopic)
    {
        char topic[strlen(subtopic) + strlen(device_serial_number) + 1];
        sprintf(topic, "%s%s%s", subtopic, "/", device_serial_number);
        client.subscribe(topic);
        Serial.print("subscribed to topic: ");
        Serial.println(topic);
    }

    /**
 * publish helper
 **/
    void publish(char *subtopic, char *msg)
    {
        char topic[strlen(subtopic) + strlen(device_serial_number) + 1];
        sprintf(topic, "%s%s%s", subtopic, "/", device_serial_number);
        client.publish(topic, msg);
        Serial.print("published to topic: ");
        Serial.print(topic);
        Serial.print("; with message: ");
        Serial.println(msg);
    }

    /**
 * use to turn on/off relay
 * 
 **/
    void switchRelay(bool status)
    {
        if (relay_status != status)
        {
            if (status)
            {
                digitalWrite(Relay, HIGH);
                publish("turned_on", "1");
                Serial.println("relay's turned on");
            }
            else
            {
                digitalWrite(Relay, LOW);
                publish("turned_on", "0");
                Serial.println("relay's turned off");
            }
            relay_status = status;
        }
    }

    /**
 * 
 * 
 *
 **/
    void mqttCallback(char *topic, byte *payload, unsigned int length)
    {
        Serial.print("Message arrived [");
        Serial.print(topic);
        Serial.print("] ");
        Serial.print(length);
        Serial.println((char *)payload);
        if (String(topic).startsWith("setting") && payload[0] == '{')
        {

            char *json = (char *)payload;
            StaticJsonBuffer<2000> jsonBuffer;
            JsonObject &root = jsonBuffer.parseObject(json);
            mode = root["mode"];
            Serial.println(mode);
            Serial.println(mode);
            Serial.println(mode);
            Serial.println(mode);
            Serial.println(mode);

            switch (mode)
            {
            case MODE_MANUAL:
                Serial.println("bat che do THU CONG");

                break;
            case MODE_AUTO_BALANCE:
            {
                Serial.println("bat che do TU DONG");
                float min = root["setting"]["min"];
                float max = root["setting"]["max"];
                bool mode_on = root["setting"]["mode_on"];

                Serial.println(mode);
                Serial.println(min);
                Serial.println(max);
                Serial.println(mode_on);

                chedo = mode_on;
                nhietmax = max;
                nhietmin = min;
                Serial.println(nhietmax);
                Serial.println(nhietmin);
            }
            break;

            case MODE_CYCLE:
            {
                Serial.println("da bat mode 3");
                cy_run_time = root["setting"]["run_time"];
                cy_rest_time = root["setting"]["rest_time"];
                cy_apply_datetime = root["setting"]["apply_datetime"];
                cy_stop_datetime = root["setting"]["stop_datetime"];
                cy_repeat = root["setting"]["repeat"];
                Serial.println(cy_run_time);
                Serial.println(cy_rest_time);
                Serial.println(cy_apply_datetime);
                Serial.println(cy_stop_datetime);
                Serial.println(cy_repeat);

                Alarm.timerOnce(cy_apply_datetime - time(nullptr), RunCycleAlarm); // hen gio BAT  che do cycle time
                Alarm.timerOnce(cy_stop_datetime - time(nullptr), StopcycleMode);  // hen gio TAT che do cycle time
                Serial.println(cy_apply_datetime - time(nullptr));
                is_set_alarm_cycle = false;
                Serial.println("bat che do CYCLE TIME");
            }
            break;

            case MODE_DATETIME:
            {
                is_set_alarm_date_time = false;
                Serial.println("bat che do REAL TIME");

                long rt_apply_datetime = root["setting"]["apply_datetime"];
                long rt_stop_datetime = root["setting"]["stop_datetime"];
                int rt_repeat = root["setting"]["repeat"];

                Serial.println(rt_apply_datetime);
                Serial.println(rt_stop_datetime);
                Serial.println(rt_repeat);

                for (int week_index = 0; week_index < 7; week_index++)
                {
                    int j = 0;
                    while (root["setting"][weekly_time_name[week_index]][j])
                    {
                        for (int k = 0; k <= 1; k++)
                        {
                            String hourString = root["setting"][weekly_time_name[week_index]][j][k];
                            int hour = hourString.substring(0, 2).toInt();
                            int minute = hourString.substring(3, 5).toInt();

                            Serial.print(hour);
                            Serial.print(minute);

                            id = Alarm.alarmRepeat(
                                weekly_Arlarm_name[week_index],
                                hour,
                                minute,
                                0,
                                k == 0 ? RunCycleAlarm : StopCycleAlarm

                            );
                        }
                        // stop
                        j++;
                    }
                }
                Alarm.timerOnce(rt_apply_datetime, StopCycleAlarm);

                is_set_alarm_date_time = false;
            }
            break;
            }
        }
        if (mode == 1 && payload[0] == '1')
        {
            switchRelay(ON);
        }
        if (mode == 1 && payload[0] == '0')
        {
            switchRelay(OFF);
        }
    }

    /**
 * 
 * 
 *
 **/
    void reconnect()
    {
        // Loop until we're reconnected
        while (!client.connected())
        {

            digitalWrite(ledwifi, LOW); //neu mat ket noi thi den tat
            Serial.print("Attempting MQTT connection...");
            // Create a random client ID
            String clientId = "ESP8266Client-";
            clientId += String(random(0xffff), HEX);
            // Attempt to connect
            if (client.connect(clientId.c_str(), willTopic, willQoS, willRetain, willMessage))
            {
                Serial.println("connected");
                publish("connected", "1");                   //neu ket noi dc thi gui len sever connected/192168110 [1]
                Serial.println("da ket noi duoc voi sever"); //neu ket noi dc thi gui len sever connected/192168110 [1]
                // Once connected, publish an announcement...

                // ... and resubscribe
                subscribe("setting");
                subscribe("turn_on");
            }
            else
            {
                Serial.print("failed, rc=");
                Serial.print(client.state());
                Serial.println(" try again in 5 seconds");
                // Wait 5 seconds before retrying
                delay(5000);
            }
        }
    }
    void set_real_time_start()
    {
    }

    /**
 * 
 * 
 *
 **/
    void StopcycleMode()
    {
        Serial.println("Alarm:         TAT");
        switchRelay(OFF);
        // mode = 1;
        cy_repeat = 0;
    }
    /**
 * 
 * 
 *
 **/
    void RunCycleAlarm()
    {
        Serial.println("Alarm:         Bat");
        switchRelay(ON);

        if (mode == 3)
            Alarm.timerOnce(cy_run_time, StopCycleAlarm);
    }

    /**
 * 
 * 
 *
 **/
    void StopCycleAlarm()
    {
        Serial.println("Alarm:        Tat");
        switchRelay(OFF);
        cy_repeat--;
        if (mode == 3 && cy_repeat > 0)
        {
            Alarm.timerOnce(cy_rest_time, RunCycleAlarm);
        }
    }

    /**
 * 
 * 
 *
 **/
    void MorningAlarm()
    {
        Serial.println("Alarm:         HOAT DONG");
    }

    /**
 * 
 * 
 *
 **/
    void EveningAlarm()
    {
        Serial.println("Alarm: - turn lights on");
        switchRelay(OFF);
    }

    /**
 * 
 * 
 *
 **/
    void WeeklyAlarm()
    {
        Serial.println("Alarm: - its Monday Morning");
    }

    /**
 * 
 * 
 *
 **/
    void ExplicitAlarm()
    {
        Serial.println("Alarm: - this triggers only at the given date and time");
    }

    /**
 * 
 * 
 *
 **/
    void Repeats()
    {
        Serial.println("15 second timer");
    }

    /**
 * 
 * 
 *
 **/
    void Repeats2()
    {
        Serial.println("2 second timer");
    }

    /**
 * 
 * 
 *
 **/
    void OnceOnly()
    {
        Serial.println("This timer only triggers once, stop the 2 second timer");
        // use Alarm.free() to disable a timer and recycle its memory.
        Alarm.free(id);
        // optional, but safest to "forget" the ID after memory recycled
        id = dtINVALID_ALARM_ID;
        // you can also use Alarm.disable() to turn the timer off, but keep
        // it in memory, to turn back on later with Alarm.enable().
    }

    /**
 * 
 * 
 *
 **/
    void digitalClockDisplay()
    {
        time_t tnow = time(nullptr);
        Serial.println(tnow);
        Serial.println(ctime(&tnow));

        //    Serial.println(ctime(&tnow));
    }
