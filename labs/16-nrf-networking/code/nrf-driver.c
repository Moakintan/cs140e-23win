#include "nrf.h"
#include "nrf-hw-support.h"

// enable crc, enable 2 byte
#   define set_bit(x) (1<<(x))
#   define enable_crc      set_bit(3)
#   define crc_two_byte    set_bit(2)
#   define mask_int         set_bit(6)|set_bit(5)|set_bit(4)
enum {
    // pre-computed: can write into NRF_CONFIG to enable TX.
    tx_config = enable_crc | crc_two_byte | set_bit(PWR_UP) | mask_int,
    // pre-computed: can write into NRF_CONFIG to enable RX.
    rx_config = tx_config  | set_bit(PRIM_RX)
} ;

nrf_t * nrf_init(nrf_conf_t c, uint32_t rxaddr, unsigned acked_p) {
    return staff_nrf_init(c, rxaddr, acked_p);
    //return staff_nrf_init();

    //1. You need to setup GPIO and SPI first or nothing will work (see thecode above).
    nrf_t *n = kmalloc(sizeof *n);
    n->config = c;
    nrf_stat_start(n);
    n->spi = pin_init(c.ce_pin, c.spi_chip);
    n->rxaddr = rxaddr;
    

    unsigned v = 0; // temp var to hold values
    // First, power off and set to known state for state machine
    nrf_put8_chk(n, NRF_CONFIG, v);
    assert(!nrf_is_pwrup(n));
    // 2. You must put the chip in "power down" mode before you change the configure.
    //bit_set(PWR_UP,0);
    //nrf_set_pwrup_off(n);
    //PWR_UP(0x0);
    //set channel
    nrf_put8_chk(n, NRF_EN_RXADDR, v);

    if (!acked_p) {
        // Non-ack mode
        // disable auto acknowledgement
        nrf_put8_chk(n, NRF_EN_AA, v);
        // enable pipe 1 only
        v = set_bit(1);
        nrf_put8_chk(n, NRF_EN_RXADDR, v);
    } else {
        // enable auto-ack
        v = set_bit(0) | set_bit(1);
        nrf_put8_chk(n, NRF_EN_AA, v);
        // enable pipes 1 and 2
        nrf_put8_chk(n, NRF_EN_RXADDR, v);

        unsigned rt_cnt = nrf_default_retran_attempts;
        unsigned rt_d = 0b0111;  // config code for 2000 usec

        // setup retransmit delay and retry counts
        v = (rt_d << 4) | rt_cnt;
        nrf_put8_chk(n, NRF_SETUP_RETR, v);
    }
    v = nrf_default_addr_nbytes - 2;
    nrf_put8_chk(n, NRF_SETUP_AW, v);

    nrf_set_addr(n, NRF_TX_ADDR, 0x0, nrf_default_addr_nbytes);

    nrf_put8_chk(n, NRF_RX_PW_P0, 0x0);
    nrf_set_addr(n, NRF_RX_ADDR_P0, 0x0, nrf_default_addr_nbytes);

    // disable tx server
    nrf_put8_chk(n, NRF_RX_PW_P1, c.nbytes);
    nrf_set_addr(n, NRF_RX_ADDR_P1, n->rxaddr, nrf_default_addr_nbytes);

    // setup channel
    nrf_put8_chk(n, NRF_RF_CH, c.channel);

    // setup data rate and power
    nrf_put8_chk(n, NRF_RF_SETUP, 0b1110);

    // setup NRF status
    nrf_put8(n, NRF_STATUS, ~0);

    // 5. You should flush the RX (`nrf_rx_flush()`) and TX fifos (`nrf_tx_flush()`). 
    nrf_rx_flush(n);
    nrf_tx_flush(n);

    assert(!nrf_tx_fifo_full(n));
    assert(nrf_tx_fifo_empty(n));
    assert(!nrf_rx_fifo_full(n));
    assert(nrf_rx_fifo_empty(n));
    assert(!nrf_has_rx_intr(n));
    assert(!nrf_has_tx_intr(n));
    assert(pipeid_empty(nrf_rx_get_pipeid(n)));
    assert(!nrf_rx_has_packet(n));
    // should be true after setup.

    // reg=0x1c dynamic payload (next register --- don't set the others!)
    assert(nrf_get8(n, NRF_DYNPD) == 0);

    // reg 0x1d: feature register. we don't use it yet.
    nrf_put8_chk(n, NRF_FEATURE, 0);

    // now go to RX mode: invariant = we are always in RX except for the
    // small amount of time we need to switch to TX to send a message.
    nrf_put8_chk(n, NRF_CONFIG, rx_config);
    delay_us(140);

    // set to RX mode
    gpio_set_on(c.ce_pin);
    delay_us(140);
 
    if(acked_p) {
        nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
        nrf_opt_assert(n, nrf_pipe_is_enabled(n, 0));
        nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
        nrf_opt_assert(n, nrf_pipe_is_acked(n, 0));
        nrf_opt_assert(n, nrf_pipe_is_acked(n, 1));
        nrf_opt_assert(n, nrf_tx_fifo_empty(n));
    } else {
        nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
        nrf_opt_assert(n, !nrf_pipe_is_enabled(n, 0));
        nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
        nrf_opt_assert(n, !nrf_pipe_is_acked(n, 1));
        nrf_opt_assert(n, nrf_tx_fifo_empty(n));
    }
    return n;
}

