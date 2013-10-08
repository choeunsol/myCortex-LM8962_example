//*****************************************************************************
//
// enet_thermometer.c - Example main source file
//
// Copyright (c) 2003-2013 Withrobot, Inc.  All rights reserved.
//
// Software License Agreement
//
// Withrobot, Inc.(Withrobot) is supplying this software for use solely and
// exclusively on Withrobot's products.
//
// The software is owned by Withrobot and/or its suppliers, and is protected
// under applicable copyright laws.  All rights are reserved.
// Any use in violation of the foregoing restrictions may subject the user
// to criminal sanctions under applicable laws, as well as to civil liability
// for the breach of the terms and conditions of this license.
//
// THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
// OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
// WITHROBOT SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
// OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
//
//*****************************************************************************


/*
 * �� ������ myCortex-LM8962 ���� ���忡�� �̴��� ������ Ȯ���ϱ� ���� ������ �������� �����
 * �����̴�.
 *
 * DEFAULT_IPADDR0~DEFAULT_IPADDR03�� ���� �����ؼ� myCortex-LM8962 ������ IP�ּҸ�
 * ������ �� �ִ�. ��Ʈ��ũ ȯ���� ���ͳ� ������ ȯ���� ��õ�Ѵ�. �����⿡�� 1���� �̴��� ������
 * �����ϰ�, ������ ���� PC ���� ���� ������ �Ʒ��� ������ �����ϰ� ������ �� �ִ�.
 *
 * ����ϱ⸦ ���ϴ� IP �ּҸ� �����ϰ� ������ �� �߿�� myCortex-LM8962 ���� ���忡 �ٿ�
 * �ε��� ���� �̴��� ���̺��� J3�� �����ϰ� ������ �ΰ��Ѵ�. ������ IP �ּҸ� �̿��Ͽ� PC����
 * ���ͳ� �������� �����Ѵ�. ���� ��� IP�� 192.168.10.175�� �����ߴٸ� �Ʒ� �ּҷ� ������
 * ��.
 * 			http://192.168.10.175
 * ���ͳ� �������� ������ �ؽ�Ʈ�� ��µǰ� ���� �µ��� ���� �� ������ ���� �����ϴ� ���̴�.
 */



#include <stdio.h>
#include <string.h>

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"

#include "sysctl.h"
#include "systick.h"
#include "gpio.h"
#include "adc.h"
#include "timer.h"
#include "ethernet.h"
#include "interrupt.h"

#include "uip.h"
#include "uip_arp.h"
#include "httpd.h"

#include "llio.h"





// Macro for accessing the Ethernet header information in the buffer.
#define BUF                     ((struct uip_eth_hdr *)&uip_buf[0])


//
// UIP Timers (in MS)
//
#define UIP_PERIODIC_TIMER_MS   500
#define UIP_ARP_TIMER_MS        10000





//*****************************************************************************
//
// Default TCP/IP Settings for this application.
//
// Default to Link Local address ... (169.254.1.0 to 169.254.254.255).  Note:
// This application does not implement the Zeroconf protocol.  No ARP query is
// issued to determine if this static IP address is already in use.
//
//*****************************************************************************
#define DEFAULT_IPADDR0         192
#define DEFAULT_IPADDR1         168
#define DEFAULT_IPADDR2         10
#define DEFAULT_IPADDR3         175

#define DEFAULT_NETMASK0        255
#define DEFAULT_NETMASK1        255
#define DEFAULT_NETMASK2        255
#define DEFAULT_NETMASK3        0






static volatile long lPeriodicTimer;   // SysTick�� ���� �����Ǵ� �ð�
static volatile long lARPTimer;


volatile long g_temp;           // ���� �µ�.
static unsigned long g_tick;    // tick count
unsigned long g_runtime;        // �ý��� ��Ÿ��



#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif



static void SysTickIntHandler(void);
static void EthernetIntHandler(void);
static void ADCIntHandler(void);



