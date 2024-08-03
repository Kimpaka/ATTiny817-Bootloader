//[*]===================================================================================[*]
#include <atmel_start.h>
#include <atomic.h>
#include <string.h>
#include <i2c_slave.h>
#include <nvmctrl_basic.h>
//[*]===================================================================================[*]
uint8_t addr = 0x00;
//[*]===================================================================================[*]
volatile uint8_t  read_buf [70] = { 0x00, };
volatile uint8_t  write_buf[70] = { 0x00, };
volatile uint8_t  page_data[64] = { 0x00, };
volatile uint8_t  crc_data_chk  = 0x00;
//[*]===================================================================================[*]
volatile uint8_t  data_len = 0;
//[*]===================================================================================[*]
volatile uint8_t  read_index  = 0;
volatile uint8_t  write_index = 0;
//[*]===================================================================================[*]
uint8_t master_read_flag = 0;
//[*]===================================================================================[*]
volatile uint8_t  master_dir        = 0u;
volatile uint8_t  r_data_len        = 0u;
volatile uint8_t  w_data_len        = 0u;
volatile uint8_t  master_com_1      = 0u;
volatile uint8_t  master_com_2      = 0u;
volatile uint8_t  page_num          = 0u;
volatile uint8_t  crc_data          = 0u;
//[*]===================================================================================[*]
volatile uint8_t fw_major           = 0u;
volatile uint8_t fw_minor           = 0u;
volatile uint8_t fw_rev             = 0u;
//[*]===================================================================================[*]
volatile bool       ready_flag  = false;
volatile uint8_t    update_flag = 0x00;
//[*]===================================================================================[*]
#define MASTER_WRITE    0x00
#define MASTER_READ     0x01
//[*]===================================================================================[*]
//[*] Master Read Command
//[*]===================================================================================[*]
#define CHIP_ID         0b00000001  // "ATTINY1617"
#define BOOT_VER        0b00000010  // Boot Major, Minor, Revision
#define FW_VER          0b00000100  // FW Major, Minor, Revision
#define CHECKSUM        0b00001000  // APP DATA CheckSum
#define VALID_FLAG      0b00010000  // Valid Flag --> Include Majic Number
#define INVALID_FLAG    0b00100000  // InValid Flag --> Always 0xffff
#define PAGE_READ       0b01000000  // APP Page Read
#define STATUS          0b10000000  // Boot Status
//[*]===================================================================================[*]
//[*] Master Read Command ( Update Mode )
//[*]===================================================================================[*]
#define PAGE_ERASE_CHK	0b00000001  // Page Erase Complete
#define PAGE_WRITE_CHK	0b00000010	// Page Write Complete
#define UPDATE_PAGE_CHK 0b00000100	// Page number written just before 
//[*]===================================================================================[*]
//[*] Master Write Command
//[*]===================================================================================[*]
#define EXIT_BOOT       0b00000001  // Go to Application
#define GO_BOOT         0b00000010  // GO to Boot
#define GO_UPDATE_MODE  0b00000100  // Go to FW Update Mode
#define OUT_UPDATE_MODE 0b00001000  // Out FW Update Mode
#define CHIP_ERASE      0b00010000  // Application Area Erase
#define PAGE_WRITE      0b00100000  // Flash Page Write
//[*]===================================================================================[*]
#define CHIP            "ATTINY1617"
//[*]===================================================================================[*]
#define BOOT_MAJOR      9
#define BOOT_MINOR      9
#define BOOT_REV        0
//[*]===================================================================================[*]
//[*] NVMCTRL
//[*]===================================================================================[*]
#ifndef SPM_PAGESIZE
#define SPM_PAGESIZE    1
#endif
//[*]===================================================================================[*]
#define PAGE_SIZE	        64u
#define PAGE_NUM	        88u
#define APP_ADDRESS         2560u
#define LAST_DATA_ADDRESS   8176u
//[*]===================================================================================[*]
static uint8_t                   rambuf[SPM_PAGESIZE];
static volatile nvmctrl_status_t nvm_status;
static volatile uint8_t          rb;
//[*]===================================================================================[*]
//[*] Interrupt Handler
//[*]===================================================================================[*]
void i2c_clear(void)
{
    read_index  = 0u;
    write_index = 0u;
    data_len  = 0u;
    memset(read_buf,  0x00, sizeof(read_buf));
    memset(write_buf, 0x00, sizeof(write_buf));
    master_read_flag = 0;
}
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
//[*] NVMCTRL
//[*]===================================================================================[*]
uint8_t flash_page_erase(uint8_t page)
{
    uint8_t erase_status = 0u;
    erase_status = FLASH_0_erase_flash_page(APP_ADDRESS + (page * PAGE_SIZE));
    return erase_status;
}
//[*]===================================================================================[*]
uint8_t flash_erase(void)
{
	uint8_t erase_status = 0;
    
    // Erase All Page
    if(page_num == 0xff)    {
	    for(uint8_t i=0; i<PAGE_NUM; i++)	{
        	erase_status = FLASH_0_erase_flash_page(APP_ADDRESS + (i * PAGE_SIZE));
        	nvm_status = nvm_status || erase_status;
	    }
    }
    else    {
        nvm_status = flash_page_erase(page_num);
    }
    
    return nvm_status;
}
//[*]===================================================================================[*]
uint8_t flash_page_write(uint8_t page)
{    
    uint8_t cal_crc = 0x00;
    
    for(uint8_t i=0; i<PAGE_NUM; i++)   {
        page_data[i] = read_buf[i+5];
    }
    
    cal_crc = calculate_crc8((uint8_t*)&read_buf, read_buf[1]+1);
    
    if(crc_data != cal_crc)
        crc_data_chk = 1;
    else
        crc_data_chk = 0;
    
    if(!crc_data_chk)
        FLASH_0_write_flash_page(APP_ADDRESS + (page * PAGE_SIZE), page_data);
    
    return 0;
}
//[*]===================================================================================[*]
uint8_t flash_page_read(uint8_t page)
{
    write_buf[0] = 65;
    for(uint8_t i=0; i<PAGE_SIZE; i++)   {
        write_buf[i+1] = FLASH_0_read_flash_byte((APP_ADDRESS + (page*PAGE_SIZE)) + i);
    }
    write_buf[write_buf[0]] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
    
    return 0;
}
//[*]===================================================================================[*]
void fw_ver_check(void)
{
    fw_major = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS+1);
    fw_minor = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS+2);
    fw_rev   = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS+3);
}
//[*]===================================================================================[*]
void i2c_read(void)
{
	if(master_dir == MASTER_WRITE) {
		switch(master_com_1)  {
			case EXIT_BOOT :
			jump_to_address(0x500);
			break;
			
			case GO_UPDATE_MODE :
			update_flag = 1;
			break;
            
            case OUT_UPDATE_MODE :
            update_flag = 0;
            break;
			
			case CHIP_ERASE :
			flash_erase();
            break;
                
            case PAGE_WRITE :
            flash_page_write(page_num);
            
			break;
		}
	}
}
//[*]===================================================================================[*]
void i2c_write(void)
{       
	if(master_dir == MASTER_READ)   {
		if(!update_flag)	{
			switch(master_com_1) {
				case CHIP_ID :
				write_buf[0] = 11;
				strcpy(write_buf+1, CHIP);
				write_buf[11] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
				break;
				
				case BOOT_VER :
				write_buf[0] = 4;
				write_buf[1] = BOOT_MAJOR;
				write_buf[2] = BOOT_MINOR;
				write_buf[3] = BOOT_REV;
				write_buf[4] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
				break;

				case FW_VER :
                fw_ver_check();
				write_buf[0] = 4;
				write_buf[1] = fw_major;
				write_buf[2] = fw_minor;
				write_buf[3] = fw_rev;
				write_buf[4] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
				break;
				
				case STATUS :
				write_buf[0] = 2;
				write_buf[1] = update_flag;
				write_buf[2] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
				break;

			}
		}
		else	{
			switch(master_com_1)	{
				case STATUS :
				write_buf[0] = 2;
				write_buf[1] = update_flag;
				write_buf[2] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
				break;
                
				case PAGE_ERASE_CHK	:
				write_buf[0] = 2;
				write_buf[1] = nvm_status;
				write_buf[2] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
				break;
                
                case PAGE_WRITE_CHK :
                write_buf[0] = 2;
                write_buf[1] = crc_data_chk;
                write_buf[2] = calculate_crc8((uint8_t*)&write_buf, write_buf[0]);
                break;
                
                case PAGE_READ :
                flash_page_read(page_num);
                break;
			}
		}
	}
}
//[*]===================================================================================[*]
void I2C_address_handler()
{
	addr = I2C_0_read();
	I2C_0_send_ack(); // or send_nack() if we don't want to ack the address
}
//[*]===================================================================================[*]
void I2C_read_handler() // Master Write / Slave Read
{
	// Master write handler
	if (read_index < sizeof(read_buf)) {
		
		read_buf[read_index] = I2C_0_read();
		
        // Check Master Read / Write Status
		if(read_index == 0u)    {
		    master_dir = read_buf[read_index];
        }
        
        // Check Data Length
		else if(read_index == 1u)    {
			r_data_len = read_buf[read_index];
		}
        
        else if(read_index == 2u)   {
            master_com_1 = read_buf[read_index];
        }
        
        else if(read_index == 3u)   {
            master_com_2 = read_buf[read_index];
        }

        else if(read_index == 4u)   {
            page_num = read_buf[read_index];
        }
        		
		if(r_data_len)    {
			if(r_data_len < read_index)   {
                crc_data = read_buf[read_index];
                if(master_dir == MASTER_WRITE)
                    i2c_read();
				I2C_0_send_nack();
				return;
			}
		}
		I2C_0_send_ack();
		read_index++;
	}
}
//[*]===================================================================================[*]
void I2C_write_handler() // Master Read / Slave Write
{
    if(!master_read_flag)   {
        master_read_flag = 1u;
        i2c_write();
    }    
	// Master read handler
	if (write_index < sizeof(write_buf)) {
		
		I2C_0_write(write_buf[write_index]);
		
		if(write_index == 0u)
		    w_data_len = write_buf[write_index];
		
		if(w_data_len)  {
			if(w_data_len < write_index)    {
                master_read_flag = 0u;
                I2C_0_send_nack();
                return;
			}
		}
		
		I2C_0_send_ack();
		write_index++;
	}
}
//[*]===================================================================================[*]
uint8_t i2c_slave_init(void)
{
	I2C_0_set_write_callback(I2C_read_handler);
	I2C_0_set_read_callback(I2C_write_handler);
	I2C_0_set_address_callback(I2C_address_handler);
	return 1;
}
//[*]===================================================================================[*]
//[*] Interrupt Service Routine
//[*]===================================================================================[*]
ISR(TWI0_TWIS_vect)
{
	if ((TWI0.SSTATUS & TWI_APIF_bm) && (TWI0.SSTATUS & TWI_AP_bm)) {
		I2C_address_handler();
		return;
	}
	// Address or stop condition
	if (TWI0.SSTATUS & TWI_APIF_bm) {
		if (TWI0.SSTATUS & TWI_AP_bm) {
			if (!(TWI0.SSTATUS & TWI_DIR_bm)) {
				I2C_read_handler();
			}
        }		
		TWI0.SSTATUS |= TWI_APIF_bm;
        i2c_clear();
	}

	// Data interrupt
	if (TWI0.SSTATUS & TWI_DIF_bm) {
		if (TWI0_SSTATUS & TWI_DIR_bm)  {
			//Master read
			I2C_write_handler();
		}
		else
		{
			I2C_read_handler();
		}
		TWI0_SSTATUS |= TWI_DIF_bm;
	}
}
//[*]===================================================================================[*]
void jump_to_address(uint16_t address);
//[*]===================================================================================[*]
//[*] Boot Main
//[*]===================================================================================[*]
uint8_t magic_num_check(void)
{
    uint8_t magic_num = 0u;
    
    magic_num = FLASH_0_read_flash_byte(LAST_DATA_ADDRESS);
    
    if(magic_num != 0xff)
        return 0;
        
    return 1;
}
uint32_t checksum = 0u;
//[*]===================================================================================[*]
void fw_checksum_check(void)
{
    uint8_t  page_data[64] = { 0x00, };
    
    checksum = 0u;
    
    // calculate checksum
    for(uint8_t pn=0; pn<PAGE_NUM; pn++)    {
        for (uint8_t ps = 0; ps < PAGE_SIZE; ps++) {
            page_data[ps] = FLASH_0_read_flash_byte((APP_ADDRESS + (pn*PAGE_SIZE)) + ps);
            checksum += page_data[ps];
        }
    }    
    
    return;
}
//[*]===================================================================================[*]
int main(void)
{   
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();
    
//    if(!magic_num_check())
//        fw_checksum_check();
    
	i2c_slave_init();

	ENABLE_INTERRUPTS();
   
	/* Replace with your application code */
	while (1) {
	}
	return 0;
}
//[*]===================================================================================[*]
void jump_to_address(uint16_t address)
{
    // Define a function pointer with the target address
    void (*jump_to_app)(void) = (void (*)(void)) address;

    I2C_0_close();

    // Disable interrupts before jumping
    cli();

    // Jump to the target address
    jump_to_app();
}
//[*]===================================================================================[*]