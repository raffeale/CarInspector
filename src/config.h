
#pragma once

//MCP2518 configuration
static const int MCP2517_SCK  = 12 ; // SCK input of MCP2517FD
static const int MCP2517_MOSI = 11 ; // SDI input of MCP2517FD
static const int MCP2517_MISO = 13 ; // SDO output of MCP2517FD
static const int MCP2517_CS  = 10 ; // CS input of MCP2517FD
static const int MCP2517_INT = 9 ;
static const int MCP2517_RESET = 8 ;
static const int MCP2517_SPI_SPEED = 10000000;


//need change User_Setup.h file to set following params
// static const int TFT_SCK = 36; //SCK for st7789
// static const int TFT_MISO = 37; //NC
// static const int TFT_MOSI = 35;//MOSI for st7789
// static const int TFT_CS = 45; // CS for st7789
// static const int TFT_DC = 21; 
// static const int TFT_RST = 20; //RESET for st7789

//ec11  需要测试 使用gpio47,48作为 按钮按键是否正常工作
static const int EC11_A = 33;
static const int EC11_B = 32;
static const int EC11_BTN = 34;
static const int EC11_SPEED = 1000;
static const int EC11_BTN_DEBOUNCE = 50;
static const int EC11_BTN_LONG_PRESS = 1500;
static const int EC11_BTN_LONG_PRESS_TIMEOUT = 5000;
static const int MAIN_BTN = 38;



#define MCP2518_DEFAULT_WORK_MODE ACAN2517FDSettings::InternalLoopBack

//ata6535 STBY = LOW is normal mode, STBY  = 1 STAND BY mode
//this chip could replace with tja1051t/3 or sit1051t/3 ,these chip have silent mode with this pin
static const int ATA6363_STBY = 4;

//tja2019 SLP=HIGH is normal mode , SLP=LOW is sleep mode
//LIN BUS
static const int TJA2019T_SLP = 21;
static const int LIN_BAUDRATE = 10940;


//SDIO configuration
static const int SDIO_CLK = 40;
static const int SDIO_CMD = 41;
static const int SDIO_D0 = 42;

//K-Line configuration
//if K-line did not send any data to K-Line bus , check LSF0102DCUR chip ,does it have any data send from 5 pin
//test LSF0102DCUR pin8(EN) to GND voltage,it should 5v
static const int K_LINE_TX = 7;
static const int K_LINE_RX = 6;
static const int KLINE_BAUDRATE = 50000;


//LIN-Bus configuration
static const int LIN_TX = 5;
static const int LIN_RX = 4;
static const int LIN_SLP = 21;
static const int LIN_BUS_ID = 0x01;
static const int LIN_BUS_ID_LENGTH = 1;
static const int LIN_BUS_ID_OFFSET = 0;
static const int LIN_BUS_ID_MASK = 0xFF;
static const int LIN_BUS_ID_MATCH = 0x01;
static const int LIN_BUS_ID_MATCH_LENGTH = 1;
static const int LIN_BUS_ID_MATCH_OFFSET = 0;

//self test mode setting
bool self_test_mode = false;
bool debug_mode = false;

// bool print_can_message = false;
// bool print_lin_message = false;
// bool print_kin_message = false;

//print all bus data 
bool print_bus_message = false;

bool print_debug_info = false;
bool print_debug_error = false;
