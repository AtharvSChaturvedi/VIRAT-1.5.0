//V.I.R.A.T.(Visual Intelligent Routine Assistant for Tasks)
#define BLYNK_TEMPLATE_ID "TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "VIRAT Study Automation"
#define BLYNK_AUTH_TOKEN "BLYNK_TOKEN_NUM"

// Comment this out to disable prints and save space
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Your WiFi credentials
char ssid[] = "SSID";
char pass[] = "PASSWORD";
char auth[] = BLYNK_AUTH_TOKEN;

// Pins
#define pirSensorPin 12
#define ledPin1 14
#define ledPin2 13
#define buttonPin 27

// Blynk Virtual Pins
#define VPIN_TASK1_NAME V1
#define VPIN_TASK1_HOURS V2
#define VPIN_TASK1_MINUTES V3
#define VPIN_TASK2_NAME V4
#define VPIN_TASK2_HOURS V5
#define VPIN_TASK2_MINUTES V6
#define VPIN_TASK3_NAME V7
#define VPIN_TASK3_HOURS V8
#define VPIN_TASK3_MINUTES V9
#define VPIN_TASK4_NAME V10
#define VPIN_TASK4_HOURS V11
#define VPIN_TASK4_MINUTES V12
#define VPIN_TASK5_NAME V13
#define VPIN_TASK5_HOURS V14
#define VPIN_TASK5_MINUTES V15
#define VPIN_UPDATE_SCHEDULE V16
#define VPIN_RESET_TASKS V17
#define VPIN_CURRENT_TASK V18
#define VPIN_SYSTEM_STATUS V19
#define VPIN_WIFI_STATUS V20

// Task structure with duration
struct Task {
  String name;
  int durationHours;
  int durationMinutes;
  bool completed;
  bool active;
  unsigned long startTime;
};

// Schedule array - Default tasks with proper names
Task schedule[] = {
  {"Task 1", 0, 0, false, false, 0},
  {"Task 2", 0, 0, false, false, 0},
  {"Task 3", 0, 0, false, false, 0},
  {"Task 4", 0, 0, false, false, 0},
  {"Task 5", 0, 0, false, false, 0}
};

const int numTasks = sizeof(schedule) / sizeof(schedule[0]);

// System states
enum SystemState {
  IDLE,
  SHOWING_SCHEDULE,
  TASK_ACTIVE,
  TASK_TIMER_WARNING
};

SystemState currentState = IDLE;
bool ledState = false;
bool displayAvailable = false;
bool displayVersionToggle = false;
bool wifiConnected = false;
bool initialSyncComplete = false; // ADD THIS FLAG
int currentTaskIndex = -1;
int scheduleDisplayIndex = 0;
int currentTaskOnDisplay = -1;

unsigned long lastMotionTime = 0;
unsigned long lastButtonPress = 0;
unsigned long lastScheduleScroll = 0;
unsigned long lastTimerCheck = 0;
unsigned long lastBlynkUpdate = 0;

const unsigned long cooldown = 10000;
const unsigned long buttonDebounce = 500;
const unsigned long scheduleScrollDelay = 3000;
const unsigned long timerCheckInterval = 1000;
const unsigned long blynkUpdateInterval = 5000;
const int calibrationTime = 10;

// Function declarations
void updateDisplay(const String& line1, const String& line2 = "", const String& line3 = "");
void printScheduleToSerial();
void updateBlynkStatus();
void sendScheduleToBlynk();
void showScheduleOnDisplay();

// Blynk Virtual Pin Handlers

// Update Schedule Button - MODIFIED
BLYNK_WRITE(V16) {
  if (param.asInt()) {
    Serial.println("Manual schedule sync requested from Blynk...");
    updateDisplay("Syncing", "schedule...", "");
    delay(1000);
    
    // Only send current schedule to Blynk, don't override from default
    // This button should be used to push current ESP32 schedule to app
    sendScheduleToBlynk();
    
    if (currentState == IDLE) {
      updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
    }
    
    Serial.println("Schedule sync completed!");
  }
}