int main(void)
{
    uip_ipaddr_t ipaddr;
    static struct uip_eth_addr sTempAddr;
    long lPacketLength;
    unsigned long ulTemp;


    //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_8MHZ);


    //
    // Enable and Reset the Ethernet Controller.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);


    //
    // Enable Port F for Ethernet LEDs.
    //  LED0        Bit 3   Output
    //  LED1        Bit 2   Output
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

    //
    // Intialize the Ethernet Controller and disable all Ethernet Controller
    // interrupt sources.
    //
    EthernetIntDisable(ETH_BASE, (ETH_INT_PHY | ETH_INT_MDIO | ETH_INT_RXER |
                       ETH_INT_RXOF | ETH_INT_TX | ETH_INT_TXER | ETH_INT_RX));
    ulTemp = EthernetIntStatus(ETH_BASE, false);
    EthernetIntClear(ETH_BASE, ulTemp);

    //
    // Initialize the Ethernet Controller for operation.
    //
    EthernetInitExpClk(ETH_BASE, SysCtlClockGet());

    //
    // Configure the Ethernet Controller for normal operation.
    // - Full Duplex
    // - TX CRC Auto Generation
    // - TX Padding Enabled
    //
    EthernetConfigSet(ETH_BASE, (ETH_CFG_TX_DPLXEN | ETH_CFG_TX_CRCEN |
                                 ETH_CFG_TX_PADEN));

    //
    // Enable the Ethernet Controller.
    //
    EthernetEnable(ETH_BASE);

    //
    // Enable the Ethernet interrupt.
    //
    IntEnable(INT_ETH);

    //
    // Enable the Ethernet RX Packet interrupt source.
    //
    EthernetIntRegister(ETH_BASE, EthernetIntHandler);
    EthernetIntEnable(ETH_BASE, ETH_INT_RX);

    //
    // Enable all processor interrupts.
    //
    IntMasterEnable();

    //
    // Initialize the uIP TCP/IP stack.
    //
    uip_init();
    uip_ipaddr(ipaddr, DEFAULT_IPADDR0, DEFAULT_IPADDR1, DEFAULT_IPADDR2,
               DEFAULT_IPADDR3);
    uip_sethostaddr(ipaddr);
    uip_ipaddr(ipaddr, DEFAULT_NETMASK0, DEFAULT_NETMASK1, DEFAULT_NETMASK2,
               DEFAULT_NETMASK3);
    uip_setnetmask(ipaddr);

    // ����������� ����ϴ� MAC �ּ�.
    // �� �ּҴ� �������� IEEE�� ���� �Ҵ���� �ּҰ� �ƴϴ�.
    // ��� ��ǰ�� ����ϴ� ��쿡�� ���� MAC �ּҸ� �Ҵ�޾� ����ؾ� �Ѵ�.
    sTempAddr.addr[0] = 0xb6;
    sTempAddr.addr[1] = 0x1a;
    sTempAddr.addr[2] = 0x00;
    sTempAddr.addr[3] = 0x67;
    sTempAddr.addr[4] = 0x45;
    sTempAddr.addr[5] = 0x23;


    //
    // Program the hardware with it's MAC address (for filtering).
    //
    EthernetMACAddrSet(ETH_BASE, (unsigned char *)&sTempAddr);
    uip_setethaddr(sTempAddr);

    //
    // Initialize the TCP/IP Application (e.g. web server).
    //
    httpd_init();



    //
    // Configure SysTick for a periodic interrupt.
    //
    SysTickPeriodSet(SysCtlClockGet() / 100);       // 100Hz timer
    SysTickIntRegister(SysTickIntHandler);
    SysTickEnable();
    SysTickIntEnable();



    //
    // Configure ADC
    // 1�� �������� �µ� ������ ADC�Ͽ� ���� �µ��� ���� ������ ������ �д�.
    // client���� http request�� ������ �� ���� ������ ����� �µ� ����
    // HTML�� ����� �����Ѵ�.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC);
    SysCtlADCSpeedSet(SYSCTL_ADCSPEED_500KSPS);

    // ���� �µ� ���� ä�θ����� ADC sequence�� �����Ѵ�.
    ADCSequenceDisable(ADC_BASE, 0);
    ADCSequenceConfigure(ADC_BASE, 0, ADC_TRIGGER_TIMER, 0);

    ADCSequenceStepConfigure(ADC_BASE, 0, 0, ADC_CTL_TS | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceEnable(ADC_BASE, 0);

    ADCIntRegister(ADC_BASE, 0, ADCIntHandler);
    ADCIntEnable(ADC_BASE, 0);


    //
    // Configure Timer to trigger ADC
    // �� 1�ʸ��� �ڵ����� ADC �� �� �ֵ��� ADC trigger timer�� �����Ѵ�.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerConfigure( TIMER0_BASE, TIMER_CFG_32_BIT_PER );
    TimerControlTrigger(TIMER0_BASE, TIMER_A, true);
    TimerLoadSet( TIMER0_BASE, TIMER_A, SysCtlClockGet() / 1 );     // 1Hz trigger
    TimerEnable( TIMER0_BASE, TIMER_A );


    lPeriodicTimer = 0;
    lARPTimer = 0;


    //
    // Loop forever.
    //
    while(1)
    {
        SysCtlSleep();      // SysTick/ADC/ETH ���ͷ�Ʈ�� �߻��� �� ���� sleep


        //
        // Check for an RX Packet and read it.
        //
        lPacketLength = EthernetPacketGetNonBlocking(ETH_BASE, uip_buf, sizeof(uip_buf));
        if(lPacketLength > 0)
        {
            //
            // Set uip_len for uIP stack usage.
            //
            uip_len = (unsigned short)lPacketLength;

            //
            // Renable RX Packet interrupts.
            //
            EthernetIntEnable(ETH_BASE, ETH_INT_RX);

            //
            // Process incoming IP packets here.
            //
            if(BUF->type == htons(UIP_ETHTYPE_IP))
            {
                uip_arp_ipin();
                uip_input();

                //
                // should be sent out on the network, the global variable
                // uip_len is set to a value > 0.
                //
                if(uip_len > 0)
                {
                    uip_arp_out();
                    EthernetPacketPut(ETH_BASE, uip_buf, uip_len);
                    uip_len = 0;
                }
            }

            //
            // Process incoming ARP packets here.
            //
            else if(BUF->type == htons(UIP_ETHTYPE_ARP))
            {
                uip_arp_arpin();

                //
                // If the above function invocation resulted in data that
                // should be sent out on the network, the global variable
                // uip_len is set to a value > 0.
                //
                if(uip_len > 0)
                {
                    EthernetPacketPut(ETH_BASE, uip_buf, uip_len);
                    uip_len = 0;
                }
            }
        }

        //
        // Process TCP/IP Periodic Timer here.
        //
        if(lPeriodicTimer > UIP_PERIODIC_TIMER_MS)
        {
            lPeriodicTimer = 0;
            for(ulTemp = 0; ulTemp < UIP_CONNS; ulTemp++)
            {
                uip_periodic(ulTemp);

                //
                // If the above function invocation resulted in data that
                // should be sent out on the network, the global variable
                // uip_len is set to a value > 0.
                //
                if(uip_len > 0)
                {
                    uip_arp_out();
                    EthernetPacketPut(ETH_BASE, uip_buf, uip_len);
                    uip_len = 0;
                }
            }

#if UIP_UDP
            for(ulTemp = 0; ulTemp < UIP_UDP_CONNS; ulTemp++)
            {
                uip_udp_periodic(i);

                //
                // If the above function invocation resulted in data that
                // should be sent out on the network, the global variable
                // uip_len is set to a value > 0.
                //
                if(uip_len > 0)
                {
                    uip_arp_out();
                    EthernetPacketPut(ETH_BASE, uip_buf, uip_len);
                    uip_len = 0;
                }
            }
#endif // UIP_UDP
        }

        //
        // Process ARP Timer here.
        //
        if(lARPTimer > UIP_ARP_TIMER_MS)
        {
            lARPTimer = 0;
            uip_arp_timer();
        }

    }
}




//
// SysTick tiemr interrupt handler
//
static void SysTickIntHandler(void)
{
    // SysTick timer�� 100Hz �ֱ�� �����Ǿ� �����Ƿ� 10ms �������� ���ͷ�Ʈ �߻�.
    lPeriodicTimer += 10;
    lARPTimer += 10;

    g_tick++;

    if (g_tick % 100 == 0)      // 1�� ���
    {
        g_runtime++;
    }

}




//
// The interrupt handler for the Ethernet interrupt.
//
static void EthernetIntHandler(void)
{
    unsigned long ulTemp;

    //
    // Read and Clear the interrupt.
    //
    ulTemp = EthernetIntStatus(ETH_BASE, false);
    EthernetIntClear(ETH_BASE, ulTemp);

    //
    // Check to see if an RX Interrupt has occured.
    //
    if(ulTemp & ETH_INT_RX)
    {
        //
        // Disable Ethernet RX Interrupt.
        //
        EthernetIntDisable(ETH_BASE, ETH_INT_RX);
    }
}





/*
 * ADC Interrupt Handler
 */
static void ADCIntHandler(void)
{
	unsigned long adc_result[8];
	double temp;

	ADCIntClear(ADC_BASE, 0);

	ADCSequenceDataGet(ADC_BASE, 0, adc_result);

	temp = (2.7 - 3.0 * (double)adc_result[0] / 1024.0) * 75.0 - 55.0;
	g_temp = (unsigned long)(temp * 100);

}


