//[*]===================================================================================[*]
#include <stdio.h>
#include <string.h>
#include <atmel_start.h>
#include <usart_basic.h>
#include <atomic.h>
#include <i2c_types.h>
#include <i2c_master.h>
#include <util/delay.h>
//[*]===================================================================================[*]
#include "boot_host.h"
//[*]===================================================================================[*]
int main(void)
{
    /* Initializes MCU, drivers and middleware */
    atmel_start_init();

    ENABLE_INTERRUPTS();
    
    chip_information* chip_info  = (chip_information*)malloc(sizeof(chip_information));
    fw_information* fw_info = (fw_information*)malloc(sizeof(fw_information));
    
    printf("hello\r\n");
    
RET:
    chip_info = boot_init();
    fw_info = fw_init();
    
    printf("chip id : %s\r\n", (char*)chip_info->id);
    printf("boot major : %02x, boot minor : %02x, boot rev : %02x\r\n", chip_info->boot_major, chip_info->boot_minor, chip_info->boot_rev);
    printf("fw major : %02x, fw minor : %02x, fw rev : %02x\r\n", fw_info->fw_major, fw_info->fw_minor, fw_info->fw_rev);
    
    if((fw_info->fw_major == 0xff) && (fw_info->fw_minor == 0xff) && (fw_info->fw_rev == 0xff)) {
        printf("Ni, Firmware Er De Got No?\r\n");
        fw_update();
        goto RET;
    }
    else    {
        printf("Firmware Update Iranaiyo! Kono Baka!\r\n");
    }
    
    free(chip_info);
    free(fw_info);
    
//    exit_boot();
    
    /* Replace with your application code */
    while (1) {
        _delay_ms(500);
    }
    
    return 0;
}
//[*]===================================================================================[*]