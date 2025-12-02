#include "driver/gpio.h"
#include "esp_timer.h"

#include "button.h"

Button::Button(gpio_num_t pin, int64_t longpress) :
    m_pin(pin), m_longpress(longpress), m_state(0), m_timeout_tmr(0),
    m_timeout(0), m_button_tmr(0), m_last_run(0), m_lastpress(0), m_event(BTN_EVENT_NOP), m_repctr(0) {

}

void Button::startTimeout(uint32_t ms) {
    m_timeout_tmr = esp_timer_get_time() / 1000ll;
    m_timeout = ms;
}

void Button::setLongPress(int64_t lp) {
    m_longpress = lp;
}

void Button::update() {
    int64_t ticks = esp_timer_get_time() / 1000ll;

    m_last_run = ticks;

    // recycled 8 years old state-machine^^
    switch (m_state) {
    case 0:
        if (gpio_get_level(m_pin)) {
            m_state = 1;
        }
        break;
    case 1:
        m_event &= ~BTN_EVENT_PRESSED;
        if (!gpio_get_level(m_pin)) {
            m_state = 2;
            m_button_tmr = ticks;
        }
        if (m_timeout_tmr && m_timeout && ticks > m_timeout_tmr + m_timeout) {
            m_event |= BTN_EVENT_TIMEOUT;
            m_timeout_tmr = 0;
            m_timeout = 0;
            m_state = 0;
        }

        break;
    case 2: // button pressed, measure release time
        m_event |= BTN_EVENT_PRESSED;
        if (m_button_tmr && ticks - m_button_tmr > m_longpress) {
            m_event |= BTN_EVENT_LONGPRESS;
            m_state = 0;
            m_button_tmr = 0;
        } else if (gpio_get_level(m_pin)) {
            m_event |= BTN_EVENT_SHORTPRESS;

            if (ticks < m_lastpress + 1000) {
                m_repctr++;
                if (m_repctr >= 5) {
                    m_lastpress = 0;
                    m_repctr = 0;
                    m_event |= BTN_EVENT_FASTPRESS;
                }
            } else {
                m_repctr = 0;
            }
            m_lastpress = ticks;

            m_state = 1;
        }
        break;
    }
}

uint32_t Button::getEvent() {
    return m_event;
}

void Button::clearEvent() {
    m_event = BTN_EVENT_NOP;
}