// Reset Tasks Button
BLYNK_WRITE(V17) {
  if (param.asInt()) {
    Serial.println("Resetting all tasks from Blynk...");
    
    for (int i = 0; i < numTasks; i++) {
      schedule[i].completed = false;
      schedule[i].active = false;
      schedule[i].startTime = 0;
    }
    
    currentState = IDLE;
    currentTaskIndex = -1;
    digitalWrite(ledPin1, LOW);
    digitalWrite(ledPin2, LOW);
    updateDisplay("Tasks Reset", "from Blynk!", "");
    delay(2000);
    updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
    
    updateBlynkStatus();
    Serial.println("All tasks reset from Blynk!");
  }
}

// Task Name and Duration Handlers - IMPROVED
BLYNK_WRITE(V1) { 
  String newName = param.asStr();
  if (newName.length() > 0 && newName != schedule[0].name) {
    schedule[0].name = newName;
    Serial.println("Task 1 name updated from Blynk: " + newName);
    if (currentState == SHOWING_SCHEDULE) {
      showScheduleOnDisplay();
    }
  }
}
BLYNK_WRITE(V2) { 
  int newHours = param.asInt();
  if (newHours != schedule[0].durationHours) {
    schedule[0].durationHours = newHours;
    Serial.println("Task 1 hours updated from Blynk: " + String(newHours));
  }
}
BLYNK_WRITE(V3) { 
  int newMinutes = param.asInt();
  if (newMinutes != schedule[0].durationMinutes) {
    schedule[0].durationMinutes = newMinutes;
    Serial.println("Task 1 minutes updated from Blynk: " + String(newMinutes));
  }
}

BLYNK_WRITE(V4) { 
  String newName = param.asStr();
  if (newName.length() > 0 && newName != schedule[1].name) {
    schedule[1].name = newName;
    Serial.println("Task 2 name updated from Blynk: " + newName);
    if (currentState == SHOWING_SCHEDULE) showScheduleOnDisplay();
  }
}
BLYNK_WRITE(V5) { 
  int newHours = param.asInt();
  if (newHours != schedule[1].durationHours) {
    schedule[1].durationHours = newHours;
    Serial.println("Task 2 hours updated from Blynk: " + String(newHours));
  }
}
BLYNK_WRITE(V6) { 
  int newMinutes = param.asInt();
  if (newMinutes != schedule[1].durationMinutes) {
    schedule[1].durationMinutes = newMinutes;
    Serial.println("Task 2 minutes updated from Blynk: " + String(newMinutes));
  }
}

BLYNK_WRITE(V7) { 
  String newName = param.asStr();
  if (newName.length() > 0 && newName != schedule[2].name) {
    schedule[2].name = newName;
    Serial.println("Task 3 name updated from Blynk: " + newName);
    if (currentState == SHOWING_SCHEDULE) showScheduleOnDisplay();
  }
}
BLYNK_WRITE(V8) { 
  int newHours = param.asInt();
  if (newHours != schedule[2].durationHours) {
    schedule[2].durationHours = newHours;
    Serial.println("Task 3 hours updated from Blynk: " + String(newHours));
  }
}
BLYNK_WRITE(V9) { 
  int newMinutes = param.asInt();
  if (newMinutes != schedule[2].durationMinutes) {
    schedule[2].durationMinutes = newMinutes;
    Serial.println("Task 3 minutes updated from Blynk: " + String(newMinutes));
  }
}

BLYNK_WRITE(V10) { 
  String newName = param.asStr();
  if (newName.length() > 0 && newName != schedule[3].name) {
    schedule[3].name = newName;
    Serial.println("Task 4 name updated from Blynk: " + newName);
    if (currentState == SHOWING_SCHEDULE) showScheduleOnDisplay();
  }
}
BLYNK_WRITE(V11) { 
  int newHours = param.asInt();
  if (newHours != schedule[3].durationHours) {
    schedule[3].durationHours = newHours;
    Serial.println("Task 4 hours updated from Blynk: " + String(newHours));
  }
}
BLYNK_WRITE(V12) { 
  int newMinutes = param.asInt();
  if (newMinutes != schedule[3].durationMinutes) {
    schedule[3].durationMinutes = newMinutes;
    Serial.println("Task 4 minutes updated from Blynk: " + String(newMinutes));
  }
}

