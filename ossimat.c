#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/spi.h>
#include <hardware/vreg.h>
#include <pico/stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "joybus.pio.h"
#include "psxcontroller.pio.h"

typedef struct
{
    PIO pio;
    uint sm;
    uint program_offset;
    uint data_pin;
} JoybusHost;

typedef struct
{
    PIO pio;
    uint sm;
    uint program_offset;

    uint transaction_idx;

    uint16_t controller_type; 
    uint response;
} PSXController;

bool read_tx_fifo_timeout(PIO pio, uint sm, uint* result, uint timeout_us)
{
    uint64_t start_time = time_us_64();
    while (pio_sm_is_rx_fifo_empty(pio, sm))
    {
        if ((uint)(time_us_64() - start_time) >= timeout_us)
            return false;
    }

    *result = pio_sm_get(pio, sm);
    return true;
}

void joybus_program_init(PIO pio, uint sm, uint offset, uint data_pin)
{
    pio_sm_config c = joybus_program_get_default_config(offset);

    pio_gpio_init(pio, data_pin);

    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 1, false);

    sm_config_set_in_shift(&c, false, true, 8);
    sm_config_set_out_shift(&c, false, true, 8);

    float div = clock_get_hz(clk_sys) / (16.0 / 0.000001);
    sm_config_set_clkdiv(&c, div);

    sm_config_set_in_pins(&c, data_pin);
    sm_config_set_set_pins(&c, data_pin, 1);
    sm_config_set_out_pins(&c, data_pin, 1);
    sm_config_set_jmp_pin(&c, data_pin);

    pio_sm_init(pio, sm, offset, &c);
}

void joybus_host_init(JoybusHost* host, PIO pio, uint data_pin)
{
    host->pio = pio;
    host->program_offset = pio_add_program(pio, &joybus_program);

    host->sm = pio_claim_unused_sm(pio, true);
    host->data_pin = data_pin;
}

bool joybus_host_transaction(JoybusHost* host,
    uint8_t* outdata,
    uint outdata_len,
    uint8_t* indata,
    uint indata_len)
{
    joybus_program_init(host->pio, host->sm, host->program_offset, host->data_pin);

    uint outdata_bits = outdata_len * 8 - 1;
    pio_sm_put_blocking(host->pio, host->sm, (outdata_bits & 0xFF) << 24);
    for (uint i = 0; i < outdata_len; i++)
    {
        pio_sm_put_blocking(host->pio, host->sm, (uint)outdata[i] << 24);
    }
    pio_sm_set_enabled(host->pio, host->sm, true);

    sleep_us(outdata_len * 8 * 4 + 32);

    for (uint i = 0; i < indata_len; i++)
    {
        uint value;
        if (!read_tx_fifo_timeout(host->pio, host->sm, &value, 8 * 4 * 2))
            return false;
        indata[i] = (uint8_t)value;
    }

    return true;
}

enum
{
    psxcontroller_sel_pin,
    psxcontroller_clk_pin,
    psxcontroller_cmd_pin,
    psxcontroller_ack_pin,
    psxcontroller_dat_pin,
};

void psxcontroller_program_init(PIO pio,
    uint sm,
    uint offset,
    uint pins_start)
{
    pio_sm_config c = psxcontroller_program_get_default_config(offset);

    for (uint i = 0; i < 5; i++)
        pio_gpio_init(pio, pins_start + i);

    pio_sm_set_consecutive_pindirs(pio, sm, pins_start, 5, false);
    pio_sm_set_pins(pio, sm, 0);

    sm_config_set_in_shift(&c, true, true, 8);
    sm_config_set_out_shift(&c, true, true, 8);

    float div = clock_get_hz(clk_sys) / (8.0 / 0.000001);
    sm_config_set_clkdiv(&c, div);

    pio_interrupt_clear(pio, 0);
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);

    sm_config_set_in_pins(&c, pins_start + psxcontroller_cmd_pin);
    sm_config_set_out_pins(&c, pins_start + psxcontroller_dat_pin, 1);
    sm_config_set_set_pins(&c, pins_start + psxcontroller_ack_pin, 2);

    pio_sm_init(pio, sm, offset, &c);
}

