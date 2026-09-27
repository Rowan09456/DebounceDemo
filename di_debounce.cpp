#include "di_debounce.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

namespace {

enum State { IDLE, TOLERANT, INTOLERANT };

uint              s_pin;
volatile State    s_state = IDLE;
volatile bool     s_candidate_level = false;
volatile bool     s_debounced_level = false;
volatile uint64_t s_first_edge_us = 0;
volatile uint64_t s_phase_start_us = 0;
volatile bool     s_event_pending = false;
repeating_timer_t s_timer;

void enter_tolerant(uint64_t now) {
    s_state = TOLERANT;
    s_phase_start_us = now;
    // Chatter in this window is expected and free: mask the interrupt.
    gpio_set_irq_enabled(s_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
}

void enter_intolerant(uint64_t now) {
    s_state = INTOLERANT;
    s_phase_start_us = now;
    // Clear any edge latched while masked, then re-arm: from here on, any
    // edge is a genuine "not settled yet" violation we want to catch.
    gpio_acknowledge_irq(s_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    gpio_set_irq_enabled(s_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

void accept() {
    s_debounced_level = s_candidate_level;
    s_state = IDLE;
    s_event_pending = true;
    gpio_acknowledge_irq(s_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    gpio_set_irq_enabled(s_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

// Fires on every raw edge (except while TOLERANT, when the interrupt is
// masked at the hardware level).
void gpio_isr(uint gpio, uint32_t /*events*/) {
    if (gpio != s_pin) return;
    uint64_t now = time_us_64();

    if (s_state == IDLE) {
        // The very first raw transition of a new sequence -- latch it now.
        s_first_edge_us = now;
        s_candidate_level = gpio_get(s_pin);
        enter_tolerant(now);
    } else if (s_state == INTOLERANT) {
        // Edge during the "must stay quiet" window: still bouncing.
        // Restart the tolerant window; s_first_edge_us is left untouched.
        s_candidate_level = gpio_get(s_pin);
        enter_tolerant(now);
    }
}

// Services the two window timeouts; doesn't touch the pin's raw level.
bool timer_isr(repeating_timer_t*) {
    uint64_t now = time_us_64();
    if (s_state == TOLERANT && now - s_phase_start_us >= DI_TOLERANT_US) {
        enter_intolerant(now);
    } else if (s_state == INTOLERANT && now - s_phase_start_us >= DI_INTOLERANT_US) {
        accept();
    }
    return true; // keep repeating
}

} // namespace

void di_init(uint pin, bool pull_up) {
    s_pin = pin;
    gpio_init(s_pin);
    gpio_set_dir(s_pin, GPIO_IN);
    pull_up ? gpio_pull_up(s_pin) : gpio_pull_down(s_pin);
    sleep_us(5); // let the pull settle before the first read

    s_debounced_level = gpio_get(s_pin);
    s_candidate_level = s_debounced_level;
    s_state = IDLE;

    gpio_set_irq_enabled_with_callback(s_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                        true, &gpio_isr);
    add_repeating_timer_us(-250, &timer_isr, nullptr, &s_timer); // 250 us servicing tick
}

bool di_event_ready() {
    uint32_t ints = save_and_disable_interrupts();
    bool ready = s_event_pending;
    s_event_pending = false;
    restore_interrupts(ints);
    return ready;
}

bool di_level() {
    uint32_t ints = save_and_disable_interrupts();
    bool lvl = s_debounced_level;
    restore_interrupts(ints);
    return lvl;
}

uint64_t di_first_edge_us() {
    uint32_t ints = save_and_disable_interrupts();
    uint64_t t = s_first_edge_us;
    restore_interrupts(ints);
    return t;
}
