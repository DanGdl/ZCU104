## XDC file for GPIO pin assignments

# Define the pin assignments and voltage standards for GPIO pin 0 (GPIO_PS)
set_property PACKAGE_PIN H8       [get_ports "spi_rtl_io0_io"] 
set_property IOSTANDARD  LVCMOS33 [get_ports "spi_rtl_io0_io"] 

# Define the pin assignments and voltage standards for GPIO pin 1 (GPIO_PS)
set_property PACKAGE_PIN G7       [get_ports "spi_rtl_io1_io"] 
set_property IOSTANDARD  LVCMOS33 [get_ports "spi_rtl_io1_io"] 

# Define the pin assignments and voltage standards for GPIO pin 2 (GPIO_PS)
set_property PACKAGE_PIN H7       [get_ports "spi_rtl_sck_io"] 
set_property IOSTANDARD  LVCMOS33 [get_ports "spi_rtl_sck_io"] 

# Define the pin assignments and voltage standards for GPIO pin 3 (GPIO_PS)
set_property PACKAGE_PIN G6       [get_ports "spi_rtl_ss_io"] 
set_property IOSTANDARD  LVCMOS33 [get_ports "spi_rtl_ss_io"] 

# Define the pin assignments and voltage standards for GPIO pin 4 (GPIO_PS)
#set_property PACKAGE_PIN H6       [get_ports "gpio_ps_tri_io[4]"]
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_ps_tri_io[4]"] 

## Define the pin assignments and voltage standards for GPIO pin 5 (GPIO_PS)
#set_property PACKAGE_PIN J6       [get_ports "gpio_ps_tri_io[5]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_ps_tri_io[5]"] 

## Define the pin assignments and voltage standards for GPIO pin 6 (GPIO_PS)
#set_property PACKAGE_PIN J7       [get_ports "gpio_ps_tri_io[6]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_ps_tri_io[6]"] 

## Define the pin assignments and voltage standards for GPIO pin 7 (GPIO_PS)
#set_property PACKAGE_PIN J9       [get_ports "gpio_ps_tri_io[7]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_ps_tri_io[7]"] 

## Comments:
## - This XDC file defines pin assignments and voltage standards for GPIO_PS pins.
## - The PACKAGE_PIN property specifies the physical pin on the FPGA package to which the GPIO_PS signal is mapped.
## - The IOSTANDARD property specifies the voltage standard for the GPIO_PS signal (LVCMOS33 in this case).
## - The [x] notation in get_ports "gpio_ps_tri_io[x]" indicates the index of the GPIO_PS pin being defined.


## Define the pin assignments and voltage standards for GPIO pin 0 (GPIO_PL)
#set_property PACKAGE_PIN K9       [get_ports "gpio_pl_tri_i[0]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[0]"] 

## Define the pin assignments and voltage standards for GPIO pin 1 (GPIO_PL)
#set_property PACKAGE_PIN K8       [get_ports "gpio_pl_tri_i[1]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[1]"]

## Define the pin assignments and voltage standards for GPIO pin 2 (GPIO_PL)
#set_property PACKAGE_PIN L8       [get_ports "gpio_pl_tri_i[2]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[2]"]

## Define the pin assignments and voltage standards for GPIO pin 3 (GPIO_PL)
#set_property PACKAGE_PIN L10      [get_ports "gpio_pl_tri_i[3]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[3]"]

## Define the pin assignments and voltage standards for GPIO pin 4 (GPIO_PL)
#set_property PACKAGE_PIN M10      [get_ports "gpio_pl_tri_i[4]"]
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[4]"]

## Define the pin assignments and voltage standards for GPIO pin 5 (GPIO_PL)
#set_property PACKAGE_PIN M8       [get_ports "gpio_pl_tri_i[5]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[5]"]

## Define the pin assignments and voltage standards for GPIO pin 6 (GPIO_PL)
#set_property PACKAGE_PIN M9       [get_ports "gpio_pl_tri_i[6]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "gpio_pl_tri_i[6]"]

## Comments:
## - This XDC file defines pin assignments and voltage standards for GPIO_PL pins.
## - The PACKAGE_PIN property specifies the physical pin on the FPGA package to which the GPIO_PL signal is mapped.
## - The IOSTANDARD property specifies the voltage standard for the GPIO_PL signal (LVCMOS33 in this case).
## - The [x] notation in get_ports "gpio_pl_tri_i[x]" indicates the index of the GPIO_PL pin being defined.

## LED output, bit 0
#set_property PACKAGE_PIN D5       [get_ports "led_4bits_tri_o[0]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "led_4bits_tri_o[0]"] 

## LED output, bit 1
#set_property PACKAGE_PIN D6       [get_ports "led_4bits_tri_o[1]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "led_4bits_tri_o[1]"] 

## LED output, bit 2
#set_property PACKAGE_PIN A5       [get_ports "led_4bits_tri_o[2]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "led_4bits_tri_o[2]"] 

## LED output, bit 3
#set_property PACKAGE_PIN B5       [get_ports "led_4bits_tri_o[3]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "led_4bits_tri_o[3]"] 

## DIP switch input, bit 3
#set_property PACKAGE_PIN F4       [get_ports "dip_switch_4bits_tri_i[3]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "dip_switch_4bits_tri_i[3]"] 

## DIP switch input, bit 2
#set_property PACKAGE_PIN F5       [get_ports "dip_switch_4bits_tri_i[2]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "dip_switch_4bits_tri_i[2]"] 

## DIP switch input, bit 1
#set_property PACKAGE_PIN D4       [get_ports "dip_switch_4bits_tri_i[1]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "dip_switch_4bits_tri_i[1]"] 

## DIP switch input, bit 0
#set_property PACKAGE_PIN E4       [get_ports "dip_switch_4bits_tri_i[0]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "dip_switch_4bits_tri_i[0]"] 

## Push button input, bit 0
#set_property PACKAGE_PIN B4       [get_ports "push_button_4bits_tri_i[0]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "push_button_4bits_tri_i[0]"]

## Push button input, bit 1
#set_property PACKAGE_PIN C4       [get_ports "push_button_4bits_tri_i[1]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "push_button_4bits_tri_i[1]"] 

## Push button input, bit 2
#set_property PACKAGE_PIN B3       [get_ports "push_button_4bits_tri_i[2]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "push_button_4bits_tri_i[2]"] 

## Push button input, bit 3
#set_property PACKAGE_PIN C3       [get_ports "push_button_4bits_tri_i[3]"] 
#set_property IOSTANDARD  LVCMOS33 [get_ports "push_button_4bits_tri_i[3]"] 
