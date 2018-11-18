/*
 * 
 * lora-words: simple sketch to illustrate sending and receiving
 * data over LoRa, in addition to using WiFi, getting the current 
 * time from NTP, and displaying text and images on an OLED screen.
 * 
 * The same code contains both sender and receiver, with
 * WORDS_SENDER defining what the built code will do.
 * 
 * In sender mode, one of a list of 100 random words will be 
 * sent over LoRa.
 * 
 * In receive mode, the code listens for a random word, displaying
 * it on the screen, along with the LoRa RSSI, the WiFi signal 
 * strength, and the current time (from NTP).
 * 
 * The code is based largely on 'WiFi_LoRa_32FactoryTest.ino'
 * and other samples.
 * 
 * Code has only been tested on a Heltec LoRa WiFi OLED v2 module.
 * 
 * IMPORTANT: change the setting of BAND to suit your location.
 * 
 */

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <time.h>
#include <ESPmDNS.h>

#include "SSD1306.h"
#include "WiFi.h"
#include "font_Dialog_plain_20.h"
#include "words.h"
#include "signal_strength_xbm.h"
#include "wifi_strength_xbm.h"
#include "wifi_config.h"		// WiFi SSID and password.

// Pin definetion of WIFI LoRa 32
// HelTec AutoMation 2017 support@heltec.cn
#define SCK     5    // GPIO5  -- SX127x's SCK
#define MISO    19   // GPIO19 -- SX127x's MISO
#define MOSI    27   // GPIO27 -- SX127x's MOSI
#define SS      18   // GPIO18 -- SX127x's CS
#define RST     14   // GPIO14 -- SX127x's RESET
#define DI0     26   // GPIO26 -- SX127x's IRQ(Interrupt Request)
#define SDA      4
#define SCL     15
#define RSTOLED  16   //RST must be set by software
#define Light  25
#define V2  1

#ifdef V2 //WIFI Kit series V1 not support Vext control
  #define Vext  21
#endif

#define BAND    915E6 //868E6  //you can set band here directly,e.g. 868E6,915E6
#define PABOOST true

#define DISPLAY_HEIGHT 64
#define DISPLAY_WIDTH  128

#define MIN_WAIT_SECS		2*1000
#define WAIT_VARIANCE_SECS	2*1000

#define RSSI_NO_SIGNAL		(-999)

#define UTC_OFFSET 		10*60*60
#define NTP_UTC_OFFSET 	10*60*60
#define NTP_SERVER		"time.iinet.net.au" // Local NTP server.

#define WORDS_SENDER	false	//true

#define WORDS_BUILD_DATE (__DATE__ __TIME__)
#define WORDS_BUILD_FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

String rssi = "RSSI --";
String packSize = "--";
String packet;

unsigned int counter = 0;

bool receiveflag = false; // software flag for LoRa receiver, received data makes it true.

long lastSendTime = 0;        // last send time
long lastReceiveTime = 0;     // last packet received
int interval = 1000;          // interval between sends

bool use_display = false;
bool words_sender = WORDS_SENDER;

bool receive_mutex = false;

char time_buffer[9] = ""; // HH:MM:SS

int received_signal_strength = RSSI_NO_SIGNAL;

int ntp_offset = 0;

bool screen_up = false;
bool wifi_up = false;
bool ntp_up = false;
bool lora_up = false;
bool mdns_up = false;

char hostname[64] = "";


SSD1306  display(0x3c, SDA, SCL, RSTOLED);

void show_startup_screen(String line1, String line2, String line3, String line4, String line5, int progress = -1){

	if (!use_display){
		return;
	}

	display.clear();
	display.setFont(ArialMT_Plain_10);
	display.drawString(0, 0, line1);
	display.drawString(0, 10, line2);
	display.drawString(0, 20, line3);
	display.drawString(0, 30, line4);
	display.drawString(0, 40, line5);

	if (-1 != progress){
		display.drawProgressBar(0, 55, 120, 8, progress); 
	}

	display.display();

	return;
}

void show_splash_screen(String info){

	if (!use_display) {
		return;
	}

	display.clear();

	display.setColor(WHITE); 
	display.drawProgressBar(0, 0, DISPLAY_WIDTH, 25, 100); 

	display.setColor(BLACK);
	display.setFont(Dialog_plain_20);
	display.setTextAlignment(TEXT_ALIGN_CENTER);
	display.drawString(DISPLAY_WIDTH / 2, 0, "bimblers");


	display.setColor(WHITE);
	display.setFont(ArialMT_Plain_10);
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.drawString(0, 30, WORDS_BUILD_FILE);
	display.drawString(0, 40, WORDS_BUILD_DATE);

	display.drawString(0, 50, info);

	display.display();

	delay (4000);

	return;
}

/*
 * Return a bitmap representing the WiFi RSSI.
 * 
 * Lookups taken from:
 * 	https://www.adriangranados.com/blog/dbm-to-percent-conversion
 * 
 */