BLYNK_WRITE(V13) { 
  String newName = param.asStr();
  if (newName.length() > 0 && newName != schedule[4].name) {
    schedule[4].name = newName;
    Serial.println("Task 5 name updated from Blynk: " + newName);
    if (currentState == SHOWING_SCHEDULE) showScheduleOnDisplay();
  }
}
BLYNK_WRITE(V14) { 
  int newHours = param.asInt();
  if (newHours != schedule[4].durationHours) {
    schedule[4].durationHours = newHours;
    Serial.println("Task 5 hours updated from Blynk: " + String(newHours));
  }
}
BLYNK_WRITE(V15) { 
  int newMinutes = param.asInt();
  if (newMinutes != schedule[4].durationMinutes) {
    schedule[4].durationMinutes = newMinutes;
    Serial.println("Task 5 minutes updated from Blynk: " + String(newMinutes));
  }
}

void updateDisplay(const String& line1, const String& line2, const String& line3) {
  // Always show on Serial
  Serial.println("Display: " + line1 + (line2 != "" ? " | " + line2 : "") + (line3 != "" ? " | " + line3 : ""));
  
  if (!displayAvailable) {
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Line 1
  display.setCursor(0, 0);
  display.println(line1);
  
  // Line 2
  if (line2 != "") {
    display.setCursor(0, 10);
    display.println(line2);
  }
  
  // Line 3
  if (line3 != "") {
    display.setCursor(0, 20);
    display.println(line3);
  }
  
  display.display();
}

void setup() {
  pinMode(pirSensorPin, INPUT);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("Starting VIRAT system...");
  
  // Initialize I2C with explicit pins
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  
  delay(100);
  
  // OLED init with error handling
  Serial.println("Initializing OLED display...");
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed - trying alternative address 0x3D"));
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("OLED init failed completely - continuing without display"));
      displayAvailable = false;
    } else {
      displayAvailable = true;
      Serial.println("OLED initialized successfully at address 0x3D");
    }
  } else {
    displayAvailable = true;
    Serial.println("OLED initialized successfully at address 0x3C");
  }
  
  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
  }

  // WiFi and Blynk connection using Blynk.begin()
  updateDisplay("V.I.R.A.T.", "Connecting to", "WiFi & Blynk...");
  Serial.println("Connecting to WiFi and Blynk...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Auth Token: ");
  Serial.println(BLYNK_AUTH_TOKEN);
  
  // Use Blynk.begin() - this handles both WiFi and Blynk connection
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);
  
  // Wait for connection with timeout
  unsigned long startTime = millis();
  while (!Blynk.connected() && (millis() - startTime < 30000)) { // 30 second timeout
    delay(500);
    Serial.print(".");
    updateDisplay("V.I.R.A.T.", "Connecting...", String((millis() - startTime)/1000) + "s");
  }
  
  if (Blynk.connected()) {
    wifiConnected = true;
    Serial.println("\nBlynk connected successfully!");
    Serial.print("WiFi IP: ");
    Serial.println(WiFi.localIP());
    
    updateDisplay("V.I.R.A.T.", "Connected!", "WiFi & Blynk OK");
    delay(2000);
    
    // MODIFIED: Only send initial data once, then let app control
    sendScheduleToBlynk();
    updateBlynkStatus();
    initialSyncComplete = true; // Mark initial sync as done
    
    Serial.println("Initial data sent to Blynk - App can now control schedule");
  } else {
    Serial.println("\nConnection failed! Running in offline mode.");
    updateDisplay("V.I.R.A.T.", "Connection Failed", "Offline mode");
    delay(2000);
  }

  // PIR Calibration
  updateDisplay("V.I.R.A.T.", "Calibrating PIR...", "");
  Serial.println("Calibrating sensor...");
  for (int i = 0; i < calibrationTime; i++) {
    Serial.print(".");
    delay(1000);
  }
  
  // Welcome message
  updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
  Serial.println("\nSensor calibrated. Ready!");
  Serial.println("V.I.R.A.T. - Welcome to study table");
  
  printScheduleToSerial();
}

