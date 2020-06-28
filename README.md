# YJSNPI-Zoomer

### 这是啥  
基于 STM32F0 + MS41929 的监控用变焦镜头控制器  
硬件设计在 [LCEDA 的这里](https://oshwhub.com/libc0607/step-motors-controller-for-ipc)  
这个东西属于 [YJSNPI-Broadcast](https://github.com/libc0607/YJSNPI-Broadcast) 巨坑的一部分

### 如何编译  
以下在 Arduino 1.8.12 + STM32duino 1.9.0 下验证通过  
1. 安装 Arduino 和 STM32CubeProgrammer
2. 按照 [Github: stm32duino/Arduino_Core_STM32](https://github.com/stm32duino/Arduino_Core_STM32) 安装 stm32duino
3. 下载本仓库源码并打开  
4. 配置开发板：选择 "Generic STM32F0 Series", "STM32F030F4 Demo board", "Disabled (no Serial support)"
5. SWD 连接到板上
6. 下载即可

### 如何食用
将接收机输出的 PWM 信号接到板上的三个 PWM 输入（需要带5V供电的），三个输入分别控制 Focus电机 / Zoom 电机 / IR-Cut + LED  
