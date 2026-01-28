
/**
 * @brief
 * CarInspector 目标是检测汽车CAN，K-Line，LIN总线上的数据，并可以把数据发送给PC。也可以以文件形式保存板子上的sd卡上，用于后续的分析。
 * 
 * 串口命令：
 * 串口需要的功能：
 * 1.接收Can，K，LIN，总线上的数据，并且发送到pc上
 * 2.将pc上的数据发送到指定的总线上。
 * 3.板卡自我测试，测试mcp2518是否正确工作正常
 * 4.文件管理，可以上传数据文件到办卡上，也可以下载数据文件到pc上，考虑加入压缩传输
 * 5.在线调试功能，可以通过指令调试某一个功能是否工作正常，该功能需要以后修改原理图，
 * （1）添加对一些发送后的数据传回到esp32s3某个gpio来做检测
 * （2）通过pc设置 can总线的传输率，以及工作模式
 * （3）读取故障码，清除故障码
 *  
 * 
 * Serial 默认的输出可以作为上位机的调试信息与数据分析信息，当所有信息输出的时候就需要一个标准。
 * 这里将串口 用于上位机的数据输出与普通调试信息输出进行区分。
 * 调试信息输出调用debug_info函数， 调试数据输出调用print_xxx_data函数输出
 *  上位机需要判断串口输出的数据前面的几个字符来区分是数据信息，还是调试信息
 * 
 * ACAN2517FD 驱动中 在使用esp32开发板的时候没有使用中断来完成数据接收和发送，因为esp32的中断处理函数中可能导致watchdog的超时，
 * 会导致Guru meditation 错误。这个问题不知道在esp32s3中是否解决，这个问题主要和arduino-sdk有关，在overflowstack上看到有人提起，
 * 在旧版本的esp32-arduino sdk中就没有这个问题。
 * Downgrade the ESp32 by expressive system library from 2.0.6 to 2.0.3.
 * 
 * 2026-1-21: 关于添加一块tft屏幕来观察can总线以及lin总线数据流，考虑到实时性问题，可以采用多任务来出现，
 * 尽量让任务空闲时候主动调用vTaskDelay来释放掉当前调度，从而提高任务执行效率。把屏幕的显示和ec11的读取放到一个高优先级的任务中。
 * 
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "menu.h"

// 声明全局变量用于可编辑项目
bool engine_enabled = true;
bool abs_enabled = false;
int brightness_level = 50;
int volume_level = 75;
char vehicle_name[20] = "My Car";

// 函数声明
void displayVehicleInfo();
void startDiagnostics();
void resetSystem();

// 创建TFT对象
TFT_eSPI tft = TFT_eSPI();

// 声明菜单对象
Menu* mainMenu;
Menu* settingsMenu;
Menu* advancedSettingsMenu;

// EC11旋钮引脚定义 - 请根据实际硬件连接调整这些引脚
#define EC11_PIN_A 39  // 旋钮A相连接的GPIO引脚
#define EC11_PIN_B 38  // 旋钮B相连接的GPIO引脚
#define EC11_PIN_SW 37 // 旋钮按键连接的GPIO引脚

// 全局变量用于EC11旋钮状态跟踪
volatile int encoderPos = 0;  // 旋钮位置计数器
volatile int lastEncoded = 0; // 上一次编码器状态
volatile long lastMS = 0;     // 上次检测时间，用于消抖
volatile bool buttonPressed = false; // 按键按下标志

// 中断服务程序 - 处理EC11旋钮旋转
void IRAM_ATTR handleEncoder() {
  int MS = millis();
  // 简单消抖处理
  if (MS - lastMS > 2) {
    int MSB = digitalRead(EC11_PIN_A); // MSB = most significant bit
    int LSB = digitalRead(EC11_PIN_B); // LSB = least significant bit

    int encoded = (MSB << 1) | LSB; // 将两个位组合成一个二进制数
    int sum = (lastEncoded << 2) | encoded; // 编码序列

    // 根据编码序列确定旋转方向
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
      encoderPos--; // 逆时针旋转
    }
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
      encoderPos++; // 顺时针旋转
    }

    lastEncoded = encoded;
    lastMS = MS;
  }
}

// 中断服务程序 - 处理EC11按键
void IRAM_ATTR handleButton() {
  int MS = millis();
  // 消抖处理
  if (MS - lastMS > 50) {
    buttonPressed = true;
    lastMS = MS;
  }
}

// 当前活动菜单指针
Menu* currentMenu = nullptr;

void menu_setup() {
  Serial.begin(115200);
  
  // 初始化TFT
  tft.init();
  tft.setRotation(1); // 根据需要调整旋转方向
  tft.fillScreen(TFT_BLACK);

  // 初始化EC11旋钮引脚
  pinMode(EC11_PIN_A, INPUT_PULLUP);
  pinMode(EC11_PIN_B, INPUT_PULLUP);
  pinMode(EC11_PIN_SW, INPUT_PULLUP);

  // 设置中断
  attachInterrupt(digitalPinToInterrupt(EC11_PIN_A), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(EC11_PIN_B), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(EC11_PIN_SW), handleButton, FALLING);

  // 创建各个菜单
  mainMenu = new Menu(tft);
  settingsMenu = new Menu(tft);
  advancedSettingsMenu = new Menu(tft);

  // 设置菜单ID
  mainMenu->setId(0);
  settingsMenu->setId(1);
  advancedSettingsMenu->setId(2);

  // 设置菜单层级关系
  settingsMenu->setParent(mainMenu);
  advancedSettingsMenu->setParent(settingsMenu);

  // 主菜单项目
  mainMenu->addSubmenuItem("Car Information", nullptr); // 暂时设为nullptr，后续再处理
  mainMenu->addSubmenuItem("Diagnoise", nullptr); // 暂时设为nullptr，后续再处理
  mainMenu->addSubmenuItem("Setting", settingsMenu);
  mainMenu->addActionItem("Debug", startDiagnostics);
  mainMenu->addActionItem("Reset System", resetSystem);

  // 设置菜单项目
  settingsMenu->addEditableBoolItem("test1", &engine_enabled);
  settingsMenu->addEditableBoolItem("test2", &abs_enabled);
  settingsMenu->addEditableNumberItem("test3", &brightness_level, 0, 100);
  settingsMenu->addEditableNumberItem("test4", &volume_level, 0, 100);
  settingsMenu->addEditableTextItem("Carname", vehicle_name);
  settingsMenu->addSubmenuItem("Advance Setting", advancedSettingsMenu);

  // 高级设置菜单项目
  advancedSettingsMenu->addEditableNumberItem("PID Kp", &brightness_level, 0, 200); // 使用示例变量
  advancedSettingsMenu->addEditableNumberItem("PID Ki", &volume_level, 0, 200); // 使用示例变量
  advancedSettingsMenu->addActionItem("保存设置", []() { 
    Serial.println("Setting saved"); 
  });
  advancedSettingsMenu->addActionItem("返回", []() { 
    Serial.println("back to main menu"); 
  });

  // 初始化菜单
  mainMenu->init();
  settingsMenu->init();
  advancedSettingsMenu->init();

  // 设置当前活动菜单为默认主菜单
  currentMenu = mainMenu;
  currentMenu->show();
}

// 菜单导航处理函数
void handleMenuNavigation() {
  // 处理旋钮旋转
  if (encoderPos > 0) {
    // 顺时针旋转 - 向下选择
    currentMenu->navigateDown();
    encoderPos = 0;
  } else if (encoderPos < 0) {
    // 逆时针旋转 - 向上选择
    currentMenu->navigateUp();
    encoderPos = 0;
  }

  // 处理按键按下
  if (buttonPressed) {
    buttonPressed = false;  // 清除按键标志
    
    // 根据当前选中项执行相应操作
    int result = currentMenu->selectCurrent();
    
    if (result != -1) {
      // 如果返回的是子菜单ID，则切换到对应子菜单
      if (result == settingsMenu->getId()) {
        currentMenu = settingsMenu;
        currentMenu->show();
      } else if (result == advancedSettingsMenu->getId()) {
        currentMenu = advancedSettingsMenu;
        currentMenu->show();
      }
    } else {
      // 如果是返回操作，切换回父菜单
      if (strcmp(currentMenu->getCurrentSelectionLabel().c_str(), "返回") == 0) {
        Menu* parent = currentMenu->getParent();
        if (parent != nullptr) {
          currentMenu = parent;
          currentMenu->show();
        }
      }
    }
  }
}

// 功能函数定义
void displayVehicleInfo() {
  Serial.println("显示车辆信息");
  // 实现显示车辆信息的逻辑
}

void startDiagnostics() {
  Serial.println("开始诊断...");
  // 实现诊断功能的逻辑
}

void resetSystem() {
  Serial.println("系统重置");
  // 实现系统重置的逻辑
}

#include "HardwareSerial.h"

// #include <esp32-hal-bt.h>
// #include <esp32-hal-wifi.h>

#include "esp_wifi.h"
#include "esp_bt.h"

//#include "esp_psram.h"

#include "config.h"


//sdio mmc driver
#include "FS.h"
#include "SD_MMC.h"

#include <SPI.h>

//mcp2518 driver
#include <ACAN2517FD.h>

//LIN bus library
#include <LINBus_stack.h>

//K-Line library
#include "OBD2_KLine.h"  // Include the library for OBD2 K-Line communication
// #include <AltSoftSerial.h>  // Optional alternative software serial (not used here)
// AltSoftSerial Alt_Serial;   // Create an alternative serial object (commented out)

// ---------------- Create an OBD2_KLine object for communication.
OBD2_KLine KLine(Serial2, 10400, K_LINE_RX, K_LINE_TX);  // Uses Hardware Serial (Serial1) at 10400 baud, with RX on pin 10 and TX on pin 11.
// OBD2_KLine KLine(Alt_Serial, 10400, 8, 9); // Uses AltSoftSerial at 10400 baud, with RX on pin 8 and TX on pin 9.



#include "tfcard.h"

#include "commandProccessor.h"



/**  当晶振不能正常启振，采用gpio输出一个20Mhz 50%占空比的方波来驱动mcp2518 **/
//#define EMU20Mhz_OSC 1