// MODIFIED: Only send to Blynk when explicitly requested
void sendScheduleToBlynk() {
  if (!Blynk.connected()) {
    Serial.println("Blynk not connected - cannot send schedule");
    return;
  }
  
  Serial.println("Sending current ESP32 schedule to Blynk app...");
  
  // Send current schedule to Blynk (this will show in the app inputs)
  Blynk.virtualWrite(V1, schedule[0].name);
  Blynk.virtualWrite(V2, schedule[0].durationHours);
  Blynk.virtualWrite(V3, schedule[0].durationMinutes);
  
  Blynk.virtualWrite(V4, schedule[1].name);
  Blynk.virtualWrite(V5, schedule[1].durationHours);
  Blynk.virtualWrite(V6, schedule[1].durationMinutes);
  
  Blynk.virtualWrite(V7, schedule[2].name);
  Blynk.virtualWrite(V8, schedule[2].durationHours);
  Blynk.virtualWrite(V9, schedule[2].durationMinutes);
  
  Blynk.virtualWrite(V10, schedule[3].name);
  Blynk.virtualWrite(V11, schedule[3].durationHours);
  Blynk.virtualWrite(V12, schedule[3].durationMinutes);
  
  Blynk.virtualWrite(V13, schedule[4].name);
  Blynk.virtualWrite(V14, schedule[4].durationHours);
  Blynk.virtualWrite(V15, schedule[4].durationMinutes);
  
  Serial.println("Schedule sent to Blynk app successfully");
}

void updateBlynkStatus() {
  if (!Blynk.connected()) return;
  
  // Send current task info
  if (currentTaskIndex != -1) {
    String currentTaskInfo = schedule[currentTaskIndex].name;
    if (schedule[currentTaskIndex].active) {
      unsigned long elapsed = (millis() - schedule[currentTaskIndex].startTime) / 1000 / 60;
      unsigned long total = schedule[currentTaskIndex].durationHours * 60 + schedule[currentTaskIndex].durationMinutes;
      currentTaskInfo += " (" + String(elapsed) + "/" + String(total) + "m)";
    }
    Blynk.virtualWrite(V18, currentTaskInfo);
  } else {
    Blynk.virtualWrite(V18, "No active task");
  }
  
  // Send system status
  String status = "";
  switch(currentState) {
    case IDLE: status = "IDLE"; break;
    case SHOWING_SCHEDULE: status = "SHOWING_SCHEDULE"; break;
    case TASK_ACTIVE: status = "TASK_ACTIVE"; break;
    case TASK_TIMER_WARNING: status = "TASK_TIMER_WARNING"; break;
  }
  Blynk.virtualWrite(V19, status);
  
  // Send WiFi status
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.virtualWrite(V20, "Connected: " + WiFi.localIP().toString());
  } else {
    Blynk.virtualWrite(V20, "Disconnected");
  }
}

void printScheduleToSerial() {
  Serial.println("\n=== TODAY'S SCHEDULE ===");
  for (int i = 0; i < numTasks; i++) {
    if (!schedule[i].completed) {
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.print(schedule[i].name);
      Serial.print(" - Duration: ");
      if (schedule[i].durationHours > 0) {
        Serial.print(schedule[i].durationHours);
        Serial.print("h ");
      }
      if (schedule[i].durationMinutes > 0) {
        Serial.print(schedule[i].durationMinutes);
        Serial.print("m");
      }
      if (schedule[i].active) {
        unsigned long elapsed = (millis() - schedule[i].startTime) / 1000 / 60;
        unsigned long totalDuration = schedule[i].durationHours * 60 + schedule[i].durationMinutes;
        Serial.print(" [ACTIVE - ");
        Serial.print(elapsed);
        Serial.print("/");
        Serial.print(totalDuration);
        Serial.print(" mins]");
      }
      Serial.println();
    }
  }
  Serial.println("========================\n");
}

