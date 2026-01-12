
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
 */

#include <Arduino.h>

#include "config.h"

#include <SPI.h>
#include <ACAN2517FD.h>

/**  当晶振不能正常启振，采用gpio输出一个20Mhz 50%占空比的方波来驱动mcp2518 **/
//#define EMU20Mhz_OSC 1

#ifdef EMU20Mhz_OSC
#include "osc_emu.h"
#endif 

// #define CAN_DATA 1;
// #define K_LINE_DATA 2;
// #define LIN_DATA 3;

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


SPIClass SPI2(FSPI);
ACAN2517FD can (MCP2517_CS, SPI2, 255) ; // Last argument is 255 -> no interrupt pin
ACAN2517FDSettings settings (ACAN2517FDSettings::OSC_40MHz, 500 * 1000, DataBitRateFactor::x1) ;

QueueHandle_t recv_queue, dbg_msg_queue;
TaskHandle_t task;
void loop2(void *);
void setup() {
  // put your setup code here, to run once:

  setCpuFrequencyMhz(240);
  
  Serial.begin(115200);

  
  recv_queue = xQueueCreate(1000 , sizeof(data_t));
  
  
  //   // Create Task 2 on Core 1
  xTaskCreatePinnedToCore(
    loop2,       // Function
    "loop2",     // Name
    1024*16,          // Stack size
    NULL,          // Parameters
    1,             // Priority
    &task,        // Task handle
    xPortGetCoreID()?0:1);            // Core 0

  pinMode(MCP2517_CS, OUTPUT);

  #ifdef EMU20Mhz_OSC
    output20Mhz_osc1();
  #endif
  
  // wait for OSC to stabilize
  delay(10);

  SPI2.begin (MCP2517_SCK, MCP2517_MISO, MCP2517_MOSI);
  
  
  

}

void loop() {
  // put your main code here, to run repeatedly:
}


void loop2(void *pvParameters) {


}