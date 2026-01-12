#include <ACAN2517FD.h>

extern ACAN2517FD can ;
extern ACAN2517FDSettings settings 
bool setCanWorkMode(uint8_t mode) {

    //ACAN2517FDSettings settings (ACAN2517FDSettings::OSC_40MHz, 500 * 1000, DataBitRateFactor::x1) ;
    settings.mRequestedMode = mode ;
    
    
}