unsigned char* wifi_rssi_xbm (int rssi) {

	unsigned char* xbm = wifi_image_0;

/*	if (rssi >= -90) {
		xbm = wifi_image_20;
	}*/

	if (rssi >= -80) {
		xbm = wifi_image_40;
	}

	if (rssi >= -70) {
		xbm = wifi_image_60;
	}

	if (rssi >= -55) {
		xbm = wifi_image_80;
	}

	if (rssi >= -45) {
		xbm = wifi_image_100;
	}

	return xbm;
}


/*
 * Return a bitmap representing the LoRa RSSI.
 * 
 * Note that various lookups exist on the internet, but it seems
 * that one of the key features of LoRa is that signals can be usefully 
 * received at very low RSSI values.
 * 
 */
unsigned char* lora_rssi_xbm (int rssi) {

	unsigned char* xbm = signal_image_0;


	// i.e. we will show a 20% signal strength at RSSI of -130 or above.
	// This oroughly corresponds to the weakest RSSI which will allow
	// data to be transmitted and received.
	if (rssi >= -130) {
		xbm = signal_image_20;
	}

	if (rssi >= -110) {
		xbm = signal_image_40;
	}

	if (rssi >= -90) {
		xbm = signal_image_60;
	}

	if (rssi >= -80) {
		xbm = signal_image_80;
	}

	if (rssi >= -60) {
		xbm = signal_image_100;
	}

	return xbm;
}

void check_time (){

	struct tm tmstruct;

	if(!ntp_up) {
		return;
	}

	getLocalTime(&tmstruct);

    sprintf(time_buffer, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

	return;
}

void display_gui(String text, int ssi){

	char buffer[100];

	if (!use_display) {
		return;
	}

	// Don't update display whist we're processing a packet receive event, as
	// the buffer may still be being filled.
	// There are probably better mechanisms for this, but I'm still learning.
	if (receive_mutex){
		return;
	}

	display.clear();
	display.setColor(WHITE);

	display.setFont(Dialog_plain_20);

	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.drawString(0, 30, text);

	if (ssi != RSSI_NO_SIGNAL) {
		display.setFont(ArialMT_Plain_10);
		display.drawString(0, 50, "RSSI: " + String(ssi));
	}

  	display.drawXbm(DISPLAY_WIDTH - SIGNAL_IMAGE_WIDTH, 0, SIGNAL_IMAGE_WIDTH, SIGNAL_IMAGE_HEIGHT, lora_rssi_xbm(ssi));

	display.setFont(ArialMT_Plain_10);
	display.drawString(0, 0, String(time_buffer));

  	display.drawXbm(DISPLAY_WIDTH - SIGNAL_IMAGE_WIDTH - WIFI_IMAGE_WIDTH, 0, SIGNAL_IMAGE_WIDTH, SIGNAL_IMAGE_HEIGHT, wifi_rssi_xbm(WiFi.RSSI()));

	display.display();

	return;
}

void setup_ntp() {

	if (!wifi_up) {
		return;
	}

	Serial.println("Contacting Time Server");

	show_startup_screen("Contacting time server...", "", "", "", "", 30);

	configTime(NTP_UTC_OFFSET, 0, NTP_SERVER, "0.pool.ntp.org", "1.pool.ntp.org");

	delay(2000);

	if (!time(nullptr)){
		show_startup_screen("Contacting time server...", "FAILED", "", "", "", 40);
	}
	else {
		show_startup_screen("Contacting time server...", "Succeeded", "", "", "", 50);
	}

	check_time();

	delay(1000);

	ntp_up = true;

	return;
}

bool setup_mdns(char* host){

	show_startup_screen("Registering as", String(host) + "...", "", "", "", 60);

    if (!MDNS.begin(host)) {

		show_startup_screen("Registering as", String(host) + "...", "FAILED", "", "", 70);
	
        Serial.println("Error setting up MDNS responder!");

		delay(1000);

		return false;
    }

	show_startup_screen("Registering as", String(host) + "...", "Succeeded", "", "", 70);

	mdns_up = true;

	delay(1000);

	return true;
}

void setup_wifi(void)
{
	byte count = 0;
	uint64_t chipid;

	show_startup_screen("Connecting to WiFi...", "", "", "", "", 10);

	// Note that code using getChipID wouldn't compile for me.
	chipid=ESP.getEfuseMac();	//The chip ID is its MAC address (length: 6 bytes).

	sprintf (hostname, "LORA%02X%02X", ((uint16_t)(chipid>>32) & 0x00ff), ((uint16_t)(chipid>>32) >> 8));

	Serial.println("Setting hostname to: " + String(hostname));

	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);

	delay(1000);

	WiFi.setHostname(hostname);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);

	WiFi.begin(WIFI_SSID,WIFI_PWD);  // Stored in wifi_config.h.

	delay(100);

	while(WiFi.status() != WL_CONNECTED && count < 10)
	{
		count ++;
		delay(500);
	}

	if(WiFi.status() == WL_CONNECTED)
	{
		show_startup_screen("Connected to WiFi.", "SSID:", WiFi.SSID(), "IP address:", String(WiFi.localIP().toString()), 20);
	}
	else // Connect failed.
	{
		show_startup_screen("Connecting to WiFi...", "FAILED", "", "", "", 20);
    	delay (2000);
	}

	delay(2000);

	wifi_up = true;

	return;
}

