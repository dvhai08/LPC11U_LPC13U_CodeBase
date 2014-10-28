/**************************************************************************/
/*!
    @file     board_lpcxpresso11u68.c
    @author   K. Townsend (microBuilder.eu)

    @section DESCRIPTION

    Common, board-specific files for lpcxpresso LPC11U68 boards

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2014, K. Townsend
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#include "projectconfig.h"

#ifdef CFG_BRD_LPCXPRESSO_LPC11U37H

#include <string.h>

#include "boards/board.h"
#include "core/gpio/gpio.h"
#include "core/delay/delay.h"
#include "core/eeprom/eeprom.h"
#include "core/adc/adc.h"

#ifdef CFG_CHIBI
  #include "messages.h"
  #include "drivers/rf/802.15.4/chibi/chb.h"
  #include "drivers/rf/802.15.4/chibi/chb_drvr.h"
#endif

#ifdef CFG_USB
  #include "core/usb/usbd.h"
  #ifdef CFG_USB_CDC
    #include "core/usb/usb_cdc.h"
  #endif
#endif

#ifdef CFG_TFTLCD
  #include "drivers/displays/graphic/lcd.h"
#endif

#ifdef CFG_INTERFACE
  #include "cli/cli.h"
#endif

#ifdef CFG_PROTOCOL
  #include "protocol/protocol.h"
#endif

#ifdef CFG_ENABLE_UART
  #include "core/uart/uart.h"
#endif

#ifdef CFG_SDCARD
  #include "drivers/storage/fatfs/diskio.h"
  #include "drivers/storage/fatfs/ff.h"
#endif

#ifdef CFG_CC3000
  #include "drivers/rf/wifi/cc3000/wifi.h"
#endif

#include "core/ssp0/ssp0.h"

#ifdef CFG_SDCARD
/**************************************************************************/
/*!
    Handles timestamp requests for SD cards (adjust depending on if you
    want to use the RTC, or just return 0, etc.)
*/
/**************************************************************************/
DWORD get_fattime ()
{
  DWORD tmr = 0;

  // tmr =  (((DWORD)rtcYear - 80) << 25)
  //      | ((DWORD)rtcMon << 21)
  //      | ((DWORD)rtcMday << 16)
  //      | (WORD)(rtcHour << 11)
  //      | (WORD)(rtcMin << 5)
  //      | (WORD)(rtcSec >> 1);

  return tmr;
}
#endif

/**************************************************************************/
/*!
    @brief Board-specific initialisation function
*/
/**************************************************************************/
void boardInit(void)
{
  SystemCoreClockUpdate();
  delayInit();
  GPIOInit();

  #ifdef CFG_PRINTF_UART
    uartInit(CFG_UART_BAUDRATE);
  #endif

  /* Set user LED pin to output and disable it */
  LPC_GPIO->DIR[CFG_LED_PORT] |= (1 << CFG_LED_PIN);
  boardLED(CFG_LED_OFF);

  /* Start Chibi */
  #ifdef CFG_CHIBI
    /* You may need to write a new address to EEPROM if it doesn't exist */
    // uint16_t nodeaddr = 0xCAFE;
    // uint64_t ieeeaddr = 0x123456780000CAFE;
    // writeEEPROM((uint8_t*)CFG_EEPROM_CHIBI_NODEADDR, (uint8_t*)&nodeaddr, sizeof(nodeaddr));
    // writeEEPROM((uint8_t*)CFG_EEPROM_CHIBI_IEEEADDR, (uint8_t*)&ieeeaddr, sizeof(ieeeaddr));
    chb_init();
  #endif

  /* Initialise USB */
  #ifdef CFG_USB
    delay(500);
    usb_init();
  #endif

  /* Initialise the LCD if requested */
  #ifdef CFG_TFTLCD
    lcdInit();
  #endif

  /* Start the command line interface */
  #ifdef CFG_INTERFACE
    cliInit();
  #endif
  /* Init Protocol Module */
  #ifdef CFG_PROTOCOL
    prot_init();
  #endif
  /* Initialise the CC3000 WiFi module and connect to an AP */
  #ifdef CFG_CC3000
    /* Setup the CC3000 pins */
    LPC_IOCON->TRST_PIO0_14  &= ~0x07;
    LPC_IOCON->TRST_PIO0_14  |= 0x01;
    LPC_IOCON->PIO0_17       &= ~0x07;
    LPC_IOCON->PIO0_16       &= ~0x1F;
    LPC_IOCON->PIO0_16       |= (1<<4);
  #endif

  /* Initialise the SD Card? */
  #ifdef CFG_SDCARD
    // DSTATUS stat = disk_initialize(0);
  #endif

  /* Initialise ADC channel 1 (pin 0.12) */
  //LPC_IOCON->TMS_PIO0_12   &= ~0x9F;
  //LPC_IOCON->TMS_PIO0_12   |= 0x02;
  //adcInit();

  /* Turn the user LED on after init to indicate that everything is OK */
  boardLED(CFG_LED_ON);
}