#ifdef EMU20Mhz_OSC
#include "osc_emu.h"
#endif 

// #define CAN_DATA 1;
// #define K_LINE_DATA 2;
// #define LIN_DATA 3;


typedef struct {
  uint8_t data_len;
  char * data;
} lin_bus_data;

typedef struct {
  uint8_t data_len;
  char * data;
} kline_bus_data;



typedef enum {
  CAN_DATA = 1,
  K_LINE_DATA = 2,
  LIN_DATA = 3
} data_type;

//所有的总线数据都使用data_t结构体保存，通过type来区分数据来源
typedef struct {
  data_type type;
  void * obj;
} data_t;


void debug_info(String str){
  Serial.println("|info:"+str);
}

void debug_err(String str){
  Serial.println("|error:"+str);
}

void print_lin_data(String str) {
  Serial.println("|data-lin:"+str);
}

void print_can_data(String str) {
  Serial.println("|data-can:"+str);
}

void print_kline_data(String str) {
  Serial.println("|data-kline:"+str);
}


//use another spi 
SPIClass tftspi(1);

SPIClass SPI2(FSPI);
ACAN2517FD can (MCP2517_CS, SPI2, 255) ; // Last argument is 255 -> no interrupt pin
ACAN2517FDSettings settings (ACAN2517FDSettings::OSC_40MHz, 500 * 1000, DataBitRateFactor::x1) ;
//LIN bus  use Serial1 gpio:15,16

