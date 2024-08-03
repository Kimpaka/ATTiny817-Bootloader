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
uint8_t adr_nack = 0;
uint8_t data_nack = 0;
bool slave_busy = false;
//[*]===================================================================================[*]
//[*] I2C Function
//[*]===================================================================================[*]
i2c_operations_t i2c_cb_adr_nack(void *p);
i2c_operations_t i2c_cb_data_nack(void *p);
i2c_operations_t i2c_cb_busy(void *p);
//[*]===================================================================================[*]
i2c_operations_t i2c_cb_adr_nack(void *p)
{
    adr_nack++;
    return i2c_stop;
}
//[*]===================================================================================[*]
i2c_operations_t i2c_cb_data_nack(void *p)
{
    data_nack++;
    return i2c_stop;
}
//[*]===================================================================================[*]
i2c_operations_t i2c_cb_busy(void *p)
{
    slave_busy = true;
    return i2c_stop;
}
//[*]===================================================================================[*]
i2c_operations_t boot_write_n_data_handler(void *d)
{
    transfer_descriptor_t *desc = (transfer_descriptor_t *)d;
    
    I2C_0_set_buffer((void *)desc->data, desc->size);
    I2C_0_set_data_complete_callback(i2c_cb_return_stop, NULL);
    I2C_0_set_data_nack_callback(i2c_cb_data_nack, NULL);
    I2C_0_set_address_nack_callback(i2c_cb_adr_nack, NULL);
    
    return i2c_continue;
}
//[*]===================================================================================[*]
void boot_write_n(uint8_t adr, uint8_t *data, uint16_t size)
{
    transfer_descriptor_t d = {data, size};
    
    while (!I2C_0_open(SLAVE_ADDR));

    I2C_0_set_data_complete_callback(boot_write_n_data_handler, &d);
    I2C_0_set_address_nack_callback(i2c_cb_adr_nack, NULL);
    I2C_0_set_buffer((void *)&adr, 1);
    I2C_0_master_operation(false);
    
    while (I2C_BUSY == I2C_0_close());
}
//[*]===================================================================================[*]
i2c_operations_t boot_read_n_data_handler(void *d)
{
    transfer_descriptor_t *desc = (transfer_descriptor_t *)d;
    
    I2C_0_set_buffer((void *)desc->data, desc-> size);
    I2C_0_set_data_complete_callback(i2c_cb_return_stop, NULL);
    I2C_0_set_data_nack_callback(i2c_cb_data_nack, NULL);
    I2C_0_set_address_nack_callback(i2c_cb_adr_nack, NULL);
    
    return i2c_continue;
}
//[*]===================================================================================[*]
void boot_read_n(uint8_t adr, uint8_t *w_data, uint16_t w_size, uint8_t *r_data, uint16_t r_size)
{
    transfer_descriptor_t d = {w_data, w_size};

    while (!I2C_0_open(SLAVE_ADDR));

    I2C_0_set_data_complete_callback(boot_read_n_data_handler, &d);
    I2C_0_set_address_nack_callback(i2c_cb_adr_nack, NULL);
    I2C_0_set_buffer((void *)&adr, 1);
    I2C_0_master_operation(false);
    
    while(I2C_BUSY == I2C_0_close());
    
    while (!I2C_0_open(SLAVE_ADDR));
    I2C_0_set_buffer((void *)r_data, r_size);
    I2C_0_master_operation(true);
    
    while(I2C_BUSY == I2C_0_close());
}
//[*]===================================================================================[*]
void boot_check_ready(uint16_t adr)
{
    uint8_t sla = SLAVE_ADDR | ((adr >> 8) & 0x3);
    uint8_t a = adr & 0xff;

    slave_busy = false;
    
    while (!I2C_0_open(sla));
    
    I2C_0_set_address_nack_callback(i2c_cb_busy, NULL);
    I2C_0_set_buffer((void *)&a, 1);
    I2C_0_master_operation(false);
    
    while (I2C_BUSY == I2C_0_close());
}
//[*]===================================================================================[*]
void boot_wait_ready()
{
    slave_busy = true;
    while (slave_busy)
    boot_check_ready(0);
}
//[*]===================================================================================[*]
//[*] CRC Function
//[*]===================================================================================[*]
uint8_t calculate_crc8(uint8_t *data, uint8_t length)
{
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    
    for(uint8_t i=0; i<length; i++) {
        crc ^= data[i];
        for(uint8_t j=0; j<8; j++)  {
            if(crc & 0x80)  {
                crc = (crc << 1) ^ polynomial;
            }
            else    {
                crc <<= 1;
            }
        }
    }
    return crc;
}
//[*]===================================================================================[*]
uint16_t calculate_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0x0000;
    uint16_t polynomial = 0x1021;
    
    for(uint8_t i=0; i<length; i++) {
        crc ^= (uint16_t)(*data++) << 8;
        for (uint8_t j= 0; j<8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            }
            else    {
                crc <<= 1;
            }
        }
    }
    return crc;
}
//[*]===================================================================================[*]
//[*] NVMCTRL Function
//[*]===================================================================================[*]
uint8_t one_page_write(uint8_t page)
{
    struct i2c_command command;

    uint8_t crc_chk_res = 0;
    uint8_t buf[70] = { 0x00, };
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    uint8_t* page_start_addr = (uint8_t*)&buf[1];
    
    uint16_t fw_image_start = 0;
    
    buf[0] = MASTER_WRITE;
    buf[1] = 68u;
    buf[2] = PAGE_WRITE;
    buf[3] = 0x00;
    buf[4] = page;
    
    fw_image_start = page * PAGE_SIZE;
    
    for(uint8_t i=0; i<PAGE_SIZE; i++)   {
        buf[i+5] = fw_image[i + fw_image_start];
    }
    
    buf[69] = calculate_crc8((uint8_t*)&buf, 69);
    
    boot_write_n(MASTER_WRITE, page_start_addr, 69);
    
    _delay_ms(10);
    
    printf("page data crc : %02x\r\n", buf[69]);
    
    printf("page(%d) data =>\r\n", page);
    for(uint8_t n=1; n<PAGE_SIZE+1; n++)    {
        printf("%02x, ", buf[n+4]);
        if(n%16==0)
        printf("\r\n");
    }
    
    command.master_dir = MASTER_READ;
    command.data_len   = 0x04;
    command.command_1  = PAGE_WRITE_CHK;
    command.command_2  = 0x00;
    command.page_num   = 0x00;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_read_n(command.master_dir, start_addr, 5, buf, 3);
    
    crc_chk_res = buf[1];
    printf("page write crc check : %d\r\n", crc_chk_res);
    
    return crc_chk_res;
}
//[*]===================================================================================[*]
uint8_t fw_update_function(void)
{
    uint8_t page_write_res = 0;

    for(uint8_t i=0; i<PAGE_NUM; i++)   {
        page_write_res = page_write_res || one_page_write(i);
    }

    printf("page write result : %d\r\n", page_write_res);

    return page_write_res;
}
//[*]===================================================================================[*]
uint8_t one_page_read(uint8_t page)
{
    struct i2c_command command;
    
    uint8_t buf[70] = { 0x00, };
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    
    command.master_dir = MASTER_READ;
    command.data_len   = 0x04;
    command.command_1  = PAGE_READ;
    command.command_2  = 0x00;
    command.page_num   = page;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_read_n(command.master_dir, start_addr, 5, buf, 66);
    
    for(uint8_t i=1; i<PAGE_SIZE+1; i++)    {
        printf("%02x, ", buf[i]);
        if(i%16==0)
        printf("\r\n");
    }
    printf("\r\n");
    return 0;
}
//[*]===================================================================================[*]
void page_read_function(void)
{
    for(uint8_t i=0; i<PAGE_NUM; i++)   {
        one_page_read(i);
    }
}
//[*]===================================================================================[*]
uint8_t page_erase(uint8_t page)
{
    struct i2c_command command;
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;    
    uint8_t r_buf[10] = { 0x00, };
    uint8_t erase_result = 0;

    // Application Flash Erase
    command.master_dir = MASTER_WRITE;
    command.data_len   = 0x04;
    command.command_1  = CHIP_ERASE;
    command.command_2  = 0x00;
    command.page_num   = page;      // 0xff : Erase All
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_write_n(command.master_dir, start_addr, 5);        

    // Check Application Flash Erase
    command.master_dir = MASTER_READ;
    command.data_len   = 0x04;
    command.command_1  = PAGE_ERASE_CHK;
    command.command_2  = 0x00;
    command.page_num   = page;      // 0xff : Erase All
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_read_n(command.master_dir, start_addr, 5, r_buf, 3);
    
    erase_result = r_buf[1];
    
    return erase_result;
}
//[*]===================================================================================[*]
//[*] Initialize Function
//[*]===================================================================================[*]
uint8_t check_update_mode(void)
{
    struct i2c_command command;
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;   
    uint8_t r_buf[12] = { 0x00, };
    uint8_t crc_check = 0x00;
    uint8_t update_mode = 0;
    
    // Check FW Update Mode
    command.master_dir = MASTER_READ;
    command.data_len   = 0x04;
    command.command_1  = STATUS;
    command.command_2  = 0x00;
    command.page_num   = 0x00;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    update_mode = r_buf[1];
    printf("check fw update mode : %d\r\n", update_mode);
    
    return update_mode;
}
//[*]===================================================================================[*]
uint8_t set_boot_mode(uint8_t mode)
{
    struct i2c_command command;
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    
    uint8_t r_buf[12] = { 0x00, };
        
    if(mode == NORMAL_MODE) {
        printf("Go Boot Normal Mode\r\n");
        command.master_dir = MASTER_WRITE;
        command.data_len   = 0x04;
        command.command_1  = OUT_UPDATE_MODE;
        command.command_2  = 0x00;
        command.page_num   = 0x00;
        command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
        
        boot_write_n(command.master_dir, start_addr, 5);
        
        if(check_update_mode() != 0)    {
            printf("Mode Change Fail!!\r\n");
            return 1;
        }
        else    {
            printf("Mode Change Sucess!!\r\n");
            return 0;
        }
    }
    else if(mode == UPDATE_MODE)    {
        printf("Go Boot Normal Mode\r\n");
        command.master_dir = MASTER_WRITE;
        command.data_len   = 0x04;
        command.command_1  = GO_UPDATE_MODE;
        command.command_2  = 0x00;
        command.page_num   = 0x00;
        command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
        
        boot_write_n(command.master_dir, start_addr, 5);
        
        if(check_update_mode() != 1)    {
            printf("Mode Change Fail!!\r\n");
            return 1;
        }
        else    {
            printf("Mode Change Sucess!!\r\n");
            return 0;
        }
    }
}
//[*]===================================================================================[*]
chip_id* check_chip_id(void)
{
    struct i2c_command command;
    
    chip_id* id = (chip_id*)malloc(sizeof(chip_id));
    
    uint8_t r_buf[12] = { 0x00, };
                
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    uint8_t* id_start   = (uint8_t*)&r_buf[1];
    
    command.master_dir = MASTER_READ;
    command.data_len   = 4;
    command.command_1  = CHIP_ID;
    command.command_2  = 0x00;
    command.page_num   = 0;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_read_n(command.master_dir, start_addr, 5, r_buf, 12);
    strncpy((char*)id->id, id_start, sizeof(id->id));
    
    return id;
}
//[*]===================================================================================[*]
boot_version* check_boot_version(void)
{
    boot_version* boot = (boot_version*)malloc(sizeof(boot_version));
    struct i2c_command command;
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    
    uint8_t r_buf[12] = { 0x00, };

    command.master_dir = MASTER_READ;
    command.data_len   = 4;
    command.command_1  = BOOT_VER;
    command.command_2  = 0x00;
    command.page_num   = 0;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_read_n(command.master_dir, start_addr, 5, r_buf, 5);
    
    boot->boot_major = r_buf[1];
    boot->boot_minor = r_buf[2];
    boot->boot_rev   = r_buf[3];
    
    return boot;
}
//[*]===================================================================================[*]
chip_information* boot_init(void)
{
    chip_information*   chip = (chip_information*)malloc(sizeof(chip_information));
    boot_version*       boot = (boot_version*)malloc(sizeof(boot_version));
    
    struct i2c_command command;
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    
    uint8_t r_buf[12] = { 0x00, };
    uint8_t crc_check = 0x00;
    
    memset(r_buf, 0x00, sizeof(r_buf));
    
    // Check FW Update Mode
    if(check_update_mode() == UPDATE_MODE)   {
        set_boot_mode(NORMAL_MODE);
    }
    
    // Chip ID Read
    strncpy((char*)chip->id, (const char*)check_chip_id(), sizeof(chip->id));
    
    // Boot Version Read
    boot = check_boot_version();

    chip->boot_major = boot->boot_major;
    chip->boot_minor = boot->boot_minor;
    chip->boot_rev   = boot->boot_rev;
    
    free(boot);
    
    return chip;
}
//[*]===================================================================================[*]
fw_information* fw_init(void)
{
    struct i2c_command command;
    
    fw_information* fw = (fw_information*)malloc(sizeof(fw_information));
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    uint8_t r_buf[12] = { 0x00, };
    
    // Boot Version Read
    command.master_dir = MASTER_READ;
    command.data_len   = 4;
    command.command_1  = FW_VER;
    command.command_2  = 0x00;
    command.page_num   = 0;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_read_n(command.master_dir, start_addr, 5, r_buf, 5);
    fw->fw_major = r_buf[1];
    fw->fw_minor = r_buf[2];
    fw->fw_rev   = r_buf[3];
    
    return fw;
}
//[*]===================================================================================[*]
void fw_update(void)
{
    uint16_t fw_size = 0;
    uint8_t  page_size = 0;
    
    uint8_t r_buf[10] = { 0x00, };

    struct i2c_command command;
    
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    
    //    fw_size = sizeof(fw_image);
    printf("fw size : %d\r\n", fw_size);
    page_size = fw_size / 64u;
    printf("fw page size : %d\r\n", page_size);
    
    // Check FW Update Mode
    if(check_update_mode() != 1u)   {
        set_boot_mode(UPDATE_MODE);
    }
    
    // Application Flash Erase
    if(!page_erase(0xff))   {
        printf("All Page Erase Sucess!!\r\n");
    }
    
    // Firmware Update
    fw_update_function();
    
    // FW Read All
    page_read_function();
    
    set_boot_mode(NORMAL_MODE);
    
    return;
}
//[*]===================================================================================[*]
uint8_t exit_boot()
{
    struct i2c_command command;
    uint8_t* start_addr = (uint8_t*)&command.data_len;
    
    command.master_dir = MASTER_WRITE;
    command.data_len   = 0x04;
    command.command_1  = EXIT_BOOT;
    command.command_2  = 0x00;
    command.page_num   = 0x00;
    command.crc_8      = calculate_crc8((uint8_t*)&command, 5);
    
    boot_write_n(MASTER_WRITE, start_addr, 5);
    printf("Exit Boot!!\r\n");
    
    return 0;
}
//[*]===================================================================================[*]