int nrf_tx_send_ack(nrf_t *n, uint32_t txaddr, 
    const void *msg, unsigned nbytes) {

    // default config for acked state.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
    nrf_opt_assert(n, nrf_pipe_is_acked(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_acked(n, 1));
    nrf_opt_assert(n, nrf_tx_fifo_empty(n));

    // if interrupts not enabled: make sure we check for packets.
    // while(staff_nrf_get_pkts(n))
    //     ;

    // TODO: you would implement the rest of the code at this point.
    // int res = staff_nrf_tx_send_ack(n, txaddr, msg, nbytes);

    int res = 0;
    gpio_set_off(n->config.ce_pin);
    delay_us(140);

    nrf_set_addr(n, NRF_TX_ADDR, txaddr, nrf_default_addr_nbytes);
    nrf_put8_chk(n, NRF_RX_PW_P0, n->config.nbytes);
    nrf_set_addr(n, NRF_RX_ADDR_P0, txaddr, nrf_default_addr_nbytes);
    nrf_put8_chk(n, NRF_CONFIG, tx_config);

    nrf_putn(n, NRF_W_TX_PAYLOAD, msg, nbytes);
    gpio_set_on(n->config.ce_pin);
    delay_us(140);

    //5
    while(!nrf_tx_fifo_empty(n)) {

    }



    if (nrf_has_tx_intr(n)) {
//        output("TX SUCCESS!\n");
        res = n->config.nbytes;
        nrf_tx_intr_clr(n);
    }
    if (nrf_has_max_rt_intr(n)) {
//        output("TX EXCEEDED MAX!\n");
        res = 0;
        nrf_rt_intr_clr(n);
    }

    //7 set back to RX mode
    gpio_set_off(n->config.ce_pin);
    delay_us(140);
    nrf_put8_chk(n, NRF_CONFIG, rx_config);
    nrf_put8_chk(n, NRF_RX_PW_P0, 0x0);
    nrf_set_addr(n, NRF_RX_ADDR_P0, 0x0, nrf_default_addr_nbytes);
    gpio_set_on(n->config.ce_pin);
    delay_us(140);

//    int res = staff_nrf_tx_send_ack(n, txaddr, msg, nbytes);

     uint8_t cnt = nrf_get8(n, NRF_OBSERVE_TX);
     n->tot_retrans  += bits_get(cnt,0,3);

    // uint8_t cnt = nrf_get8(n, NRF_OBSERVE_TX);
    // n->tot_retrans  += bits_get(cnt,0,3);

    // tx interrupt better be cleared.
    nrf_opt_assert(n, !nrf_has_tx_intr(n));
    // better be back in rx mode.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return res;
}

int nrf_tx_send_noack(nrf_t *n, uint32_t txaddr, 
    const void *msg, unsigned nbytes) {

    // default state for no-ack config.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    nrf_opt_assert(n, !nrf_pipe_is_enabled(n, 0));
    nrf_opt_assert(n, nrf_pipe_is_enabled(n, 1));
    nrf_opt_assert(n, !nrf_pipe_is_acked(n, 1));
    nrf_opt_assert(n, nrf_tx_fifo_empty(n));

    // if interrupts not enabled: make sure we check for packets.
    // while(staff_nrf_get_pkts(n))
    //     ;


    // TODO: you would implement the code here.
    // int res = staff_nrf_tx_send_noack(n, txaddr, msg, nbytes);
    //1. Set the device to TX mode.
    gpio_set_off(n->config.ce_pin);
    delay_us(140);

    nrf_put8_chk(n, NRF_CONFIG, tx_config);

    // 2. Set the TX address.
    nrf_set_addr(n, NRF_TX_ADDR, txaddr, nrf_default_addr_nbytes);

    nrf_putn(n, NRF_W_TX_PAYLOAD, msg, nbytes);
    gpio_set_on(n->config.ce_pin);
    delay_us(140);

    //5
    while(!nrf_tx_fifo_empty(n))
        ;

    //6
    nrf_tx_intr_clr(n);

    //7 set back to RX mode
    gpio_set_off(n->config.ce_pin);
    delay_us(140);
    nrf_put8_chk(n, NRF_CONFIG, rx_config);
    gpio_set_on(n->config.ce_pin);
    delay_us(140);

    int res = 1;

    // tx interrupt better be cleared.
    nrf_opt_assert(n, !nrf_has_tx_intr(n));
    // better be back in rx mode.
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return res;
}

int nrf_get_pkts(nrf_t *n) {
    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);

    // TODO:
    // data sheet gives the sequence to follow to get packets.
    // p63: 
    //    1. read packet through spi.
    //    2. clear IRQ.
    //    3. read fifo status to see if more packets: 
    //       if so, repeat from (1) --- we need to do this now in case
    //       a packet arrives b/n (1) and (2)
    // done when: nrf_rx_fifo_empty(n)
    // int res = staff_nrf_get_pkts(n);
    int res = 0;

    while (!nrf_rx_fifo_empty(n)) {
        unsigned pipen = nrf_rx_get_pipeid(n);
        if (pipen == NRF_PIPEID_EMPTY) panic("impossible: empty pipeid: %b\n", pipen);

        uint8_t msg[NRF_PKT_MAX];
        uint8_t pkt = nrf_getn(n, NRF_R_RX_PAYLOAD, msg, n->config.nbytes);

        if(!cq_push_n(&n->recvq, msg, n->config.nbytes))
            panic("not enough space left for message on pipe=%d\n", pipen);

        nrf_rx_intr_clr(n);
        res++;
    }

    nrf_opt_assert(n, nrf_get8(n, NRF_CONFIG) == rx_config);
    return res;
}