//HardwareSerial LIN(1);
//HardwareSerial KLINE(2);

const int LIN_BAUD = 19200;
LINBus_stack LinBus(Serial1,LIN_BAUD);

QueueHandle_t recv_queue;
TaskHandle_t task;
TfCard tf;

void loop2(void *);
void setup() {
  // put your setup code here, to run once:

  esp_wifi_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);

  //set ata6563 stby pin to LOW for entering normal mode
  pinMode(ATA6363_STBY , LOW);

  //set tja2019 slp pin to HIGH for entering normal mode
  pinMode(TJA2019T_SLP , HIGH);

  setCpuFrequencyMhz(240);

  
  Serial.setTxBufferSize(512);
  Serial.begin(115200);
  
  menu_setup();

  //LIN.begin(115200 , SERIAL_8N1);
  
  //LIN总线一般工作的速率：低速2400bps，中速9600bps，高速19200bps
  //LinBus.begin();
  LinBus.setupSerial();

  KLine.setDebug(Serial);          // Optional: outputs debug messages to the selected serial port
  KLine.setProtocol("Automatic");  // Optional: communication protocol (default: Automatic; supported: ISO9141, ISO14230_Slow, ISO14230_Fast, Automatic)
  KLine.setByteWriteInterval(5);   // Optional: delay (ms) between bytes when writing
  KLine.setInterByteTimeout(60);   // Optional: sets the maximum inter-byte timeout (ms) while receiving data
  KLine.setReadTimeout(1000);      // Optional: maximum time (ms) to wait for a response after sending a request

    // Attempt to initialize OBD2 communication
  if (KLine.initOBD2()) {
    String VIN = KLine.getVehicleInfo(0x02);  // Get Vehicle Identification Number (VIN)
    Serial.print("VIN: "), Serial.println(VIN);

    String CalibrationID = KLine.getVehicleInfo(0x04);  // Get Calibration ID
    Serial.print("CalibrationID: "), Serial.println(CalibrationID);

    String CalibrationID_Num = KLine.getVehicleInfo(0x06);  // Get Calibration ID Number
    Serial.print("CalibrationID_Num: "), Serial.println(CalibrationID_Num);

    Serial.println();
  }


  recv_queue = xQueueCreate(1000 , sizeof(data_t *));
  
  //   // Create Task 2 on another Core
  xTaskCreatePinnedToCore(
    loop2,       // Function
    "core2",     // Name
    1024*16,          // Stack size
    NULL,          // Parameters
    16,             // Priority
    &task,        // Task handle
    xPortGetCoreID()?0:1);            // Core 0

  pinMode(MCP2517_CS, OUTPUT);

  #ifdef EMU20Mhz_OSC
    output20Mhz_osc1();
  #endif
  


  // wait for OSC to stabilize,the OSC need 5ms to stable
  delay(10);

  SPI2.begin (MCP2517_SCK, MCP2517_MISO, MCP2517_MOSI);


  
  //clear all data on LIN bus
  Serial1.flush();

}

