#include <system.h>
#include <altera_avalon_spi.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/alt_irq.h>
#include "sys/alt_alarm.h"
#include "altera_avalon_pio_regs.h"

#include "touch_spi.h"

#define ALT_ENHANCED_INTERRUPT_API_PRESENT


// internal data structure
#define ACTIVE_DELAY_TIME   (alt_ticks_per_second()/60)
#define SAMPLE_RATE         60  // times per seconds
#define FIFO_SIZE           10

typedef struct{
    alt_u32 spi_base;
    alt_u32 penirq_base;
    alt_u32 penirq_irq;
    alt_u32 irq_mask;
    alt_u16 fifo_front;
    alt_u16 fifo_rear;
    alt_u16 fifo_x[FIFO_SIZE];
    alt_u16 fifo_y[FIFO_SIZE];
    int pen_pressed;
}TERASIC_TOUCH_PANEL;

// prototype of internal function
void touch_isr(void* context);
void touch_get_xy(TERASIC_TOUCH_PANEL *p);
void touch_xy_transform(int *x, int *y);
void touch_enable_penirq(TERASIC_TOUCH_PANEL *p);
int touch_is_pen_pressed(TERASIC_TOUCH_PANEL *p);
void touch_empty_fifo(TERASIC_TOUCH_PANEL *p);
void touch_clear_input(TERASIC_TOUCH_PANEL *p);

// init touch panel
TOUCH_HANDLE Touch_Init(const alt_u32 spi_base, const alt_u32 penirq_base, const alt_u32 penirq_irq){
    int bSuccess = TRUE;
    
    TERASIC_TOUCH_PANEL *p;
    
    p = malloc(sizeof(TERASIC_TOUCH_PANEL));
    if (!p)
        return p;

   // memset(p, 0, sizeof(TERASIC_TOUCH_PANEL));
    p->spi_base = spi_base;
    p->penirq_base = penirq_base;
    p->irq_mask = 0x01;  // 1-pin
    p->penirq_irq = penirq_irq;

    // enalbe penirq_n interrupt (P1=1, P1=0)
    touch_enable_penirq(p);    
    
    // enable interrupt, 1-pin
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(p->penirq_base, p->irq_mask); 
    // Reset the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(p->penirq_base,0);

    // register ISR
    // register callback function
    if ((alt_ic_isr_register(TOUCH_PEN_IRQ_N_IRQ_INTERRUPT_CONTROLLER_ID, p->penirq_irq,
    		touch_isr, (void *)p, NULL) != 0)){
        bSuccess = FALSE;
    }

    if (!bSuccess && p){
        free(p);
        p = NULL;
    }

    return p;
}

// uninit touch panel
void Touch_UnInit(TOUCH_HANDLE pHandle){
    TERASIC_TOUCH_PANEL *p = (TERASIC_TOUCH_PANEL *)pHandle;
    if (!p)
        return;

    // disable irq
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(p->penirq_base, 0x00);
    
    // zero varialbe
    free(p);
}


void Touch_EmptyFifo(TOUCH_HANDLE pHandle){
    TERASIC_TOUCH_PANEL *p = (TERASIC_TOUCH_PANEL *)pHandle;
    touch_empty_fifo(p);
}

// get x/y from internal FIFO
int Touch_GetXY(TOUCH_HANDLE pHandle, int *x, int *y){
    TERASIC_TOUCH_PANEL *p = (TERASIC_TOUCH_PANEL *)pHandle;
    if (!p)
        return FALSE;    
    
    if (p->fifo_front == p->fifo_rear){
        // empty
        return FALSE;
    }
    *x = p->fifo_x[p->fifo_rear];
    *y = p->fifo_y[p->fifo_rear];
    //
    p->fifo_rear++;
    p->fifo_rear %= FIFO_SIZE;
    //
    // translate
    touch_xy_transform(x, y);
    
    return TRUE;
}



// penirq_n ISR
void touch_isr(void* context){
    TERASIC_TOUCH_PANEL *p = (TERASIC_TOUCH_PANEL *)context;

    touch_get_xy(p);
    // get the edge capture mask
    IORD_ALTERA_AVALON_PIO_EDGE_CAP(p->penirq_base);
    //if ((mask & Touch.irq_mask) == 0)  // 1-pin
    //    return;
    usleep(2000);
    
    // Reset the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(p->penirq_base,0);    

}

