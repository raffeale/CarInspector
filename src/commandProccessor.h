#pragma once

#include "Arduino.h"
#include "config.h"

extern TfCard tf;

void processSerialCommand(){

//   if(!command_btn_pressed)
//     return;

  //Serial.setRxTimeout(10);

  while(Serial.available()) {
      Serial.println("command>");

    //   while(!Serial.available()) {
    //     sys_delay_ms(1);
    //   }

    //String cmd = Serial.readString();  
    String cmd = Serial.readStringUntil('\n');
      
      if(!cmd.length())
        continue;
      
      cmd.trim();
      cmd.toLowerCase();
      Serial.printf("command:%s\n" , cmd.c_str());

      if(cmd.equals("help")) {
        Serial.println("Available Command: help , ls , download filename , del filename, status, selftest [on|off] , debug [on|off]");
        continue;
      }
      else if(cmd.startsWith("download")) {
        if(cmd.length() - String("download").length()<=1) {
            Serial.println("download command usage: download filename.txt");
            continue;
        }
        char * cmd_str = (char *) cmd.c_str();
        char *token = strtok(cmd_str , " ");
        char * down_filename = NULL;
        while(token) {
          token = strtok(NULL , " ");
          if( token)  {
            down_filename = token;
          }
        }

        //Serial.flush();
        String tmp_filename = String(down_filename).startsWith("/")? down_filename:("/"+String(down_filename));
        tf.download_file(tmp_filename.c_str());
        Serial.println("");
        continue;
      }else if(cmd.equals("ls")) {
        
        tf.listDir( "/" ,0);
        continue;;
      }else if(cmd.startsWith("del")) {

        if(cmd.length() - String("del").length()<=1) {
            Serial.println("del command usage: del filename.txt");
            continue;
        }

        char * cmd_str = (char *) cmd.c_str();
        char *token = strtok(cmd_str , " ");
        char * del_filename = NULL;
        while(token) {
          token = strtok(NULL , " ");
          if( token)  {
            del_filename = token;
          }
        }
        //Serial.println(del_filename);
        String tmp_filename = String(del_filename).startsWith("/")? del_filename:("/"+String(del_filename));
        tf.deleteFile(tmp_filename.c_str());
        continue;;
      }else if(cmd.equals("exit")) {
        //delayMicroseconds(1);
        //退出时，自动关闭自测试模式
        self_test_mode = false;
        break;
      }else if(cmd.equals("status")) {
        //不确定在使用该函数是否会导致twai接口使用不正常，因为psram一旦启用后twai接口就会工作不正常
        //Serial.println("Free Memory:"+String(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)/1024.0)+"KB");
        Serial.println("Free Disk:"+String(tf.freeBytes()/1024.0)+"KB");
        Serial.println("Self-test status:"+ String(self_test_mode?"enable":"disable")+" |  Debug Mode:"+String(debug_mode?"enable":"disable"));
        //Serial.println("CAN_Transceiver Mode:"+String( (can_work_mode==1)?"Silent":"HighSpeed" ));
        continue;
      }else if(cmd.equals("debug on")){
        debug_mode = true;
          continue;
      }else if(cmd.equals("debug off")){
        debug_mode = false;
          continue;
      }else if(cmd.equals("selftest on")) {
        //开启自测试
        self_test_mode = true;
        continue;
      }else if(cmd.equals("selftest off")) {
        //开启自测试
        self_test_mode = false;
        continue;
      }
      else {
        
        Serial.println("Available Command: help , ls , download filename , del filename, status, selftest [on|off] , debug [on|off]");
        continue;
      }

  }

  //turnOffRGBLed();

}