void psxcontroller_init(PSXController* controller, PIO pio, uint pins_start, uint16_t controller_type)
{
    controller->pio = pio;
    controller->program_offset = pio_add_program(pio, &psxcontroller_program);

    controller->sm = pio_claim_unused_sm(pio, true);
    controller->transaction_idx = 0;
    controller->controller_type = controller_type;
    controller->response = 0;

    psxcontroller_program_init(controller->pio, controller->sm, controller->program_offset, pins_start);
    // first dummy byte, otherwise the sm would get stuck
    pio_sm_put_blocking(controller->pio, controller->sm, 0);
    pio_sm_set_enabled(controller->pio, controller->sm, true);
}

void psxcontroller_set_response(PSXController* controller, uint response)
{
    controller->response = response;
}

PSXController* serving_psxcontroller = NULL;

void psxcontroller_serve_int()
{
    PSXController* controller = serving_psxcontroller;
    uint byte = pio_sm_get_blocking(controller->pio, controller->sm) >> 24;

    uint cur_idx = controller->transaction_idx;

    uint not_done = 0xFFFFFFFF, response = 0;
    controller->transaction_idx++;

    switch (cur_idx)
    {
    case 0:
        if (byte != 0x01)
            not_done = 0;
        else
            response = controller->controller_type & 0xFF;
        break;
    case 1:
        if (byte != 0x42)
            not_done = 0;
        else
            response = (controller->controller_type >> 8) & 0xFF;
        break;
    case 2:
        response = controller->response & 0xFF;
        break;
    case 3:
        response = (controller->response >> 8) & 0xFF;
        break;
    case 4:
        {
            gpio_init(PICO_DEFAULT_LED_PIN);
            gpio_set_dir(PICO_DEFAULT_LED_PIN, true);
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
        }
    default:
        not_done = 0;
        break;
    }

    if (!not_done)
        controller->transaction_idx = 0;

    pio_sm_put_blocking(controller->pio, controller->sm, not_done);
    pio_sm_put_blocking(controller->pio, controller->sm, response);

    pio_interrupt_clear(controller->pio, 0);
}

int main()
{
    const uint joybus_pin = 0;
    const uint psxcontroller_pins_start = 6;

    gpio_disable_pulls(joybus_pin);
    for (int i = 0; i < 5; i++)
    {
        gpio_disable_pulls(psxcontroller_pins_start + i);
    }

    set_sys_clock_khz(144000, true);
    stdio_init_all();

    PSXController psxcontroller;
    serving_psxcontroller = &psxcontroller;
    psxcontroller_init(&psxcontroller, pio1, psxcontroller_pins_start, 0x5A41);
    psxcontroller_set_response(&psxcontroller, 0xFFFF);
    irq_set_exclusive_handler(PIO1_IRQ_0, psxcontroller_serve_int);
    irq_set_enabled(PIO1_IRQ_0, true);

    JoybusHost joybushost;
    joybus_host_init(&joybushost, pio0, joybus_pin);

    while (true)
    {
        uint16_t response = 0xFFFF;
        uint8_t outdata[] = {0x40};
        uint8_t indata[3];
        if (joybus_host_transaction(&joybushost, outdata, 1, indata, 3))
        {
            // A -> circle
            if (indata[0] & (1<<0))
                response &= ~(1<<13);
            // B -> cross
            if (indata[0] & (1<<1))
                response &= ~(1<<14);
            // start
            if (indata[0] & (1<<4))
                response &= ~(1<<3);
            // z -> select
            if (indata[1] & (1<<4))
                response &= ~(1<<0);
            // left
            if (indata[1] & (1<<0))
                response &= ~(1<<7);
            // right
            if (indata[1] & (1<<1))
                response &= ~(1<<5);
            // down
            if (indata[1] & (1<<2))
                response &= ~(1<<6);
            // up
            if (indata[1] & (1<<3))
                response &= ~(1<<4);
        }

        irq_set_enabled(PIO1_IRQ_0, false);
        psxcontroller_set_response(&psxcontroller, response);
        irq_set_enabled(PIO1_IRQ_0, true);
        sleep_ms(2);
        tight_loop_contents();
    }

    return 0;
}
