#pragma once


#define BTN_EVENT_NOP  0
#define BTN_EVENT_SHORTPRESS  (1<<1)
#define BTN_EVENT_LONGPRESS  (1<<2)
#define BTN_EVENT_FASTPRESS  (1<<3)
#define BTN_EVENT_TIMEOUT (1<<4)

class Button {
  protected:
    gpio_num_t m_pin;
    int64_t m_longpress;
    int m_state;
    int64_t m_timeout_tmr;
    int64_t m_timeout;
    int64_t m_button_tmr;
    int64_t m_last_run;
    int64_t m_lastpress;
    uint32_t m_event;
    int m_repctr;
  public:
    Button(gpio_num_t pin, int64_t longpress);

    void startTimeout(uint32_t ms);
    void setLongPress(int64_t lp);
    void update();
    uint32_t getEvent();
    void clearEvent();
};
