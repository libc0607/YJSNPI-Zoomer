// ======================================
//  YJSNPI-Zoomer 
//  STM32F0 + MS41929 
//  dual stepper motor driver + ircut
//  https://github.com/libc0607/YJSNPI-Zoomer
// ======================================

#include <SPI.h>

// ==== Pin definition ==================
#define PIN_PWMIN_1         PA2   // zoom
#define PIN_PWMIN_2         PA1   // focus
#define PIN_PWMIN_3         PA0   // ircut
#define PIN_MS41929_RSTB    PA3   // 
#define PIN_MS41929_CS      PA4   //
#define PIN_MS41929_SCK     PA5   //
#define PIN_MS41929_SOUT    PA6   //
#define PIN_MS41929_SIN     PA7   //
#define PIN_MS41929_VDFZ    PB1   //
#define PIN_KEY_1           PA13  // unused
#define PIN_KEY_2           PA14  // unused
#define PIN_UART_TX         PA9   // as gpio
#define PIN_UART_RX         PA10  // as gpio
// ======================================

// PWM1: Motor AB: 750us~1200us CW, 1250us~1750us Brake, 1800us~2250us CCW
// PWM2: Motor CD: 750us~1200us CW, 1250us~1750us Brake, 1800us~2250us CCW
// PWM3: 750us~1150us  IRCUT Off + LED Off
//       1350us~1650us IRCUT On  + LED Off
//       1850us~2250us IRCUT On  + LED On
// 20ms period:
// ----DT1----DT2----rotate----
// ----1ms----
// 800pps, VD=50hz, OSC=27MHz
// PSUMxx=63=0x3f, INTCTxx=357=0x165

unsigned long pwm_ts[3] = {0};
unsigned long pwm_val_us[3] = {1500, 1500, 1000};
int ircut_working = false;  // 0-idle, 12345..-working
int timer_flag = false;
int ircut_status = 1; // 0-off, 1-on

void pwm1_isr() {
  if (digitalRead(PIN_PWMIN_1) == HIGH) {
    pwm_ts[0] = micros();
  } else {
    pwm_val_us[0] = micros() - pwm_ts[0];
  }
}

void pwm2_isr() {
  if (digitalRead(PIN_PWMIN_2) == HIGH) {
    pwm_ts[1] = micros();
  } else {
    pwm_val_us[1] = micros() - pwm_ts[1];
  }
}

void pwm3_isr() {
  if (digitalRead(PIN_PWMIN_3) == HIGH) {
    pwm_ts[2] = micros();
  } else {
    pwm_val_us[2] = micros() - pwm_ts[2];
  }
}

uint16_t ms41929_read(uint8_t addr) {
  uint16_t dat = 0;
  digitalWrite(PIN_MS41929_CS, HIGH);
  SPI.transfer(addr|0xC0);
  dat = SPI.transfer16(0xFFFF);
  digitalWrite(PIN_MS41929_CS, LOW);
  return dat;
}

void ms41929_write(uint8_t addr, uint16_t dat) {
  digitalWrite(PIN_MS41929_CS, HIGH);
  SPI.transfer(addr & 0x3F);
  SPI.transfer16(dat);
  digitalWrite(PIN_MS41929_CS, LOW);
  return;
}

void ms41929_vdfz() {
  digitalWrite(PIN_MS41929_VDFZ, HIGH);
  delayMicroseconds(100);
  digitalWrite(PIN_MS41929_VDFZ, LOW);
}

// channel: 1-A-Pin20 / 2-B-Pin21
// value: HIGH / LOW (pin status)
void ms41929_led_change(int channel, int value) {
  uint16_t dat;

  if (value == LOW) {
    if (channel == 1) {
      dat = ms41929_read(0x29);
      ms41929_write(0x29, dat|0x0800);
    } else if (channel == 2) {
      dat = ms41929_read(0x24);
      ms41929_write(0x24, dat|0x0800);
    }
  } else if (value == HIGH) {
    if (channel == 1) {
      dat = ms41929_read(0x29);
      ms41929_write(0x29, dat&0xF7FF);
    } else if (channel == 2) {
      dat = ms41929_read(0x24);
      ms41929_write(0x24, dat&0xF7FF);
    }
  }
}

