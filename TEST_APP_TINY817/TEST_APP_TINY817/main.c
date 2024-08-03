//[*]===================================================================================[*]
#include <atmel_start.h>
#include <stdio.h>
#include <util/delay.h>
#include <nvmctrl_basic.h>
//[*]===================================================================================[*]
#define FW_MAJOR        9
#define FW_MINOR        9
#define FW_REV          0
//[*]===================================================================================[*]
//[*] NVMCTRL
//[*]===================================================================================[*]
#ifndef SPM_PAGESIZE
#define SPM_PAGESIZE    1
#endif
//[*]===================================================================================[*]
#define LAST_DATA_ADDRESS    8176u
//[*]===================================================================================[*]
#define MAGIC_NUMBER    97u
//[*]===================================================================================[*]
static uint8_t                   rambuf[SPM_PAGESIZE];
static volatile nvmctrl_status_t nvm_status;
static volatile uint8_t          rb;
//[*]===================================================================================[*]
uint8_t first_boot_check(void)
{
    rb = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS);
    
    if(rb != MAGIC_NUMBER)  {
        printf("First Boot Man, Welcome:)\r\n");
        nvm_status  = FLASH_0_erase_flash_page(LAST_DATA_ADDRESS);
        nvm_status |= FLASH_0_write_flash_byte(LAST_DATA_ADDRESS, rambuf, MAGIC_NUMBER);
        nvm_status |= FLASH_0_write_flash_byte(LAST_DATA_ADDRESS+1, rambuf, FW_MAJOR);
        nvm_status |= FLASH_0_write_flash_byte(LAST_DATA_ADDRESS+2, rambuf, FW_MINOR);
        nvm_status |= FLASH_0_write_flash_byte(LAST_DATA_ADDRESS+3, rambuf, FW_REV);
        return 1u;
    }
    return 0u;
}
//[*]===================================================================================[*]
void fw_info_check(void)
{
    uint8_t magic_num, major, minor, rev = 0x00;
    
    magic_num   = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS);
    major       = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS+1);
    minor       = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS+2);
    rev         = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS+3);

    printf("FW Info =>\r\n");
    printf("FW Magic Number  : 0x%02x\r\n", magic_num);
    printf("FW Major Version : 0x%02x\r\n", major);
    printf("FW Minor Version : 0x%02x\r\n", minor);
    printf("FW Revision      : 0x%02x\r\n", rev);

    return;
}
//[*]===================================================================================[*]
int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

    printf("hello\r\n");
    
    first_boot_check();
    fw_info_check();
   
	/* Replace with your application code */
	while (1) {
        _delay_ms(500);
	}
}
//[*]===================================================================================[*]
