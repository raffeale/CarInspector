
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
 */

#include <Arduino.h>

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



SPIClass SPI2(FSPI);
ACAN2517FD can (MCP2517_CS, SPI2, 255) ; // Last argument is 255 -> no interrupt pin
ACAN2517FDSettings settings (ACAN2517FDSettings::OSC_40MHz, 500 * 1000, DataBitRateFactor::x1) ;
//LIN bus  use Serial1 gpio:15,16

//HardwareSerial LIN(1);
HardwareSerial KLINE(2);

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
  

  //LIN.begin(115200 , SERIAL_8N1);
  
  //LIN总线一般工作的速率：低速2400bps，中速9600bps，高速19200bps
  //LinBus.begin();
  LinBus.setupSerial();

  KLINE.begin(115200 , SERIAL_8N1 ,6 ,7);

  recv_queue = xQueueCreate(1000 , sizeof(data_t *));
  
  //   // Create Task 2 on another Core
  xTaskCreatePinnedToCore(
    loop2,       // Function
    "core2",     // Name
    1024*16,          // Stack size
    NULL,          // Parameters
    1,             // Priority
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
    
    data_t * lin_bus_data = new data_t{
      .type = LIN_DATA,
      .obj = (void *) data,
    };
    
    xQueueSend(recv_queue , &lin_bus_data , 1);


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
 */
void loop2(void *pvParameters) {
  
  while(1) {

    /**
     * 处理完message对象，需要对该对象的内存区域释放，也要释放对象里的.obj元素
     */
    data_t * message;
    xQueueReceive( recv_queue , &message, 1);
    

    //释放内存
    if(message->type == K_LINE_DATA || message->type == LIN_DATA ) {
      kline_bus_data * ptr =  (kline_bus_data *) message->obj;
      delete ptr->data;
    }else if(message->type == LIN_DATA ) {
      lin_bus_data * ptr =  (lin_bus_data *) message->obj;
      delete ptr->data;
    }

    delete message->obj;
    delete message;

    delayMicroseconds(1);

  }

}