void touch_empty_fifo(TERASIC_TOUCH_PANEL *p){
    p->fifo_rear = p->fifo_front;
}

int touch_is_pen_pressed(TERASIC_TOUCH_PANEL *p){
    int bPressed;
    bPressed = (IORD(p->penirq_base, 0) & 0x01)?FALSE:TRUE;
    return bPressed;
}


void touch_enable_penirq(TERASIC_TOUCH_PANEL *p){
    const alt_u8 CommandGetX = 0x82;
    const alt_u8 CommandGetY = 0xD2;
    alt_u8 data8;
    
    alt_avalon_spi_command(p->spi_base, 0, sizeof(CommandGetX), &CommandGetX, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(data8), (alt_u8*)&data8, ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(data8), (alt_u8*)&data8, ALT_AVALON_SPI_COMMAND_TOGGLE_SS_N);
   
    alt_avalon_spi_command(p->spi_base, 0, sizeof(CommandGetY), &CommandGetY, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(data8), (alt_u8*)&data8, ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(data8), (alt_u8*)&data8, ALT_AVALON_SPI_COMMAND_TOGGLE_SS_N);
}

void touch_clear_input(TERASIC_TOUCH_PANEL *p){
    touch_enable_penirq(p);
}

// get x/y by SPI command
void touch_get_xy(TERASIC_TOUCH_PANEL *p){
    alt_u16 x, y;
    int result;
    const alt_u8 CommandGetX = 0x92;
    const alt_u8 CommandGetY = 0xD2;
    alt_u16 ResponseX, ResponseY;
    alt_u8 high_byte, low_byte;

    // x
    result = alt_avalon_spi_command(p->spi_base, 0, sizeof(CommandGetX), &CommandGetX, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
    result = alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(high_byte), (alt_u8*)&high_byte, ALT_AVALON_SPI_COMMAND_MERGE);
    if (result != sizeof(high_byte)){

        return;
    }          
    result = alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(low_byte), (alt_u8*)&low_byte, ALT_AVALON_SPI_COMMAND_TOGGLE_SS_N);
    if (result != sizeof(low_byte)){
        return;
    }  
    ResponseX = (high_byte << 8) | low_byte;        
    
    // y
    result = alt_avalon_spi_command(p->spi_base, 0, sizeof(CommandGetY), &CommandGetY, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
    result = alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(high_byte), (alt_u8*)&high_byte, ALT_AVALON_SPI_COMMAND_MERGE);
    if (result != sizeof(high_byte)){
        return;
    }          
    result = alt_avalon_spi_command(p->spi_base, 0, 0, 0, sizeof(low_byte), (alt_u8*)&low_byte, ALT_AVALON_SPI_COMMAND_TOGGLE_SS_N);
    if (result != sizeof(low_byte)){
        return;
    }  
    ResponseY = (high_byte << 8) | low_byte;        

    if (!touch_is_pen_pressed(p))
        return; // do not use this data    
        
    x = (ResponseX >> 3 ) & 0xFFF;  // 12 bits    
    y = (ResponseY >> 3 ) & 0xFFF;  // 12 bits
    
    
    // check whether the fifo is full!
    if (((p->fifo_front+1)%FIFO_SIZE) == p->fifo_rear){
        // full, pop an old one
        p->fifo_rear++;
        p->fifo_rear %= FIFO_SIZE;        
        
    }

    // push now
    p->fifo_x[p->fifo_front] = x;
    p->fifo_y[p->fifo_front] = y;    
    p->fifo_front++;
    p->fifo_front %= FIFO_SIZE;   
    
}

void touch_xy_transform(int *x, int *y){
    int xx, yy;
    const int y_ignore = 200;
    xx = *y;
    yy = *x;

    // scale & swap
   // xx = 4096 -1 - xx;
    xx = xx * X_RES / 4096;

    // special calibrate for LT24
    if (yy > (4096-y_ignore))
    	yy = 4096-y_ignore;
    yy = yy * 4095/ (4096-y_ignore);
    
    yy = yy * Y_RES / 4096;

    // swap
    *x = xx;
    *y = yy;    
    
}

int IsPtInRect(POINT *pt, RECT *rc){
    int bYes = FALSE;

    if (pt->x >= rc->left && pt->x <= rc->right && pt->y >= rc->top && pt->y <= rc->bottom)
        bYes = TRUE;

    return bYes;
}

void PtSet(POINT *pt, int x, int y){
    pt->x = x;
    pt->y = y;
}