void showScheduleOnDisplay() {
  Serial.println("showScheduleOnDisplay() called");
  
  // Find the next incomplete task to display
  int taskToShow = -1;
  
  // Start from the current display index
  for (int i = scheduleDisplayIndex; i < numTasks; i++) {
    if (!schedule[i].completed && schedule[i].name.length() > 0) {
      taskToShow = i;
      Serial.println("Found task to show: " + String(i) + " - " + schedule[i].name);
      break;
    }
  }
  
  // If no task found from current index, wrap around
  if (taskToShow == -1) {
    for (int i = 0; i < scheduleDisplayIndex; i++) {
      if (!schedule[i].completed && schedule[i].name.length() > 0) {
        taskToShow = i;
        Serial.println("Found task to show (wrapped): " + String(i) + " - " + schedule[i].name);
        break;
      }
    }
  }
  
  if (taskToShow != -1) {
    currentTaskOnDisplay = taskToShow;
    
    // Prepare display strings
    String taskNum = String(taskToShow + 1) + ". ";
    String taskName = schedule[taskToShow].name;
    
    // Truncate long task names
    if (taskName.length() > 13) { // Leave room for task number
      taskName = taskName.substring(0, 10) + "...";
    }
    
    // Format duration
    String duration = "";
    if (schedule[taskToShow].durationHours > 0) {
      duration += String(schedule[taskToShow].durationHours) + "h ";
    }
    if (schedule[taskToShow].durationMinutes > 0) {
      duration += String(schedule[taskToShow].durationMinutes) + "m";
    }
    if (duration == "") {
      duration = "No time set";
    }
    
    Serial.println("Displaying: " + taskNum + taskName + " Duration: " + duration);
    
    // Update the display
    updateDisplay("SCHEDULE:", taskNum + taskName, "Duration: " + duration);
    
    // Set next task for scrolling
    scheduleDisplayIndex = (taskToShow + 1) % numTasks;
    
  } else {
    Serial.println("No tasks to display - all completed or empty");
    updateDisplay("All tasks", "completed!", "Great job!");
    currentTaskOnDisplay = -1;
    scheduleDisplayIndex = 0;
  }
}

int getCurrentlyDisplayedTask() {
  if (currentTaskOnDisplay >= 0 && currentTaskOnDisplay < numTasks && 
      !schedule[currentTaskOnDisplay].completed && 
      schedule[currentTaskOnDisplay].name.length() > 0) {
    return currentTaskOnDisplay;
  }
  return -1;
}

unsigned long getTaskTimeRemaining(int taskIndex) {
  if (taskIndex == -1 || !schedule[taskIndex].active) return 0;
  
  unsigned long elapsed = millis() - schedule[taskIndex].startTime;
  unsigned long totalDuration = (schedule[taskIndex].durationHours * 60 + schedule[taskIndex].durationMinutes) * 60 * 1000;
  
  if (elapsed >= totalDuration) return 0;
  return totalDuration - elapsed;
}

String formatTime(unsigned long milliseconds) {
  unsigned long totalMinutes = milliseconds / 60000;
  unsigned long hours = totalMinutes / 60;
  unsigned long minutes = totalMinutes % 60;
  
  String result = "";
  if (hours > 0) {
    result += String(hours) + "h ";
  }
  result += String(minutes) + "m";
  return result;
}