void setup_screen (){

	pinMode(Vext,OUTPUT);
	digitalWrite(Vext, LOW);    //// OLED USE Vext as power supply, must turn ON Vext before OLED init
	delay(50);

	display.init();
	display.flipScreenVertically();
	display.setFont(ArialMT_Plain_10);

	display.clear();

	screen_up = true;

	return;
}

void setup_lora(){

	show_startup_screen("Starting LoRa...", "", "", "", "", 80);

	SPI.begin(SCK,MISO,MOSI,SS);

	LoRa.setPins(SS,RST,DI0);

	if (!LoRa.begin(BAND,PABOOST))
	{
		Serial.println("LoRa initialise failed");

		show_startup_screen("Starting LoRa...", "FAILED", "", "", "", 100);

		while (1);
	}

	show_startup_screen("Starting LoRa...", "Succeeded", "", "", "", 100);
	Serial.println("LoRa initialised.");

	delay(500);

	lora_up = true;

	return;
}

void light_on(){
	digitalWrite(Light,HIGH);
}

void light_off(){
	digitalWrite(Light,LOW);
}

void send_word(){

	char random_word[100] = "";
	
	strcpy(random_word, word_array[random(100-1)]); // Arrays are zero-indexed.

	LoRa.beginPacket();
	LoRa.print(String(random_word));
	LoRa.print(counter++);
	LoRa.endPacket();

	// Put radio back into receive mode (not really needed here).
	LoRa.receive();

	Serial.println ("Packet " + (String)(counter-1) + " (" + random_word + ") sent.");

	if (use_display){
		display.clear();
		display.drawString(0, 50, "Packet " + (String)(counter-1) + " (" + random_word + ") sent.");
		display.display();
	}

	return;
}

void setup()
{
	Serial.begin(115200);
	while (!Serial);
	Serial.println("Starting...");

	pinMode(Light,OUTPUT);

	if(!words_sender) {
		use_display = true;
	}

	// PP override.
//	use_display = true;

	Serial.println("lora-words");

	// Only use the display if we're receiving.
	if(use_display) {
		setup_screen();
	}

	// Show the splash screen.
	if(words_sender){
		show_splash_screen("-> sender");
	}
	else {
		show_splash_screen("-> receiver");
	}


	if (!words_sender){
		setup_wifi();
		setup_ntp();
		setup_mdns(hostname);
	}

	setup_lora();

	if (!words_sender) {

		Serial.println("Setting radio to receive mode.");

		// register the receive callback
		LoRa.onReceive(onReceive);

		// put the radio into receive mode
		LoRa.receive();
	}

	if(use_display) {

		check_time();

		display_gui("Waiting...", RSSI_NO_SIGNAL);
	}
}

void loop()
{
	// Make the LED blink.
	light_on();

	// Sender.
	if (words_sender){

		// Do we need to send yet?
		if(millis() - lastSendTime > interval)//waiting LoRa interrupt
		{
			send_word();

			// Set up the random delay.
			interval = random(MIN_WAIT_SECS) + WAIT_VARIANCE_SECS;

			lastSendTime = millis();
		}
	}

	// Receiver.
	if (!words_sender){
		check_time();

		// Just started.
		if (RSSI_NO_SIGNAL == received_signal_strength) {

			display_gui("Waiting...", RSSI_NO_SIGNAL);

		} 
		else {
			// Timeout on receiving packets.
			if(millis() - lastReceiveTime > (2 * (MIN_WAIT_SECS	+ WAIT_VARIANCE_SECS))){

				display_gui("No signal.", RSSI_NO_SIGNAL);

			}
			else {

				// Most cases - refresh the display.
				display_gui(packet, received_signal_strength);
			}
		}

	}

	// Make the LED blink.
	light_off();

	delay(200);
}

void onReceive(int packetSize)//LoRa receiver interrupt service
{
	//if (packetSize == 0) return;

	packet = "";
    packSize = String(packetSize,DEC);

	receive_mutex = true;

    while (LoRa.available())
    {
		packet += (char) LoRa.read();
    }

	lastReceiveTime = millis();

    Serial.println(packet);
    rssi = "RSSI: " + String(LoRa.packetRssi(), DEC);

	received_signal_strength = LoRa.packetRssi();

	receive_mutex = false;

    receiveflag = true;
}