#ifndef _TEST_

#define SPI_CS_ENABLE  do { GPIOSetBitValue(0, 2, 0); delay(1); } while(0)
#define SPI_CS_DISABLE do { GPIOSetBitValue(0, 2, 1); delay(1); } while(0)

#define CFG_ATPARSER_BUFSIZE     256
static char cmd_buffer[CFG_ATPARSER_BUFSIZE];
static char *ptr_cmd_buffer;

typedef enum
{
  SDEP_MSGTYPE_COMMAND          = 0x10,
  SDEP_MSGTYPE_RESPONSE         = 0x20,
  SDEP_MSGTYPE_ALERT            = 0x40,
  SDEP_MSGTYPE_ERROR            = 0x80
} sdepMsgType_t;

typedef struct __attribute__ ((packed)) {
  uint8_t msg_type;
  union
  {
    uint16_t cmd_id;
    struct
    {
      uint8_t cmd_id_low;
      uint8_t cmd_id_high;
    };
  };
  uint8_t length;
  uint8_t payload[255];
} sdepMsgCommand_t;

#define DEF_CHARACTER   0xFEu /**< SPI default character. Character clocked out in case of an ignored transaction. */
#define ORC_CHARACTER   0xFFu /**< SPI over-read character. Character clocked out after an over-read of the transmit buffer. */

uint8_t ssp0TrasnferOne(uint8_t data)
{
  uint8_t recv;
  ssp0Transfer(&recv, &data, 1);

  return recv;
}

void nrf_ssp0Send (uint8_t *buf, uint32_t length)
{

  while (length--)
  {
    // keep resending if Ignored Character is received
    while(1)
    {
      uint8_t fb;

      SPI_CS_ENABLE;
      fb = ssp0TrasnferOne(*buf);
      SPI_CS_DISABLE;

      if (fb != DEF_CHARACTER) break;
      delay(1); // wait a bit before retry
    }

    buf++;
  }
}

uint32_t nrf_ssp0Receive(uint8_t *buf, uint32_t length)
{
  for(uint32_t count=0; count<length; count++)
  {
    uint8_t ch;

    while(1)
    {
      SPI_CS_ENABLE;
      ssp0Receive(&ch, 1);
      SPI_CS_DISABLE;

      if (ch != DEF_CHARACTER) break;
      delay(1);
    }

    if (ch == ORC_CHARACTER) return count;
    *buf++ = ch;
  }

  return length;
}