/**
 * main core logic:
 * 1.read can bus data and put it to queue
 * 2.read kline data and put it to queue
 * 3.read lin data and put it to queue
 * 
 * add internal loopback mode for mcp2518 self-test mode
 * 
 */
void loop() {
  
  //read can bus data and put it to queue
  if(can.available()) {
    CANFDMessage message;
    can.receive(message);
    CANFDMessage * msg = new CANFDMessage;
    *msg = message;
    data_t * can_data = new data_t{
      .type = CAN_DATA,
      .obj = (void *) msg,
    };

    xQueueSend(recv_queue , &can_data , 1);
    
  }

 


  //read lin bus data and put it to queue
  /**
   * 这里需要注意LIN总线的同步 前13位为显性电平(13个低电平,紧接一个高电平)，然后的SYNC场(始终为0x55),
    紧接着时pid场，pid的范围 0-59 (十进制，因为pid只占用5位，低两位为p1,p0), ||  60,61 为诊断请求使用。 0x3c代表休眠请求。
    pid后面就是数据场，这个数据大小应该是固定的，但需要测量，测试时候可以直接使用串口读取来观察数据的长度，一般最多8个字节
  **/
  if (Serial1.available())
  {
    
    //unsigned long timeout = 1;
    size_t read_bytes = 0;
    //Serial1.setTimeout(timeout);
    int read_length = 8;
    char * buffer = new char[read_length];
    
    memset(buffer,0x0,8);
    read_bytes = Serial1.readBytes(buffer , read_length);
    
    lin_bus_data * data = new lin_bus_data{.data_len=8,.data=buffer};
    
    data_t * lin_data = new data_t{
      .type = LIN_DATA,
      .obj = (void *) data,
    };
    
    xQueueSend(recv_queue , &lin_data , 1);


    if(print_bus_message) {

      String debug_str = "";

      for(int i=0;i<read_bytes;i++) {
        debug_str += String(buffer[i],16)+" ";
      }

      print_lin_data(debug_str);
    
    }
    
    //真正读取的数据大小,使用LinBus来读取时候会自动进行效验
    //size_t ret_read = 0;
    //LinBus.read((byte *)buffer , 8 , &ret_read);

  }
  


  //read k-line data and put it to queue



   //process all bus sending data queue
   //这里需要处理所有总线要发送的数据队列逻辑，数据将从send_queue队列中读取，处理之后需要释放数据所占的内存



}