void led_update() {
  if (pwm_val_us[2] < 1750 && pwm_val_us[2] > 750) {
      ms41929_led_change(1, HIGH);
      ms41929_led_change(2, HIGH);
    } else if (pwm_val_us[2] > 1750 && pwm_val_us[2] < 2250) {
      ms41929_led_change(1, LOW);
      ms41929_led_change(2, LOW);
    }
}

// state: 0-finish, 1-a, 2-b
void ms41929_ircut_change(int state) {
  switch (state) {
  case 0:
    ms41929_write(0x2C, 0x0007);
    break;
  case 1:
    ms41929_write(0x2C, 0x0005);
    break;
  case 2:
    ms41929_write(0x2C, 0x0006);
    break;
  default:
    break; 
  }
}

void ircut_update() {
  if (ircut_working) {
    ircut_working++;
    if (ircut_working > 5) {
      ms41929_ircut_change(0);
      ircut_working = false;
    }
  } else {
    if (pwm_val_us[2] < 1150 && pwm_val_us[2] > 750) {
      if (ircut_status == 0) {
        // have not changed, ignore
      } else {
        // changed
        ms41929_ircut_change(1);
        ircut_working = true;
        ircut_status = 0;
      }
    } else if (pwm_val_us[2] > 1350 && pwm_val_us[2] < 2250) {
      if (ircut_status == 1) {
        // have not changed, ignore
      } else {
        ms41929_ircut_change(2);
        ircut_working = true;
        ircut_status = 1;
      }
      
    }
  }
}

void ms41929_init() {

  SPI.begin();
  SPI.beginTransaction(SPISettings(5000000L, LSBFIRST, SPI_MODE3));
  // ms41929 pins
  pinMode(PIN_MS41929_CS, OUTPUT);
  pinMode(PIN_MS41929_VDFZ, OUTPUT);
  pinMode(PIN_MS41929_RSTB, OUTPUT);
  digitalWrite(PIN_MS41929_CS, LOW);

  // reset ms41929
  digitalWrite(PIN_MS41929_RSTB, LOW);
  delay(10);
  digitalWrite(PIN_MS41929_RSTB, HIGH);
  delay(10);
  
  // register init
  ms41929_write(0x0B, 0x0000);  // 
  ms41929_write(0x20, (1<<12) | (8<<7) | 2); 
                    // PWMRES, PWMMODE, DT1
                    //  ~105.5kHz, ~0.3ms
  ms41929_write(0x22, 0x0001);  // DT2A ~0.3ms
  ms41929_write(0x27, 0x0001);  // DT2B ~0.3ms
  ms41929_write(0x23, (0x40<<8) | 0x40);  // PPWA/PPWB
  ms41929_write(0x28, (0x40<<8) | 0x40);  // PPWC/PPWD
  ms41929_write(0x24, (0<<12) | (0 << 11) | (1<<10) | (0<<9) | (0<<8) | (0) );  
                    // MICROAB,   LEDA,     ENDISAB, BRAKEAB, CCWCWAB, PSUMAB
  ms41929_write(0x29, (0<<12) | (0 << 11) | (1<<10) | (0<<9) | (0<<8) | (0) );  
                    // MICROCD,   LEDB,     ENDISCD, BRAKECD, CCWCWCD, PSUMCD
}

