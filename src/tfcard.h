
#pragma once

// #include <Arduino.h>

#include "FS.h"
#include "SD_MMC.h"
#include "config.h"
using namespace fs;

class TfCard
{

public:
    SDMMCFS &mFS;

    TfCard():mFS (SD_MMC)
    {
        SD_MMC.setPins(SDIO_CLK,SDIO_CMD,SDIO_D0);

        if (!SD_MMC.begin("/sdcard", true ,true)) {
            Serial.println("Card Mount Failed");
            return;
  }
  
  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
  }
    }

    void listDir(const char *dirname, uint8_t levels)
    {
        Serial.printf("Listing directory: %s\n", dirname);

        File root = mFS.open(dirname);
        if (!root)
        {
            Serial.println("Failed to open directory");
            return;
        }
        if (!root.isDirectory())
        {
            Serial.println("Not a directory");
            return;
        }

        File file = root.openNextFile();
        while (file)
        {
            if (file.isDirectory())
            {
                Serial.print("  DIR : ");
                Serial.println(file.name());
                if (levels)
                {
                    listDir(file.path(), levels - 1);
                }
            }
            else
            {
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("  SIZE: ");
                Serial.println(file.size());
            }
            file = root.openNextFile();
        }
    }

    void createDir(const char *path)
    {
        Serial.printf("Creating Dir: %s\n", path);
        if (mFS.mkdir(path))
        {
            Serial.println("Dir created");
        }
        else
        {
            Serial.println("mkdir failed");
        }
    }

    void removeDir(const char *path)
    {
        Serial.printf("Removing Dir: %s\n", path);
        if (mFS.rmdir(path))
        {
            Serial.println("Dir removed");
        }
        else
        {
            Serial.println("rmdir failed");
        }
    }

    void readFile(const char *path)
    {
        Serial.printf("Reading file: %s\n", path);

        File file = mFS.open(path);
        if (!file)
        {
            Serial.println("Failed to open file for reading");
            return;
        }

        Serial.print("Read from file: ");
        while (file.available())
        {
            Serial.write(file.read());
        }
    }

    void writeFile(const char *path, const char *message)
    {
        Serial.printf("Writing file: %s\n", path);

        File file = mFS.open(path, FILE_WRITE);
        if (!file)
        {
            Serial.println("Failed to open file for writing");
            return;
        }
        if (file.print(message))
        {
            Serial.println("File written");
        }
        else
        {
            Serial.println("Write failed");
        }
    }

    void appendFile(const char *path, const char *message)
    {
        Serial.printf("Appending to file: %s\n", path);

        File file = mFS.open(path, FILE_APPEND);
        if (!file)
        {
            Serial.println("Failed to open file for appending");
            return;
        }
        if (file.print(message))
        {
            Serial.println("Message appended");
        }
        else
        {
            Serial.println("Append failed");
        }
    }

    void renameFile(const char *path1, const char *path2)
    {
        Serial.printf("Renaming file %s to %s\n", path1, path2);
        if (mFS.rename(path1, path2))
        {
            Serial.println("File renamed");
        }
        else
        {
            Serial.println("Rename failed");
        }
    }

    int freeBytes() 
    {
        return mFS.totalBytes() - mFS.usedBytes();

    }

    void deleteFile(const char *path)
    {
        Serial.printf("Deleting file: %s\n", path);
        if (mFS.remove(path))
        {
            Serial.println("File deleted");
        }
        else
        {
            Serial.println("Delete failed");
        }
    }

    void testFileIO(const char *path)
    {
        File file = mFS.open(path);
        static uint8_t buf[512];
        size_t len = 0;
        uint32_t start = millis();
        uint32_t end = start;
        if (file)
        {
            len = file.size();
            size_t flen = len;
            start = millis();
            while (len)
            {
                size_t toRead = len;
                if (toRead > 512)
                {
                    toRead = 512;
                }
                file.read(buf, toRead);
                len -= toRead;
            }
            end = millis() - start;
            Serial.printf("%u bytes read for %lu ms\n", flen, end);
            file.close();
        }
        else
        {
            Serial.println("Failed to open file for reading");
        }

        file = mFS.open(path, FILE_WRITE);
        if (!file)
        {
            Serial.println("Failed to open file for writing");
            return;
        }

        size_t i;
        start = millis();
        for (i = 0; i < 2048; i++)
        {
            file.write(buf, 512);
        }
        end = millis() - start;
        Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
        file.close();
    }

    bool download_file(const char *filename)
    {

        if (!mFS.exists(filename))
        {
            Serial.printf("%s file is not exist! i can't download it!\n" , filename);
            return false;
        }

        File file = mFS.open(filename);

        if (!file)
        {
            Serial.printf(" failed to open file %s for reading\n" , filename);
            return false;
        }

        uint file_size = file.size();
        uint alread_read_size = 0;
        unsigned long rgb_light_on_time = 0;
        bool rgb_light_on = false;

        // while(alread_read_size < file_size) {
        while (file.available())
        {

            if ((millis() % 30) < 5 && !rgb_light_on)
            {
                digitalWrite(BUILTIN_LED, HIGH);
                rgb_light_on = true;
                rgb_light_on_time = millis();
            }

            char buf[512] = {0x00};
            memset(buf, 0x00, 512);
            file.readBytes(buf, 512);
            Serial.write(buf, 512);
            alread_read_size += 512;

            if (millis() - rgb_light_on_time > 30 && rgb_light_on)
            {
                // turnOffRGBLed();
                digitalWrite(BUILTIN_LED, LOW);
                rgb_light_on = false;
            }
        }

        file.close();

        return true;
    }
};