/**
 * core2 logic:
 * 1.save queue data to file in tf card 
 * 2.process Serial command 
 * 3. print data debug information in Serial when debug command turn on
 * 4. send data to any BUS  (save the sending data to send_queue ,
 * core 0 will processing these data and send it to correspending bus)
 * 
 * 处理液晶屏幕显示功能，将数据显示到tft屏幕上，处理ec11旋钮以及按键
 * 
 * 屏幕主菜单都应该有哪些功能？
 * 1.设置 
 * （1)读取can总线数据
 *  (2)读取
 * 2.数据流列表 （进入后显示实时的所有总线的数据流列表）
 * 3.
 * 
 * 屏幕操作应该具有如下功能：
 * 1.显示所有的数据列表 这个列表
 * 2.可配置显示过滤 ，可以显示指定类型的数据，例如：只显示can和lin总线数据，kline被排除
 * 3.有数据包统计功能，显示从开机以来总共接收到的每个总线的数据包个数
 * 4.具有数据列表选择功能(该功能可以选择最近200条的数据)
 * 5.录制列表功能，当点击录制按钮开始记录所有总线的数据包，直到点击停止按钮。该列表自动保存
 * 
 * 
 */
void loop2(void *pvParameters) {
  
  while(true) {

    /**
     * 处理完message对象，需要对该对象的内存区域释放，也要释放对象里的.obj元素
     */
    data_t * message;
    xQueueReceive( recv_queue , &message, 1);
    
    if(message->type == CAN_DATA) { 
      CANFDMessage msg = *(CANFDMessage *)message->obj;
    }

    else if (message->type == LIN_DATA) {
      
    }

    else if(message->type == K_LINE_DATA) {
      
    }
    
    //释放内存
    if(message->type == K_LINE_DATA) {
      kline_bus_data * ptr =  (kline_bus_data *) message->obj;
      delete ptr->data;
      delete ptr;
    }else if(message->type == LIN_DATA ) {
      lin_bus_data * ptr =  (lin_bus_data *) message->obj;
      delete ptr->data;
      delete ptr;
    }

    //delete message->obj;
    delete message;

    delayMicroseconds(1);

  }

}