void motor_update() {
  ms41929_vdfz();
  // AB
  if (pwm_val_us[0] > 750 && pwm_val_us[0] < 1200) {
    ms41929_write(0x24, (0<<12) | (1<<10) | (0<<9) | (0<<8) | 0x3f );  
                      // MICROAB, ENDISAB, BRAKEAB, CCWCWAB, PSUMAB
  } else if (pwm_val_us[0] > 1250 && pwm_val_us[0] < 1750) {
    ms41929_write(0x24, (0<<12) | (1<<10) | (1<<9) | (0<<8) | 0x0 );  
                      // MICROAB, ENDISAB, BRAKEAB, CCWCWAB, PSUMAB
  } else if (pwm_val_us[0] > 1800 && pwm_val_us[0] < 2250) {
    ms41929_write(0x24, (0<<12) | (1<<10) | (0<<9) | (1<<8) | 0x3f );  
                      // MICROAB, ENDISAB, BRAKEAB, CCWCWAB, PSUMAB
  }

  // CD
  if (pwm_val_us[1] > 750 && pwm_val_us[1] < 1200) {
    ms41929_write(0x29, (0x0<<12) | (0x1<<10) | (0x0<<9) | (0x0<<8) | 0x3f );  
                      // MICROCD, ENDISCD, BRAKECD, CCWCWCD, PSUMCD
  } else if (pwm_val_us[1] > 1250 && pwm_val_us[1] < 1750) {
    ms41929_write(0x29, (0x0<<12) | (0x1<<10) | (0x1<<9) | (0x0<<8) | 0x0 );  
                      // MICROCD, ENDISCD, BRAKECD, CCWCWCD, PSUMCD
  } else if (pwm_val_us[1] > 1800 && pwm_val_us[1] < 2250) {
    ms41929_write(0x29, (0x0<<12) | (0x1<<10) | (0x0<<9) | (0x1<<8) | 0x3f );  
                      // MICROCD, ENDISCD, BRAKECD, CCWCWCD, PSUMCD
  }
  
  ms41929_write(0x25, 0x165); // INTCTAB
  ms41929_write(0x2A, 0x165); // INTCTCD
  
}

// stm32f030f4p6’s flash size is tooooo tm small
// for debug, pls use a logic analyzer 
void debug() {
  uint16_t dat;
  digitalWrite(PIN_UART_RX, LOW);
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)((pwm_val_us[0]>>8)&0xff));
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)(pwm_val_us[0]&0xff));
  delayMicroseconds(5);
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)((pwm_val_us[1]>>8)&0xff));
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)(pwm_val_us[1]&0xff));
  delayMicroseconds(5);
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)((pwm_val_us[2]>>8)&0xff));
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)(pwm_val_us[2]&0xff));
  delayMicroseconds(5);
  dat=ms41929_read(0x29);
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)((dat>>8)&0xff));
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)(dat&0xff));
  delayMicroseconds(5);
  dat=ms41929_read(0x24);
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)((dat>>8)&0xff));
  shiftOut(PIN_UART_TX, PIN_UART_RX, MSBFIRST, (uint8_t)(dat&0xff));
  delayMicroseconds(5);
}

void timer_isr_cb(void) {
  timer_flag = true;
}

void startup_blink() {
  // blink 
  ms41929_led_change(1, LOW);
  ms41929_led_change(2, LOW);
  ms41929_ircut_change(1);
  delay(100);
  ms41929_led_change(1, HIGH);
  ms41929_led_change(2, HIGH);
  ms41929_ircut_change(0);
  delay(100);
  ms41929_led_change(1, LOW);
  ms41929_led_change(2, LOW);
  ms41929_ircut_change(2);
  ircut_status = 1;
  delay(100);
  ms41929_led_change(1, HIGH);
  ms41929_led_change(2, HIGH);
  ms41929_ircut_change(0);
}

void setup() {

  // PWM input interrupt
  pinMode(PIN_PWMIN_1, INPUT);
  pinMode(PIN_PWMIN_2, INPUT);
  pinMode(PIN_PWMIN_3, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_PWMIN_1), pwm1_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_PWMIN_2), pwm2_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_PWMIN_3), pwm3_isr, CHANGE);

  // debug pin
  pinMode(PIN_UART_TX, OUTPUT);
  pinMode(PIN_UART_RX, OUTPUT);
  
  ms41929_init();
  startup_blink();

  HardwareTimer *main_timer = new HardwareTimer(TIM14);
  main_timer->setOverflow(50, HERTZ_FORMAT);
  main_timer->attachInterrupt(timer_isr_cb);
  main_timer->resume();
}

void loop() {
  if (timer_flag) {
    timer_flag = false;
    motor_update(); // note: call motor_update() before led_update()
    ircut_update();
    led_update();
    //debug();
  }
}