void loop() {
  // Handle Blynk - always run this first
  Blynk.run();
  
  // Update Blynk status periodically (but don't override schedule)
  unsigned long now = millis();
  if (Blynk.connected() && (now - lastBlynkUpdate > blynkUpdateInterval)) {
    updateBlynkStatus(); // Only send status, not schedule
    lastBlynkUpdate = now;
  }
  
  int pirValue = digitalRead(pirSensorPin);
  int buttonState = digitalRead(buttonPin);
  
  // Handle button press
  if (buttonState == LOW && (now - lastButtonPress > buttonDebounce)) {
    lastButtonPress = now;
    Serial.println("Button pressed in state: " + String(currentState));
    
    if (currentState == SHOWING_SCHEDULE) {
      int taskToActivate = getCurrentlyDisplayedTask();
      if (taskToActivate != -1) {
        currentTaskIndex = taskToActivate;
        currentState = TASK_ACTIVE;
        schedule[currentTaskIndex].active = true;
        schedule[currentTaskIndex].startTime = now;
        
        String taskName = schedule[currentTaskIndex].name;
        if (taskName.length() > 15) {
          taskName = taskName.substring(0, 12) + "...";
        }
        
        String duration = "";
        if (schedule[currentTaskIndex].durationHours > 0) {
          duration += String(schedule[currentTaskIndex].durationHours) + "h ";
        }
        if (schedule[currentTaskIndex].durationMinutes > 0) {
          duration += String(schedule[currentTaskIndex].durationMinutes) + "m";
        }
        
        updateDisplay("ACTIVE:", taskName, duration + " timer");
        Serial.println("Task activated: " + schedule[currentTaskIndex].name);
        
        digitalWrite(ledPin1, HIGH);
        digitalWrite(ledPin2, HIGH);
        ledState = true;
      }
    } else if (currentState == TASK_ACTIVE || currentState == TASK_TIMER_WARNING) {
      schedule[currentTaskIndex].completed = true;
      schedule[currentTaskIndex].active = false;
      Serial.println("Task completed: " + schedule[currentTaskIndex].name);
      
      for (int i = 0; i < 3; i++) {
        digitalWrite(ledPin1, LOW);
        digitalWrite(ledPin2, LOW);
        delay(200);
        digitalWrite(ledPin1, HIGH);
        digitalWrite(ledPin2, HIGH);
        delay(200);
      }
      
      updateDisplay("Task Complete!", "Great job!", "");
      delay(2000);
      
      currentState = IDLE;
      currentTaskIndex = -1;
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, LOW);
      ledState = false;
      
      updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
    }
  }
  
  // Handle serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "SCHEDULE") {
      printScheduleToSerial();
    }
    else if (command == "RESET") {
      for (int i = 0; i < numTasks; i++) {
        schedule[i].completed = false;
        schedule[i].active = false;
        schedule[i].startTime = 0;
      }
      Serial.println("All tasks reset!");
      printScheduleToSerial();
      
      currentState = IDLE;
      currentTaskIndex = -1;
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, LOW);
      updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
    }
    else if (command == "STATUS") {
      Serial.print("Current State: ");
      switch(currentState) {
        case IDLE: Serial.println("IDLE"); break;
        case SHOWING_SCHEDULE: Serial.println("SHOWING_SCHEDULE"); break;
        case TASK_ACTIVE: Serial.println("TASK_ACTIVE"); break;
        case TASK_TIMER_WARNING: Serial.println("TASK_TIMER_WARNING"); break;
      }
      if (currentTaskIndex != -1) {
        Serial.println("Active Task: " + schedule[currentTaskIndex].name);
      }
      Serial.print("WiFi Status: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      Serial.print("Blynk Status: ");
      Serial.println(Blynk.connected() ? "Connected" : "Disconnected");
    }
    else if (command == "DISPLAY") {
      // Test display command
      Serial.println("Testing display with current schedule...");
      currentState = SHOWING_SCHEDULE;
      showScheduleOnDisplay();
    }
    else if (command == "SYNC") {
      // Manual sync command
      Serial.println("Manual sync to Blynk requested...");
      sendScheduleToBlynk();
    }
  }
  
  // State machine
  switch (currentState) {
    case IDLE:
      if (pirValue == HIGH && (now - lastMotionTime > cooldown)) {
        currentState = SHOWING_SCHEDULE;
        lastMotionTime = now;
        lastScheduleScroll = now;
        scheduleDisplayIndex = 0;
        currentTaskOnDisplay = -1;

        digitalWrite(ledPin1, HIGH);
        digitalWrite(ledPin2, HIGH);
        ledState = true;

        Serial.println("Motion detected - Switching to SHOWING_SCHEDULE state");
        showScheduleOnDisplay();
      } else {
        static unsigned long lastIdleDisplayUpdate = 0;
        const unsigned long idleDisplayInterval = 5000;

        if (now - lastIdleDisplayUpdate > idleDisplayInterval) {
          if (displayVersionToggle) {
            String wifiStatus = WiFi.status() == WL_CONNECTED ? "WiFi: OK" : "WiFi: OFF";
            updateDisplay("V.I.R.A.T. v1.5.0", wifiStatus, Blynk.connected() ? "Blynk: OK" : "Blynk: OFF");
          } else {
            updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
          }
          displayVersionToggle = !displayVersionToggle;
          lastIdleDisplayUpdate = now;
        }
      }
      break;
      
    case SHOWING_SCHEDULE:
      if (now - lastScheduleScroll > scheduleScrollDelay) {
        Serial.println("Schedule scroll timeout - showing next task");
        showScheduleOnDisplay();
        lastScheduleScroll = now;
      }
      
      if (now - lastMotionTime > 15000) {
        Serial.println("Motion timeout - returning to IDLE");
        currentState = IDLE;
        digitalWrite(ledPin1, LOW);
        digitalWrite(ledPin2, LOW);
        ledState = false;
        updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
      }
      break;
      
    case TASK_ACTIVE:
      if (now - lastTimerCheck > timerCheckInterval) {
        unsigned long timeRemaining = getTaskTimeRemaining(currentTaskIndex);
        
        if (timeRemaining <= 300000 && timeRemaining > 0) {
          currentState = TASK_TIMER_WARNING;
          Serial.println("Timer warning for: " + schedule[currentTaskIndex].name);
        } else if (timeRemaining <= 0) {
          updateDisplay("TIME'S UP!", "Task timer", "finished!");
          
          for (int i = 0; i < 10; i++) {
            digitalWrite(ledPin1, LOW);
            digitalWrite(ledPin2, LOW);
            delay(100);
            digitalWrite(ledPin1, HIGH);
            digitalWrite(ledPin2, HIGH);
            delay(100);
          }
          
          schedule[currentTaskIndex].active = false;
          currentState = IDLE;
          currentTaskIndex = -1;
          digitalWrite(ledPin1, LOW);
          digitalWrite(ledPin2, LOW);
          updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
        } else {
          String taskName = schedule[currentTaskIndex].name;
          if (taskName.length() > 15) {
            taskName = taskName.substring(0, 12) + "...";
          }
          String timeLeft = formatTime(timeRemaining);
          updateDisplay("ACTIVE:", taskName, timeLeft + " left");
        }
        
        lastTimerCheck = now;
      }
      break;
      
    case TASK_TIMER_WARNING:
      static bool blinkState = false;
      static unsigned long lastBlink = 0;
      
      if (now - lastBlink > 500) {
        blinkState = !blinkState;
        digitalWrite(ledPin1, blinkState);
        digitalWrite(ledPin2, blinkState);
        lastBlink = now;
        
        if (blinkState) {
          unsigned long timeRemaining = getTaskTimeRemaining(currentTaskIndex);
          String timeLeft = formatTime(timeRemaining);
          updateDisplay("TIME WARNING!", timeLeft + " left", "Wrap up soon!");
        }
      }
      
      if (getTaskTimeRemaining(currentTaskIndex) <= 0) {
        updateDisplay("TIME'S UP!", "Task finished!", "");
        delay(2000);
        schedule[currentTaskIndex].active = false;
        currentState = IDLE;
        currentTaskIndex = -1;
        digitalWrite(ledPin1, LOW);
        digitalWrite(ledPin2, LOW);
        updateDisplay("V.I.R.A.T.", "Welcome to", "study table");
      }
      break;
  }
  
  delay(10);
}