error_t atparser_task(void)
{
  while( uartRxBufferDataPending() )
  {
    uint8_t ch = uartRxBufferRead();

    switch(ch)
    {
      case '\r':
      case '\n':
        // Execute command when getting either \r or \n. Ignoring the next \n or \r
        if ( ptr_cmd_buffer > cmd_buffer )
        {
          uartSendByte('\n');
          *ptr_cmd_buffer = 0; // Null char
          ptr_cmd_buffer = cmd_buffer;

          if( 0 == strcmp("atz", cmd_buffer) ) NVIC_SystemReset();

          sdepMsgCommand_t cmdHeader =
          {
              .msg_type = SDEP_MSGTYPE_COMMAND,
              .cmd_id = 0,
              .length = strlen(cmd_buffer)
          };

          // send command
          nrf_ssp0Send( (uint8_t*)&cmdHeader, 4);
          nrf_ssp0Send( (uint8_t*)cmd_buffer, strlen(cmd_buffer));

          // receive response
          sdepMsgCommand_t cmdResponse = { 0 };

          uint8_t sync =0;
          do{
            delay(10);
            nrf_ssp0Receive(&sync, 1);
          }while(sync != SDEP_MSGTYPE_RESPONSE);

          // response header
          cmdResponse.msg_type = sync;
          nrf_ssp0Receive((uint8_t*)&cmdResponse.cmd_id, 3);

          uint16_t len = nrf_ssp0Receive(cmdResponse.payload, 255);

          cmdResponse.payload[len] = 0;

          printf("MType: 0x%02x - CMD: 0x%04x - Len: %d\n",
                 cmdResponse.msg_type,
                 (cmdResponse.cmd_id_high << 8) + cmdResponse.cmd_id_low,
                 cmdResponse.length);

          printf("%s\n", cmdResponse.payload);
        }
      break;

      case '\b':
        if ( ptr_cmd_buffer > cmd_buffer )
        {
          printf("\b \b");
          ptr_cmd_buffer--;
        }
      break;

      default:
        uartSendByte(ch);
        *ptr_cmd_buffer++ = ch;
      break;
    }
  }

  return ERROR_NONE;
}

int main(void)
{
  uint32_t currentSecond, lastSecond;
  currentSecond = lastSecond = 0;

  /* Configure the HW */
  boardInit();

  // set P0_2 to SSEL0 (function 1)
//  LPC_IOCON->PIO0_2 = bit_set_range(LPC_IOCON->PIO0_2, 0, 2, 1);
//  LPC_IOCON->PIO0_2 = bit_set_range(LPC_IOCON->PIO0_2, 3, 4, GPIO_MODE_PULLUP);
  GPIOSetDir(0, 2, 1);
  GPIOSetBitValue(0, 2, 1);

  ssp0Init();
  delay(500);

  printf("hello world\n");

  memset(cmd_buffer, 0, sizeof(cmd_buffer));
  ptr_cmd_buffer = cmd_buffer;

  while (1)
  {
    /* Blinky (1Hz) */
    currentSecond = delayGetSecondsActive();
    if (currentSecond != lastSecond)
    {
      lastSecond = currentSecond;
      boardLED(lastSecond % 2);
    }

    atparser_task();

    /* Check for binary protocol input if CFG_PROTOCOL is enabled */
    #ifdef CFG_PROTOCOL
      prot_exec(NULL);
    #endif

    /* Poll for CLI input if CFG_INTERFACE is enabled */
    #ifdef CFG_INTERFACE
      cliPoll();
    #endif

    /* Optionally enter high level sleep mode here via WFI */
  }
}
#endif /* !_TEST_ */

/**************************************************************************/
/*!
    @brief Turns the LED(s) on or off
*/
/**************************************************************************/
void boardLED(uint8_t state)
{
  if (state)
  {
    LPC_GPIO->SET[CFG_LED_PORT] = (1 << CFG_LED_PIN);
  }
  else
  {
    LPC_GPIO->CLR[CFG_LED_PORT] = (1 << CFG_LED_PIN);
  }
}

/**************************************************************************/
/*!
    @brief  Configure the board for low power and enter sleep mode
*/
/**************************************************************************/
void boardSleep(void)
{
  // ToDo!
}

/**************************************************************************/
/*!
    @brief  Restores parts and system peripherals to an appropriate
            state after waking up from sleep mode
*/
/**************************************************************************/
void boardWakeup(void)
{
  // ToDo!
}

#endif /* !CFG_BRD_LPCXPRESSO_LPC11U37H */
